/**
 * @file test_frame.c
 * @brief Frameの単体試験。スタブAccessorだけで動かす（PCL・REFのヘッダーは一切使わない）。
 *
 * 2つの試験用Connector（A：異常の発生／解消を受信、B：イベントを受信しない）を登録して確認する。
 */
#include <string.h>

#include "eif_api.h"
#include "eif_frame_api.h"
#include "eif_test.h"
#include "stub_accessors.h"

int g_test_failures = 0;
int g_test_runs = 0;

/** @brief 試験用Connectorが受け取った内容の記録。 */
typedef struct {
    int completed;                       /**< 完了callbackの回数 */
    EIF_RequestHandle handle;            /**< 最後の完了の要求ハンドル */
    uint64_t token;                      /**< 最後の完了の要求元トークン */
    EIF_RequestOutcome outcome;          /**< 最後の完了の終端結果 */
    int events;                          /**< イベントcallbackの回数 */
    eif_event_id_t last_event;           /**< 最後のイベントID */
    char last_alarm[EIF_ALARM_TEXT_LEN]; /**< 最後のイベントの alarm_id */
} conn_t;

static conn_t s_a;              /**< Connector A の記録 */
static conn_t s_b;              /**< Connector B の記録 */
static EIF_ConnectorId s_id_a;  /**< Connector A のID */
static EIF_ConnectorId s_id_b;  /**< Connector B のID */

/**
 * @brief 試験用Connectorの完了callback。受け取った内容を記録する。
 * @param[in] h     要求ハンドル
 * @param[in] r     公開要求ID（使わない）
 * @param[in] token 要求元トークン
 * @param[in] o     終端結果
 * @param[in] ctx   記録先（conn_t*）
 */
static void on_completed(EIF_RequestHandle h, eif_request_id_t r, uint64_t token, EIF_RequestOutcome o, void *ctx)
{
    conn_t *c = (conn_t *)ctx;
    (void)r;
    c->completed++;
    c->handle = h;
    c->token = token;
    c->outcome = o;
}

/**
 * @brief 試験用Connectorのイベントcallback。受け取った内容を記録する。
 * @param[in] e   公開イベント
 * @param[in] ctx 記録先（conn_t*）
 */
static void on_event(const EIF_Event *e, void *ctx)
{
    conn_t *c = (conn_t *)ctx;
    c->events++;
    c->last_event = e->event_id;
    memcpy(c->last_alarm, e->payload.alarm.alarm_id, EIF_ALARM_TEXT_LEN);
}

/** @brief 前処理：Frame・スタブ・記録をすべて初期状態に戻す（未初期化）。 */
static void setup_none(void)
{
    EIF_Frame_ResetForTest();
    stub_reset();
    memset(&s_a, 0, sizeof(s_a));
    memset(&s_b, 0, sizeof(s_b));
}

/** @brief 前処理：初期化 → Connector A・B を登録 → 開始 まで済ませる。 */
static void setup_started(void)
{
    static const eif_event_id_t filter[] = { EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED };
    EIF_ConnectorDesc a = { "A", on_completed, on_event, filter, 2U, &s_a };
    EIF_ConnectorDesc b = { "B", on_completed, on_event, NULL, 0U, &s_b };
    setup_none();
    (void)EIF_Frame_Initialize(&g_stub_accessors);
    (void)EIF_RegisterConnector(&a, &s_id_a);
    (void)EIF_RegisterConnector(&b, &s_id_b);
    (void)EIF_Start();
}

/**
 * @brief REF Accessorからの変化通知を再現する（アラーム一覧を渡す）。
 * @param[in] ids アラームIDの配列
 * @param[in] n   件数
 */
static void push_alarms(const char *const *ids, unsigned n)
{
    eif_alarm_list_t l;
    memset(&l, 0, sizeof(l));
    for (unsigned i = 0U; i < n; i++) {
        (void)strcpy(l.items[i].alarm_id, ids[i]);
    }
    l.count = n;
    g_stub.watch_fn(EIF_INFO_ALARM_LIST, &l, sizeof(l), g_stub.watch_ctx); /* REF Accessorからの変化通知を再現 */
}

/* ---------------------------------------------------------------- 試験 */

/** @brief 構成の検証：NULLは INVALID_PARAM、下位IFが欠けていれば INVALID_CONFIG で、未初期化のまま。 */
static void test_initialize_rejects_incomplete_accessors(void)
{
    EIF_AccessorSet p = g_stub_accessors;
    p.setting = NULL;
    TEST_EQ(EIF_Frame_Initialize(NULL), EIF_ERR_INVALID_PARAM);
    TEST_EQ(EIF_Frame_Initialize(&p), EIF_ERR_INVALID_CONFIG);
    TEST_EQ(EIF_Start(), EIF_ERR_UNAVAILABLE);
}

/** @brief 開始：通知定義表の情報だけ監視を依頼し、完了トランポリンが渡されている。 */
static void test_start_watches_only_supported_info(void)
{
    TEST_EQ(EIF_Frame_Initialize(&g_stub_accessors), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub.watch_calls, 1);
    TEST_ASSERT(g_stub.done != NULL); /* 完了の受け口がつながっている */
}

/** @brief 公開範囲：対応表に無い要求は NOT_EXPOSED で、要求ハンドルは0。 */
static void test_not_exposed_request(void)
{
    EIF_RequestHandle h = 7U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_HEAD_CLEANING, NULL, 0U, 1U, &h), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(h, 0U);
}

/** @brief 振り分け：A・Bが要求し、逆順に完了しても、それぞれの要求元にだけ届く。 */
static void test_completion_routed_to_origin(void)
{
    EIF_RequestHandle ha = 0U;
    EIF_RequestHandle hb = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 11U, &ha), EIF_ACCEPTED); /* ticket 1 */
    TEST_EQ(EIF_SubmitRequest(s_id_b, EIF_REQ_PRINT, NULL, 0U, 22U, &hb), EIF_ACCEPTED); /* ticket 2 */
    g_stub.done(2U, EIF_OUTCOME_FAILED);
    TEST_EQ(s_b.completed, 1);
    TEST_EQ(s_b.token, 22U);
    TEST_EQ(s_a.completed, 0);
    g_stub.done(1U, EIF_OUTCOME_COMPLETED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.handle, ha);
    TEST_EQ(s_a.outcome, EIF_OUTCOME_COMPLETED);
}

/** @brief 先着完了：submit の中で完了が届いても、Submitの戻りまでに要求元へ1回通知される。 */
static void test_early_completion(void)
{
    EIF_RequestHandle h = 0U;
    g_stub.complete_inline = true;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 3U, &h), EIF_ACCEPTED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.handle, h);
}

/** @brief 取消：要求元以外のConnectorからは取り消せない。 */
static void test_cancel_only_by_origin(void)
{
    EIF_RequestHandle h = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 1U, &h), EIF_ACCEPTED);
    TEST_EQ(EIF_CancelRequest(s_id_b, h), EIF_ERR_NOT_FOUND);
    TEST_EQ(EIF_CancelRequest(s_id_a, h), EIF_OK);
    TEST_EQ(g_stub.last_cancel, 1U);
}

/** @brief 情報取得：公開している情報は取得でき、公開していない情報・サイズ違いは拒否される。 */
static void test_get_info(void)
{
    eif_network_settings_t m;
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m)), EIF_OK);
    TEST_ASSERT(strcmp(m.ip, "10.0.0.2") == 0);
    TEST_EQ(EIF_GetInfo(EIF_INFO_JOB, &m, sizeof(m)), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, 3U), EIF_ERR_INVALID_PARAM);
}

/** @brief 設定変更（REF直接）：指定した項目だけ変わり、未指定の項目は現在値のまま保存される。 */
static void test_patch_merges_public_model_in_frame(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 5U;
    memset(&p, 0, sizeof(p));
    p.has_ip = true;
    (void)strcpy(p.ip, "10.0.0.9");
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_NETWORK, &p, sizeof(p), 1U, &h), EIF_OK);
    TEST_EQ(h, 0U);
    TEST_ASSERT(strcmp(g_stub.stored.ip, "10.0.0.9") == 0);
    TEST_ASSERT(g_stub.stored.dhcp);                          /* 未指定は現在値 */
    TEST_ASSERT(strcmp(g_stub.stored.hostname, "mp") == 0);   /* 未指定は現在値 */
}

/** @brief 設定変更（PCL経由）：store() が EIF_ACCEPTED なら追跡され、完了が要求元へ届く。 */
static void test_patch_via_pcl_accessor_is_tracked(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 0U;
    memset(&p, 0, sizeof(p));
    g_stub.store_result = EIF_ACCEPTED; /* 設定保存IFにPCL経由のAccessorをつないだ場合 */
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_NETWORK, &p, sizeof(p), 77U, &h), EIF_ACCEPTED);
    TEST_ASSERT(h != 0U);
    g_stub.done(g_stub.next_ticket, EIF_OUTCOME_COMPLETED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.token, 77U);
}

/** @brief 差分：初回は全件発生、消えたものは解消、変化なしは通知なし。受信フィルタ外には届かない。 */
static void test_alarm_list_diff_in_frame(void)
{
    const char *first[] = { "E1", "E2" };
    const char *second[] = { "E2" };
    push_alarms(first, 2U);
    TEST_EQ(s_a.events, 2);           /* 初回：E1・E2 が発生 */
    TEST_EQ(s_b.events, 0);           /* filterに無いConnectorには届かない */
    push_alarms(second, 1U);
    TEST_EQ(s_a.events, 3);           /* E1 が解消 */
    TEST_EQ(s_a.last_event, EIF_EVT_ERROR_RESOLVED);
    TEST_ASSERT(strcmp(s_a.last_alarm, "E1") == 0);
    push_alarms(second, 1U);
    TEST_EQ(s_a.events, 3);           /* 変化なし */
}

/**
 * @brief 試験の入口。
 * @return 失敗が無ければ0
 */
int main(void)
{
    printf("== Frame tests\n");
    RUN_TEST(setup_none, test_initialize_rejects_incomplete_accessors);
    RUN_TEST(setup_none, test_start_watches_only_supported_info);
    RUN_TEST(setup_started, test_not_exposed_request);
    RUN_TEST(setup_started, test_completion_routed_to_origin);
    RUN_TEST(setup_started, test_early_completion);
    RUN_TEST(setup_started, test_cancel_only_by_origin);
    RUN_TEST(setup_started, test_get_info);
    RUN_TEST(setup_started, test_patch_merges_public_model_in_frame);
    RUN_TEST(setup_started, test_patch_via_pcl_accessor_is_tracked);
    RUN_TEST(setup_started, test_alarm_list_diff_in_frame);
    return TEST_SUMMARY();
}
