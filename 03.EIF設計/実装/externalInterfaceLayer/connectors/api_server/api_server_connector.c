/**
 * @file api_server_connector.c
 * @brief APIサーバー用Connector（詳細設計書 6.6、7.9）。
 *
 * APIサーバーの型 ⇔ EIF公開モデルの詰め替えと、完了・イベントのWebSocket送信だけを行う。
 * PCL・REFの型、機種のことは知らない。APIサーバーのヘッダーを参照してよいのはこのフォルダだけ。
 *
 * 本ファイルは初版として、ネットワーク設定（GET/PATCH）、ジョブ作成、異常の発生／解消通知を実装する。
 * 他のRESTカテゴリは、現行の rest 配下の各ファイルの処理を同じ形で移設する。
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "eif_api.h"
#include "eif_connector_api_server.h"
#include "server_api.h"

/** @brief PATCHがPCL経由（EIF_ACCEPTED）になった場合に完了を待つ上限[秒]。 */
#define CONNECTOR_PATCH_WAIT_SEC (5)
/** @brief 同時に待てるPATCHの数。 */
#define CONNECTOR_PATCH_WAITERS (4U)
/** @brief PATCHの待ち合わせに使う要求元トークンの印。 */
#define CONNECTOR_TOKEN_PATCH_FLAG (UINT64_C(1) << 63)

static EIF_ConnectorId s_id = 0U;
static atomic_uint_fast64_t s_next_token = 1U;

/* ---------------------------------------------------------------- 対応表（接続先依存） */

/** @brief APIサーバーのジョブ種別 ⇔ 公開要求ID。どの機種でも同じ。公開範囲は機種ごとの対応表で決まる。 */
static const struct {
    job_type_t job_type;
    eif_request_id_t request_id;
} k_jobs[] = {
    { JOB_TYPE_PRINT,                  EIF_REQ_PRINT },
    { JOB_TYPE_TEST_PRINT,             EIF_REQ_TEST_PRINT },
    { JOB_TYPE_HEAD_CLEANING,          EIF_REQ_HEAD_CLEANING },
    { JOB_TYPE_WIPER_MAINTENANCE,      EIF_REQ_WIPER_MAINTENANCE },
    { JOB_TYPE_HUMIDIFIER_MAINTENANCE, EIF_REQ_HUMIDIFIER_MAINTENANCE },
    { JOB_TYPE_INK_INTRODUCTION,       EIF_REQ_INK_INTRODUCTION },
    { JOB_TYPE_TUBE_AIR_REMOVAL,       EIF_REQ_TUBE_AIR_REMOVAL },
    { JOB_TYPE_CAP_PUMP_CHECK,         EIF_REQ_CAP_PUMP_CHECK },
};

static bool job_to_request(job_type_t job, eif_request_id_t *out)
{
    for (size_t i = 0U; i < sizeof(k_jobs) / sizeof(k_jobs[0]); i++) {
        if (k_jobs[i].job_type == job) {
            *out = k_jobs[i].request_id;
            return true;
        }
    }
    return false;
}

static bool request_to_job(eif_request_id_t req, job_type_t *out)
{
    for (size_t i = 0U; i < sizeof(k_jobs) / sizeof(k_jobs[0]); i++) {
        if (k_jobs[i].request_id == req) {
            *out = k_jobs[i].job_type;
            return true;
        }
    }
    return false;
}

/** @brief EIF_Result → uc_result_t（詳細設計書 9.4）。 */
static uc_result_t to_uc(EIF_Result r)
{
    switch (r) {
    case EIF_OK:
    case EIF_ACCEPTED:
        return UC_OK;
    case EIF_ERR_INVALID_PARAM:
        return UC_ERR_INVALID_PARAM;
    case EIF_ERR_NOT_EXPOSED:
        return UC_ERR_NOT_IMPLEMENTED; /* QA-09 */
    case EIF_ERR_NOT_FOUND:
        return UC_ERR_NOT_FOUND;
    case EIF_ERR_BUSY:
        return UC_ERR_BUSY;
    case EIF_ERR_UNAVAILABLE:
        return UC_ERR_SERVER_UNAVAILABLE;
    default:
        return UC_ERR_INTERNAL;
    }
}

/* ---------------------------------------------------------------- PATCHの待ち合わせ（PCL経由になった場合） */

typedef struct {
    bool used;
    bool done;
    uint64_t token;
    EIF_RequestOutcome outcome;
} waiter_t;

static waiter_t s_waiters[CONNECTOR_PATCH_WAITERS];
static pthread_mutex_t s_wait_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_wait_cond = PTHREAD_COND_INITIALIZER;

static waiter_t *waiter_acquire(uint64_t token)
{
    waiter_t *w = NULL;
    pthread_mutex_lock(&s_wait_lock);
    for (size_t i = 0U; i < CONNECTOR_PATCH_WAITERS; i++) {
        if (!s_waiters[i].used) {
            w = &s_waiters[i];
            w->used = true;
            w->done = false;
            w->token = token;
            break;
        }
    }
    pthread_mutex_unlock(&s_wait_lock);
    return w;
}

static uc_result_t waiter_wait_and_release(waiter_t *w)
{
    struct timespec until;
    uc_result_t r = UC_ERR_TIMEOUT;

    (void)clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += CONNECTOR_PATCH_WAIT_SEC;
    pthread_mutex_lock(&s_wait_lock);
    while (!w->done) {
        if (pthread_cond_timedwait(&s_wait_cond, &s_wait_lock, &until) != 0) {
            break;
        }
    }
    if (w->done) {
        r = (w->outcome == EIF_OUTCOME_COMPLETED) ? UC_OK : UC_ERR_INTERNAL;
    }
    w->used = false;
    pthread_mutex_unlock(&s_wait_lock);
    return r;
}

static bool waiter_signal(uint64_t token, EIF_RequestOutcome outcome)
{
    bool found = false;
    pthread_mutex_lock(&s_wait_lock);
    for (size_t i = 0U; i < CONNECTOR_PATCH_WAITERS; i++) {
        if (s_waiters[i].used && (s_waiters[i].token == token)) {
            s_waiters[i].done = true;
            s_waiters[i].outcome = outcome;
            found = true;
            break;
        }
    }
    pthread_cond_broadcast(&s_wait_cond);
    pthread_mutex_unlock(&s_wait_lock);
    return found;
}

/* ---------------------------------------------------------------- REST callback（APIサーバーのblockingスレッド） */

static uc_result_t rest_get_network(network_settings_t *out)
{
    eif_network_settings_t m;

    if (out == NULL) {
        return UC_ERR_INVALID_PARAM;
    }
    const EIF_Result r = EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m));
    if (r != EIF_OK) {
        return to_uc(r);
    }
    memset(out, 0, sizeof(*out));
    out->dhcp = m.dhcp;
    out->filtering = m.filtering;
    (void)snprintf(out->ip, sizeof(out->ip), "%s", m.ip);
    (void)snprintf(out->netmask, sizeof(out->netmask), "%s", m.netmask);
    (void)snprintf(out->gateway, sizeof(out->gateway), "%s", m.gateway);
    (void)snprintf(out->hostname, sizeof(out->hostname), "%s", m.hostname);
    (void)snprintf(out->macaddress, sizeof(out->macaddress), "%s", m.macaddress);
    return UC_OK;
}

static uc_result_t rest_patch_network(const network_settings_patch_t *in)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle handle = 0U;

    if (in == NULL) {
        return UC_ERR_INVALID_PARAM;
    }
    if (in->has_macaddress) {
        return UC_ERR_INVALID_PARAM; /* MACアドレスは変更できない */
    }
    memset(&p, 0, sizeof(p));
    p.has_dhcp = in->has_dhcp;
    p.dhcp = in->dhcp;
    p.has_ip = in->has_ip;
    (void)snprintf(p.ip, sizeof(p.ip), "%s", in->ip);
    p.has_netmask = in->has_netmask;
    (void)snprintf(p.netmask, sizeof(p.netmask), "%s", in->netmask);
    p.has_gateway = in->has_gateway;
    (void)snprintf(p.gateway, sizeof(p.gateway), "%s", in->gateway);
    p.has_filtering = in->has_filtering;
    p.filtering = in->filtering;
    p.has_hostname = in->has_hostname;
    (void)snprintf(p.hostname, sizeof(p.hostname), "%s", in->hostname);

    const uint64_t token = CONNECTOR_TOKEN_PATCH_FLAG | atomic_fetch_add(&s_next_token, 1U);
    waiter_t *w = waiter_acquire(token); /* PCL経由になった場合に備え、先に待ち合わせを用意する */
    if (w == NULL) {
        return UC_ERR_BUSY;
    }
    const EIF_Result r = EIF_PatchSetting(s_id, EIF_SETTING_NETWORK, &p, sizeof(p), token, &handle);
    if (r == EIF_ACCEPTED) {
        return waiter_wait_and_release(w); /* RESTは同期応答のため上限時間まで待つ */
    }
    pthread_mutex_lock(&s_wait_lock);
    w->used = false;
    pthread_mutex_unlock(&s_wait_lock);
    return to_uc(r);
}

static uc_result_t rest_jobs_create(const job_create_request_t *in, accepted_job_t *out)
{
    eif_request_id_t req;
    EIF_RequestHandle handle = 0U;

    if ((in == NULL) || (out == NULL)) {
        return UC_ERR_INVALID_PARAM;
    }
    if (!job_to_request(in->job_type, &req)) {
        return UC_ERR_NOT_IMPLEMENTED;
    }
    const uint64_t token = atomic_fetch_add(&s_next_token, 1U);
    const EIF_Result r = EIF_SubmitRequest(s_id, req, NULL, 0U, token, &handle);
    if (r != EIF_ACCEPTED) {
        return to_uc(r);
    }
    (void)snprintf(out->job_id, sizeof(out->job_id), "%llu", (unsigned long long)handle);
    return UC_OK;
}

/* ---------------------------------------------------------------- EIFからのcallback */

static void on_request_completed(EIF_RequestHandle handle, eif_request_id_t request_id, uint64_t origin_token,
                                 EIF_RequestOutcome outcome, void *user_context)
{
    ws_event_t ev;
    job_event_fields_t fields;
    (void)user_context;

    if (((origin_token & CONNECTOR_TOKEN_PATCH_FLAG) != 0U) && waiter_signal(origin_token, outcome)) {
        return; /* PATCHの待ち合わせへ渡した */
    }

    memset(&ev, 0, sizeof(ev));
    memset(&fields, 0, sizeof(fields));
    (void)snprintf(fields.job_id, sizeof(fields.job_id), "%llu", (unsigned long long)handle);
    if (!request_to_job(request_id, &fields.job_type)) {
        return;
    }
    switch (outcome) {
    case EIF_OUTCOME_COMPLETED:
        ev.event_type = WS_EVENT_JOB_COMPLETED;
        ev.event_payload.job_completed.job_fields = fields;
        break;
    case EIF_OUTCOME_CANCELED:
        ev.event_type = WS_EVENT_JOB_CANCELED;
        ev.event_payload.job_canceled.job_fields = fields;
        break;
    default:
        ev.event_type = WS_EVENT_JOB_ERROR;
        ev.event_payload.job_error.job_fields = fields;
        (void)snprintf(ev.event_payload.job_error.error_code, sizeof(ev.event_payload.job_error.error_code),
                       "OUTCOME_%d", (int)outcome);
        break;
    }
    (void)server_ws_emit_event(&ev); /* コピーして即復帰する */
}

static void on_event(const EIF_Event *event, void *user_context)
{
    ws_event_t ev;
    alarm_fields_t *alarm;
    (void)user_context;

    memset(&ev, 0, sizeof(ev));
    switch (event->event_id) {
    case EIF_EVT_ERROR_RAISED:
        ev.event_type = WS_EVENT_ERROR_RAISED;
        alarm = &ev.event_payload.error_raised.alarm_fields;
        break;
    case EIF_EVT_ERROR_RESOLVED:
        ev.event_type = WS_EVENT_ERROR_RESOLVED;
        alarm = &ev.event_payload.error_resolved.alarm_fields;
        break;
    default:
        return;
    }
    (void)snprintf(alarm->alarm_id, sizeof(alarm->alarm_id), "%s", event->payload.alarm.alarm_id);
    (void)snprintf(alarm->alarm_code, sizeof(alarm->alarm_code), "%s", event->payload.alarm.alarm_code);
    (void)server_ws_emit_event(&ev);
}

/* ---------------------------------------------------------------- 入口 */

bool EifApiServerConnector_Setup(void)
{
    static const eif_event_id_t filter[] = { EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED };
    const EIF_ConnectorDesc desc = {
        .name = "api_server",
        .on_request_completed = on_request_completed,
        .on_event = on_event,
        .event_filter = filter,
        .event_filter_count = sizeof(filter) / sizeof(filter[0]),
        .user_context = NULL,
    };
    server_rest_callbacks_t cbs;

    if (EIF_RegisterConnector(&desc, &s_id) != EIF_OK) {
        return false;
    }
    memset(&cbs, 0, sizeof(cbs));
    cbs.network_settings.get = rest_get_network;
    cbs.network_settings.patch = rest_patch_network;
    cbs.jobs.create = rest_jobs_create;
    return server_rest_register_callbacks(&cbs) == SERVER_OK;
}
