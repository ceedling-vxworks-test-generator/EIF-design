/**
 * @file api_server_connector.c
 * @brief APIサーバー用Connector（詳細設計書 4.1）。
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
#include "event_mapping.h"
#include "request_mapping.h"
#include "response_mapping.h"
#include "server_api.h"
#include "subscription_mapping.h"

/** @brief PATCHがPCL経由（EIF_ACCEPTED）になった場合に完了を待つ上限[秒]。 */
#define CONNECTOR_PATCH_WAIT_SEC (5)
/** @brief 同時に待てるPATCHの数。 */
#define CONNECTOR_PATCH_WAITERS (4U)
/** @brief PATCHの待ち合わせに使う要求元トークンの印（最上位ビット）。ジョブのトークンと区別する。 */
#define CONNECTOR_TOKEN_PATCH_FLAG (UINT64_C(1) << 63)

static EIF_ConnectorId s_id = 0U;               /**< EIFが払い出した自身のConnector ID */
static atomic_uint_fast64_t s_next_token = 1U;  /**< 次に使う要求元トークン */

/* 対応表（接続先依存）は request_mapping／response_mapping／event_mapping／subscription_mapping に置く */

/* ---------------------------------------------------------------- PATCHの待ち合わせ（PCL経由になった場合） */

/** @brief 完了待ち合わせ表の1行（詳細設計書 3.1.5 PendingEntry）。 */
typedef struct {
    bool used;                   /**< 使用中 */
    bool done;                   /**< 完了callbackが届いた */
    uint64_t token;              /**< 待っている要求元トークン */
    EIF_RequestOutcome outcome;  /**< 届いた終端結果 */
} PendingEntry;

static PendingEntry s_pending[CONNECTOR_PATCH_WAITERS];          /**< 完了待ち合わせ表 */
static pthread_mutex_t s_wait_lock = PTHREAD_MUTEX_INITIALIZER;  /**< 待ち合わせ表の保護 */
static pthread_cond_t s_wait_cond = PTHREAD_COND_INITIALIZER;    /**< 完了の知らせ */

/**
 * @brief 待ち合わせを1件確保する。
 * @param[in] token 待つ要求元トークン
 * @return 確保した待ち合わせ。満杯ならNULL
 */
static PendingEntry *pending_acquire(uint64_t token)
{
    PendingEntry *w = NULL;
    pthread_mutex_lock(&s_wait_lock);
    for (size_t i = 0U; i < CONNECTOR_PATCH_WAITERS; i++) {
        if (!s_pending[i].used) {
            w = &s_pending[i];
            w->used = true;
            w->done = false;
            w->token = token;
            break;
        }
    }
    pthread_mutex_unlock(&s_wait_lock);
    return w;
}

/**
 * @brief 完了を上限時間まで待ち、待ち合わせを解放する。
 * @param[in,out] w pending_acquire() で確保した待ち合わせ
 * @retval UC_OK            正常完了
 * @retval UC_ERR_INTERNAL  正常完了以外で終端
 * @retval UC_ERR_TIMEOUT   上限時間（CONNECTOR_PATCH_WAIT_SEC）内に完了しなかった
 * @note APIサーバーのblockingスレッド [C2] で待つ。
 */
static uc_result_t pending_wait_and_release(PendingEntry *w)
{
    struct timespec until;
    uc_result_t r = UC_ERR_TIMEOUT;

    (void)clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += CONNECTOR_PATCH_WAIT_SEC;
    pthread_mutex_lock(&s_wait_lock);
    while (!w->done) {
        if (pthread_cond_timedwait(&s_wait_cond, &s_wait_lock, &until) != 0) {
            break; /* 上限時間 */
        }
    }
    if (w->done) {
        r = (w->outcome == EIF_OUTCOME_COMPLETED) ? UC_OK : UC_ERR_INTERNAL;
    }
    w->used = false;
    pthread_mutex_unlock(&s_wait_lock);
    return r;
}

/**
 * @brief 待ち合わせに完了を知らせる。
 * @param[in] token   完了した要求の要求元トークン
 * @param[in] outcome 終端結果
 * @return そのトークンを待っている待ち合わせがあれば true
 * @note EIFの完了callback（PCLの要求処理context [C3]）から呼ばれる。
 */
static bool pending_signal(uint64_t token, EIF_RequestOutcome outcome)
{
    bool found = false;
    pthread_mutex_lock(&s_wait_lock);
    for (size_t i = 0U; i < CONNECTOR_PATCH_WAITERS; i++) {
        if (s_pending[i].used && (s_pending[i].token == token)) {
            s_pending[i].done = true;
            s_pending[i].outcome = outcome;
            found = true;
            break;
        }
    }
    pthread_cond_broadcast(&s_wait_cond);
    pthread_mutex_unlock(&s_wait_lock);
    return found;
}

/* ---------------------------------------------------------------- REST callback（APIサーバーのblockingスレッド） */

/**
 * @brief REST GET /network_settings。EIFの公開モデルをAPIサーバーの型へ詰め替える。
 * @param[out] out APIサーバーの型
 * @retval UC_OK 取得した
 * @retval その他 EIF_GetInfo() の結果の変換結果
 * @note APIサーバーのblockingスレッド [C2] から呼ばれる。すべて同期で完了する。
 */
static uc_result_t rest_get_network(network_settings_t *out)
{
    eif_network_settings_t m;

    if (out == NULL) {
        return UC_ERR_INVALID_PARAM;
    }
    const EIF_Result r = EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m));
    if (r != EIF_OK) {
        return response_mapping_to_external(r);
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

/**
 * @brief REST PATCH /network_settings。APIサーバーの型をEIFのパッチモデルへ詰め替えて更新する。
 *
 * EIF_PatchSetting() が EIF_OK（REFへ直接）ならその場で応答する。
 * EIF_ACCEPTED（PCL経由）なら、RESTは同期応答のため、完了callbackを上限時間まで待つ。
 *
 * @param[in] in APIサーバーのパッチ
 * @retval UC_OK                 反映した
 * @retval UC_ERR_INVALID_PARAM  in がNULL、MACアドレスの変更、値が不正
 * @retval UC_ERR_BUSY           待ち合わせが満杯
 * @retval UC_ERR_TIMEOUT        PCL経由で上限時間内に完了しなかった
 * @retval その他                EIF_PatchSetting() の結果の変換結果
 * @note APIサーバーのblockingスレッド [C2] から呼ばれる。
 */
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
    PendingEntry *w = pending_acquire(token); /* PCL経由になった場合に備え、先に待ち合わせを用意する（先着完了に備える） */
    if (w == NULL) {
        return UC_ERR_BUSY;
    }
    const EIF_Result r = EIF_PatchSetting(s_id, EIF_SETTING_NETWORK, &p, sizeof(p), token, &handle);
    if (r == EIF_ACCEPTED) {
        return pending_wait_and_release(w); /* RESTは同期応答のため上限時間まで待つ */
    }
    /* 同期で終わった（または失敗した）ので待ち合わせは不要 */
    pthread_mutex_lock(&s_wait_lock);
    w->used = false;
    pthread_mutex_unlock(&s_wait_lock);
    return response_mapping_to_external(r);
}

/**
 * @brief REST POST /jobs。ジョブ種別を公開要求IDへ変換してEIFへ要求し、要求ハンドルを job_id として返す。
 *
 * 受付だけで即時に応答する（HTTP 202）。結果は on_request_completed() でWebSocketへ送る。
 *
 * @param[in]  in  ジョブ作成要求
 * @param[out] out 受付結果（job_id ＝ 要求ハンドル）
 * @retval UC_OK                   受け付けた
 * @retval UC_ERR_INVALID_PARAM    in・out がNULL
 * @retval UC_ERR_NOT_IMPLEMENTED  対応表に無いジョブ種別
 * @retval その他                  EIF_SubmitRequest() の結果の変換結果
 * @note APIサーバーのblockingスレッド [C2] から呼ばれる。
 */
static uc_result_t rest_jobs_create(const job_create_request_t *in, accepted_job_t *out)
{
    eif_request_id_t req;
    EIF_RequestHandle handle = 0U;

    if ((in == NULL) || (out == NULL)) {
        return UC_ERR_INVALID_PARAM;
    }
    if (!request_mapping_to_public(in->job_type, &req)) {
        return UC_ERR_NOT_IMPLEMENTED;
    }
    const uint64_t token = atomic_fetch_add(&s_next_token, 1U);
    const EIF_Result r = EIF_SubmitRequest(s_id, req, NULL, 0U, token, &handle);
    if (r != EIF_ACCEPTED) {
        return response_mapping_to_external(r);
    }
    (void)snprintf(out->job_id, sizeof(out->job_id), "%llu", (unsigned long long)handle);
    return UC_OK;
}

/* ---------------------------------------------------------------- EIFからのcallback */

/**
 * @brief EIFの完了callback。PATCHの待ち合わせへ渡すか、ジョブの結果をWebSocketへ送る。
 *
 * @param[in] handle       要求ハンドル（＝ job_id）
 * @param[in] request_id   公開要求ID
 * @param[in] origin_token 要求時に渡したトークン（PATCHなら CONNECTOR_TOKEN_PATCH_FLAG 付き）
 * @param[in] outcome      終端結果
 * @param[in] user_context 使わない
 *
 * @note PCLの要求処理context [C3] から呼ばれる。rest_jobs_create() の戻りより先に呼ばれることがある。
 *       server_ws_emit_event() はコピーして即復帰するため、長時間ブロックしない。
 */
static void on_request_completed(EIF_RequestHandle handle, eif_request_id_t request_id, uint64_t origin_token,
                                 EIF_RequestOutcome outcome, void *user_context)
{
    ws_event_t ev;
    job_event_fields_t fields;
    (void)user_context;

    if (((origin_token & CONNECTOR_TOKEN_PATCH_FLAG) != 0U) && pending_signal(origin_token, outcome)) {
        return; /* PATCHの待ち合わせへ渡した */
    }

    memset(&ev, 0, sizeof(ev));
    memset(&fields, 0, sizeof(fields));
    (void)snprintf(fields.job_id, sizeof(fields.job_id), "%llu", (unsigned long long)handle);
    if (!request_mapping_to_external(request_id, &fields.job_type)) {
        return; /* ジョブ以外（待ち合わせが時間切れになったPATCH等）は送らない */
    }
    ev.event_type = event_mapping_outcome_to_external(outcome); /* 外部イベント対応表（完了通知） */
    switch (ev.event_type) {
    case WS_EVENT_JOB_COMPLETED:
        ev.event_payload.job_completed.job_fields = fields;
        break;
    case WS_EVENT_JOB_CANCELED:
        ev.event_payload.job_canceled.job_fields = fields;
        break;
    default:
        ev.event_payload.job_error.job_fields = fields;
        (void)snprintf(ev.event_payload.job_error.error_code, sizeof(ev.event_payload.job_error.error_code),
                       "OUTCOME_%d", (int)outcome);
        break;
    }
    (void)server_ws_emit_event(&ev); /* コピーして即復帰する */
}

/**
 * @brief EIFのイベントcallback。異常の発生／解消をWebSocketのイベントへ変換して送る。
 *
 * @param[in] event        公開イベント（呼出し中だけ有効）
 * @param[in] user_context 使わない
 *
 * @note REFの通知ワーカー [C4] から呼ばれる。購読要求に含めていない公開イベントは届かない。
 */
static void on_event(const EIF_Event *event, void *user_context)
{
    ws_event_t ev;
    alarm_fields_t *alarm;
    (void)user_context;

    memset(&ev, 0, sizeof(ev));
    if (!event_mapping_to_external(event->event_id, &ev.event_type)) {
        return; /* 外部イベント対応表に無い公開イベントは送らない */
    }
    switch (ev.event_type) {
    case WS_EVENT_ERROR_RAISED:
        alarm = &ev.event_payload.error_raised.alarm_fields;
        break;
    case WS_EVENT_ERROR_RESOLVED:
        alarm = &ev.event_payload.error_resolved.alarm_fields;
        break;
    default:
        return; /* ペイロードの詰め替えを用意していない種別 */
    }
    (void)snprintf(alarm->alarm_id, sizeof(alarm->alarm_id), "%s", event->payload.alarm.alarm_id);
    (void)snprintf(alarm->alarm_code, sizeof(alarm->alarm_code), "%s", event->payload.alarm.alarm_code);
    (void)server_ws_emit_event(&ev);
}

/* ---------------------------------------------------------------- 入口 */

/**
 * @brief EIFへ自身を登録し、APIサーバーへRESTのcallbackを登録する。
 *
 * 購読要求は異常の発生／解消（EIF_EVT_ERROR_RAISED／RESOLVED）を変化時に受け取る（詳細設計書 3.1.4）。
 *
 * @retval true  成功
 * @retval false EIFへの登録、またはAPIサーバーへの登録に失敗
 *
 * @note 起動処理のスレッド [C1] から、EIF_Start() より前に呼ぶ。
 */
bool EifApiServerConnector_Setup(void)
{
    const EIF_ConnectorDesc desc = {
        .name = "api_server",
        .on_request_completed = on_request_completed,
        .on_event = on_event,
        .subscriptions = g_subscription_mapping,           /* 購読要求定義（3.1.4） */
        .subscription_count = g_subscription_mapping_count,
        .user_context = NULL,
    };
    server_rest_callbacks_t cbs;

    /* ① EIFへ：完了・イベントを受け取る関数を渡す */
    if (EIF_RegisterConnector(&desc, &s_id) != EIF_OK) {
        return false;
    }
    /* ② APIサーバーへ：REST要求を受け取る関数を渡す */
    memset(&cbs, 0, sizeof(cbs));
    cbs.network_settings.get = rest_get_network;
    cbs.network_settings.patch = rest_patch_network;
    cbs.jobs.create = rest_jobs_create;
    return server_rest_register_callbacks(&cbs) == SERVER_OK;
}
