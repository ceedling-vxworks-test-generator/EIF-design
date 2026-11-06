/**
 * @file test_api_server_connector.c
 * @brief APIサーバー用Connectorの試験。Connectorの対応表（詳細設計書 3.1）と、Frameとのつなぎを確認する。
 *
 * APIサーバー（外部コンポーネント）の関数は、このファイルで試験用に仮実装する
 * （server_rest_register_callbacks：登録されたREST callbackを記録、server_ws_emit_event：送ったイベントを記録）。
 * EIFの下側はスタブAccessor（test/stubs）を使う。
 */
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "eif_api.h"
#include "eif_connector_api_server.h"
#include "eif_frame_api.h"
#include "eif_test.h"
#include "event_mapping.h"
#include "request_mapping.h"
#include "response_mapping.h"
#include "server_api.h"
#include "stub_accessors.h"
#include "subscription_mapping.h"

int g_test_failures = 0;
int g_test_runs = 0;

/* ================================================================ APIサーバーの仮実装（試験用） */

static server_rest_callbacks_t s_cbs;  /**< 登録されたREST callback */
static int s_cbs_registered;           /**< server_rest_register_callbacks() の回数 */
static ws_event_t s_ws[8];             /**< 送られたWebSocketイベント */
static int s_ws_count;                 /**< 送られた数 */

/**
 * @brief 仮実装：REST callbackを記録する。
 * @param[in] callbacks 登録するcallback
 * @return SERVER_OK
 */
int server_rest_register_callbacks(const server_rest_callbacks_t *callbacks)
{
    s_cbs = *callbacks;
    s_cbs_registered++;
    return SERVER_OK;
}

/**
 * @brief 仮実装：WebSocketイベントを記録する（本物はコピーして非同期に配信する）。
 * @param[in] event 送るイベント
 * @return SERVER_OK
 */
int server_ws_emit_event(const ws_event_t *event)
{
    if ((size_t)s_ws_count < sizeof(s_ws) / sizeof(s_ws[0])) {
        s_ws[s_ws_count++] = *event;
    }
    return SERVER_OK;
}

/* ================================================================ 前処理 */

/** @brief 前処理：Frameを初期化し、Connectorを登録して開始する。 */
static void setup(void)
{
    EIF_Frame_ResetForTest();
    stub_reset();
    memset(&s_cbs, 0, sizeof(s_cbs));
    memset(s_ws, 0, sizeof(s_ws));
    s_cbs_registered = 0;
    s_ws_count = 0;
    (void)EIF_Frame_Initialize(&g_stub_accessors);
    (void)EifApiServerConnector_Setup();
    (void)EIF_Start();
}

/** @brief 前処理：表の試験用（EIFは使わない）。 */
static void setup_none(void)
{
}

/* ---------------------------------------------------------------- 対応表 */

/** @brief 外部要求対応表（3.1.1）：ジョブ種別 ⇔ 公開要求ID を両方向に引ける。表に無いジョブ種別は false。 */
static void test_request_mapping(void)
{
    eif_request_id_t req = EIF_REQ_INVALID;
    ExternalRequestType job = JOB_TYPE_PRINT;
    TEST_ASSERT(request_mapping_to_public(JOB_TYPE_HEAD_CLEANING, &req));
    TEST_EQ(req, EIF_REQ_HEAD_CLEANING);
    TEST_ASSERT(request_mapping_to_external(EIF_REQ_WIPER_MAINTENANCE, &job));
    TEST_EQ(job, JOB_TYPE_WIPER_MAINTENANCE);
    TEST_ASSERT(!request_mapping_to_public((ExternalRequestType)9999, &req));
    for (size_t i = 0U; i < g_request_mapping_count; i++) { /* 1対1（往復で戻る） */
        TEST_ASSERT(request_mapping_to_external(g_request_mapping[i].public_request, &job));
        TEST_EQ(job, g_request_mapping[i].external_request);
    }
}

/** @brief 結果コード対応表（3.1.2）：設計書の対応どおり。表に無い結果コードは UC_ERR_INTERNAL。 */
static void test_result_mapping(void)
{
    TEST_EQ(response_mapping_to_external(EIF_OK), UC_OK);
    TEST_EQ(response_mapping_to_external(EIF_ACCEPTED), UC_OK);
    TEST_EQ(response_mapping_to_external(EIF_ERR_INVALID_PARAM), UC_ERR_INVALID_PARAM);
    TEST_EQ(response_mapping_to_external(EIF_ERR_NOT_EXPOSED), UC_ERR_NOT_IMPLEMENTED);
    TEST_EQ(response_mapping_to_external(EIF_ERR_NOT_FOUND), UC_ERR_NOT_FOUND);
    TEST_EQ(response_mapping_to_external(EIF_ERR_BUSY), UC_ERR_BUSY);
    TEST_EQ(response_mapping_to_external(EIF_ERR_UNAVAILABLE), UC_ERR_SERVER_UNAVAILABLE);
    TEST_EQ(response_mapping_to_external(EIF_ERR_INTERNAL), UC_ERR_INTERNAL);
}

/** @brief 外部イベント対応表（3.1.3）：公開イベント・終端結果 → WebSocketのイベント種別。 */
static void test_event_mapping(void)
{
    ExternalEventType t = WS_EVENT_JOB_STARTED;
    TEST_ASSERT(event_mapping_to_external(EIF_EVT_ERROR_RAISED, &t));
    TEST_EQ(t, WS_EVENT_ERROR_RAISED);
    TEST_ASSERT(event_mapping_to_external(EIF_EVT_ERROR_RESOLVED, &t));
    TEST_EQ(t, WS_EVENT_ERROR_RESOLVED);
    TEST_ASSERT(!event_mapping_to_external(EIF_EVT_JOB_PROGRESS, &t)); /* 表に無い公開イベントは送らない */
    TEST_EQ(event_mapping_outcome_to_external(EIF_OUTCOME_COMPLETED), WS_EVENT_JOB_COMPLETED);
    TEST_EQ(event_mapping_outcome_to_external(EIF_OUTCOME_CANCELED), WS_EVENT_JOB_CANCELED);
    TEST_EQ(event_mapping_outcome_to_external(EIF_OUTCOME_FAILED), WS_EVENT_JOB_ERROR);
}

/** @brief 購読要求定義（3.1.4）：EIFの登録時の検査を通り、組合せごとに購読される。 */
static void test_subscription_mapping_registered(void)
{
    TEST_EQ(s_cbs_registered, 1);
    TEST_EQ(g_stub.watch_count, (int)g_subscription_mapping_count);
    for (size_t i = 0U; i < g_subscription_mapping_count; i++) {
        TEST_EQ(g_stub.watches[i].req.id, g_subscription_mapping[i].id);
        TEST_EQ(g_stub.watches[i].req.type, g_subscription_mapping[i].type);
    }
}

/* ---------------------------------------------------------------- REST → EIF → WebSocket */

/** @brief ジョブ：受付で job_id を返し、完了は WebSocket の job completed で届く（対応表の逆引きでジョブ種別を戻す）。 */
static void test_job_accepted_then_completed(void)
{
    job_create_request_t in;
    accepted_job_t out;
    memset(&in, 0, sizeof(in));
    memset(&out, 0, sizeof(out));
    in.job_type = JOB_TYPE_PRINT;
    TEST_EQ(s_cbs.jobs.create(&in, &out), UC_OK);
    TEST_ASSERT(strcmp(out.job_id, "1") == 0);
    g_stub.done(g_stub.next_ticket, EIF_OUTCOME_COMPLETED);
    TEST_EQ(s_ws_count, 1);
    TEST_EQ(s_ws[0].event_type, WS_EVENT_JOB_COMPLETED);
    TEST_ASSERT(strcmp(s_ws[0].event_payload.job_completed.job_fields.job_id, "1") == 0);
    TEST_EQ(s_ws[0].event_payload.job_completed.job_fields.job_type, JOB_TYPE_PRINT);
}

/** @brief ジョブ：取消は job canceled、失敗は job error で届く。 */
static void test_job_canceled_and_failed(void)
{
    job_create_request_t in;
    accepted_job_t out;
    memset(&in, 0, sizeof(in));
    in.job_type = JOB_TYPE_PRINT;
    TEST_EQ(s_cbs.jobs.create(&in, &out), UC_OK);
    g_stub.done(g_stub.next_ticket, EIF_OUTCOME_CANCELED);
    TEST_EQ(s_cbs.jobs.create(&in, &out), UC_OK);
    g_stub.done(g_stub.next_ticket, EIF_OUTCOME_FAILED);
    TEST_EQ(s_ws_count, 2);
    TEST_EQ(s_ws[0].event_type, WS_EVENT_JOB_CANCELED);
    TEST_EQ(s_ws[1].event_type, WS_EVENT_JOB_ERROR);
}

/** @brief ジョブ：機種で公開していない要求は EIF の NOT_EXPOSED を変換した UC_ERR_NOT_IMPLEMENTED。表に無いジョブ種別も同じ（EIFは呼ばない）。 */
static void test_job_not_exposed(void)
{
    job_create_request_t in;
    accepted_job_t out;
    memset(&in, 0, sizeof(in));
    in.job_type = JOB_TYPE_HEAD_CLEANING; /* スタブAccessorは EIF_REQ_PRINT だけ公開する */
    TEST_EQ(s_cbs.jobs.create(&in, &out), UC_ERR_NOT_IMPLEMENTED);
    in.job_type = (job_type_t)9999;
    TEST_EQ(s_cbs.jobs.create(&in, &out), UC_ERR_NOT_IMPLEMENTED);
    TEST_EQ(g_stub.next_ticket, 0U); /* PCLへは何も送っていない */
}

/** @brief 情報取得：EIFの公開モデルがAPIサーバーの型へ詰め替えられる。 */
static void test_get_network(void)
{
    network_settings_t out;
    TEST_EQ(s_cbs.network_settings.get(&out), UC_OK);
    TEST_ASSERT(out.dhcp);
    TEST_ASSERT(strcmp(out.ip, "10.0.0.2") == 0);
    TEST_ASSERT(strcmp(out.hostname, "mp") == 0);
}

/** @brief 設定変更（REF直接）：指定した項目だけ保存される。MACアドレスの変更はEIFを呼ばずに拒否する。 */
static void test_patch_network_direct(void)
{
    network_settings_patch_t in;
    memset(&in, 0, sizeof(in));
    in.has_ip = true;
    (void)strcpy(in.ip, "10.0.0.9");
    TEST_EQ(s_cbs.network_settings.patch(&in), UC_OK);
    TEST_ASSERT(strcmp(g_stub.stored.ip, "10.0.0.9") == 0);
    TEST_EQ(g_stub.store_calls, 1);
    memset(&in, 0, sizeof(in));
    in.has_macaddress = true;
    TEST_EQ(s_cbs.network_settings.patch(&in), UC_ERR_INVALID_PARAM);
    TEST_EQ(g_stub.store_calls, 1);
}

/**
 * @brief PCLの完了を別スレッドから届ける（PATCHがPCL経由で待っている間に完了させる）。
 * @param[in] arg 使わない
 * @return NULL
 */
static void *complete_later(void *arg)
{
    const struct timespec t = { 0, 10 * 1000 * 1000 };
    (void)arg;
    for (int i = 0; (i < 200) && (__atomic_load_n(&g_stub.store_calls, __ATOMIC_SEQ_CST) == 0); i++) {
        (void)nanosleep(&t, NULL);
    }
    (void)nanosleep(&t, NULL);
    g_stub.done(g_stub.next_ticket, EIF_OUTCOME_COMPLETED);
    return NULL;
}

/** @brief 設定変更（PCL経由）：完了待ち合わせ表（3.1.5）で完了callbackを待ってから応答する。 */
static void test_patch_network_via_pcl_waits(void)
{
    network_settings_patch_t in;
    pthread_t th;
    memset(&in, 0, sizeof(in));
    g_stub.store_result = EIF_ACCEPTED;
    TEST_EQ(pthread_create(&th, NULL, complete_later, NULL), 0);
    const uc_result_t r = s_cbs.network_settings.patch(&in);
    (void)pthread_join(th, NULL);
    TEST_EQ(r, UC_OK);
    TEST_EQ(s_ws_count, 0); /* PATCHの完了はWebSocketへ送らない */
}

/** @brief イベント：異常の発生が WebSocket の error raised として、アラームの内容つきで届く。 */
static void test_event_to_ws(void)
{
    EIF_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.event_id = EIF_EVT_ERROR_RAISED;
    (void)strcpy(ev.payload.alarm.alarm_id, "MP0004");
    (void)strcpy(ev.payload.alarm.alarm_code, "E00004");
    TEST_ASSERT(g_stub.watch_count >= 1);
    TEST_EQ(g_stub.watches[0].req.id, EIF_EVT_ERROR_RAISED);
    g_stub.watches[0].fn(&ev, g_stub.watches[0].ctx); /* REF Accessorからの通知を再現 */
    TEST_EQ(s_ws_count, 1);
    TEST_EQ(s_ws[0].event_type, WS_EVENT_ERROR_RAISED);
    TEST_ASSERT(strcmp(s_ws[0].event_payload.error_raised.alarm_fields.alarm_id, "MP0004") == 0);
}

/**
 * @brief 試験の入口。
 * @return 失敗が無ければ0
 */
int main(void)
{
    printf("== Connector tests: api_server（APIサーバーは試験用の仮実装）\n");
    RUN_TEST(setup_none, test_request_mapping);
    RUN_TEST(setup_none, test_result_mapping);
    RUN_TEST(setup_none, test_event_mapping);
    RUN_TEST(setup, test_subscription_mapping_registered);
    RUN_TEST(setup, test_job_accepted_then_completed);
    RUN_TEST(setup, test_job_canceled_and_failed);
    RUN_TEST(setup, test_job_not_exposed);
    RUN_TEST(setup, test_get_network);
    RUN_TEST(setup, test_patch_network_direct);
    RUN_TEST(setup, test_patch_network_via_pcl_waits);
    RUN_TEST(setup, test_event_to_ws);
    return TEST_SUMMARY();
}
