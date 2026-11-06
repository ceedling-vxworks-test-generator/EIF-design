/**
 * @file test_frame.c
 * @brief Frameの単体試験。スタブAccessorだけで動かす（PCL・REFのヘッダーは一切使わない）。
 *
 * 試験用Connector（A：異常の発生／解消を購読、B：購読しない）を登録して確認する。
 * 購読の集約・上限の試験では、C・D も使い、購読要求を試験ごとに変える。
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
static conn_t s_c;              /**< Connector C の記録 */
static conn_t s_d;              /**< Connector D の記録 */
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
    memset(&s_c, 0, sizeof(s_c));
    memset(&s_d, 0, sizeof(s_d));
}

/** @brief 購読要求：異常の発生／解消を変化時に受け取る。 */
static const EIF_SubscriptionRequest k_error_subs[] = {
    { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
    { EIF_EVT_ERROR_RESOLVED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
};

/**
 * @brief 試験用Connectorの登録内容を作る。
 * @param[in] name 名前
 * @param[in] subs 購読要求の配列
 * @param[in] n    subs の要素数
 * @param[in] rec  記録先
 * @return 登録内容
 */
static EIF_ConnectorDesc desc_of(const char *name, const EIF_SubscriptionRequest *subs, size_t n, conn_t *rec)
{
    const EIF_ConnectorDesc d = { name, on_completed, on_event, subs, n, rec };
    return d;
}

/** @brief 前処理：初期化 → Connector A（異常の発生／解消を購読）・B（購読しない）を登録 → 開始 まで済ませる。 */
static void setup_started(void)
{
    EIF_ConnectorDesc a = desc_of("A", k_error_subs, 2U, &s_a);
    EIF_ConnectorDesc b = desc_of("B", NULL, 0U, &s_b);
    setup_none();
    (void)EIF_Frame_Initialize(&g_stub_accessors);
    (void)EIF_RegisterConnector(&a, &s_id_a);
    (void)EIF_RegisterConnector(&b, &s_id_b);
    (void)EIF_Start();
}

/** @brief 前処理：初期化だけ済ませる（Connectorの登録は試験が行う）。 */
static void setup_initialized(void)
{
    setup_none();
    (void)EIF_Frame_Initialize(&g_stub_accessors);
}

/**
 * @brief スタブに記録された購読から、組合せが一致するものを探す。
 * @param[in] id          公開イベントID
 * @param[in] type        通知方法
 * @param[in] interval_ms 周期
 * @return 記録。無ければNULL
 */
static const stub_watch_t *find_watch(eif_event_id_t id, EIF_SubscriptionType type, uint32_t interval_ms)
{
    for (int i = 0; i < g_stub.watch_count; i++) {
        const EIF_SubscriptionRequest *r = &g_stub.watches[i].req;
        if ((r->id == id) && (r->type == type) && (r->interval_ms == interval_ms)) {
            return &g_stub.watches[i];
        }
    }
    return NULL;
}

/**
 * @brief REF Accessorからの通知（変換済みの公開イベント1件）を再現する。
 * @param[in] w        通知する購読（find_watch() の結果）
 * @param[in] id       公開イベントID
 * @param[in] alarm_id アラームID
 */
static void notify(const stub_watch_t *w, eif_event_id_t id, const char *alarm_id)
{
    EIF_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.event_id = id;
    (void)strcpy(ev.payload.alarm.alarm_id, alarm_id);
    w->fn(&ev, w->ctx); /* REFの通知ワーカーのcontextのまま呼ばれる */
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

/** @brief 開始：購読要求が無ければ購読しない。完了トランポリンは渡されている。 */
static void test_start_without_subscriptions(void)
{
    TEST_EQ(EIF_Frame_Initialize(&g_stub_accessors), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub.watch_calls, 0);
    TEST_ASSERT(g_stub.done != NULL); /* 完了の受け口がつながっている */
}

/** @brief 登録時の検査：購読要求の形が不正なら INVALID_PARAM。機種に依存しない形の確認だけを行う。 */
static void test_register_rejects_invalid_subscriptions(void)
{
    static const EIF_SubscriptionRequest bad_id[] = { { EIF_EVT_COUNT, EIF_SUBSCRIBE_ON_CHANGE, 0U } };
    static const EIF_SubscriptionRequest bad_on_change[] = { { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 100U } };
    static const EIF_SubscriptionRequest bad_periodic[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 0U } };
    static const EIF_SubscriptionRequest not_allowed[] = { { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_PERIODIC, 1000U } };
    static const EIF_SubscriptionRequest dup[] = {
        { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
        { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U },
    };
    static const EIF_SubscriptionRequest ok[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U } };
    EIF_ConnectorId id = 0U;
    EIF_ConnectorDesc d;

    d = desc_of("x", NULL, 1U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_ERR_INVALID_PARAM);      /* 配列がNULL */
    d = desc_of("x", bad_id, 1U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_ERR_INVALID_PARAM);      /* 範囲外の公開イベントID */
    d = desc_of("x", bad_on_change, 1U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_ERR_INVALID_PARAM);      /* 変化時に周期 */
    d = desc_of("x", bad_periodic, 1U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_ERR_INVALID_PARAM);      /* 定期で周期0 */
    d = desc_of("x", not_allowed, 1U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_ERR_INVALID_PARAM);      /* エラーの発生は変化時のみ */
    d = desc_of("x", dup, 2U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_ERR_INVALID_PARAM);      /* 同じ公開イベントの重複 */
    TEST_EQ(id, 0U);
    d = desc_of("x", ok, 1U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_OK);
    TEST_EQ(id, 1U);                                                     /* 拒否した分は採番しない */
}

/** @brief 集約：A・Bが同じ組合せを購読しても購読は1回。通知は購読要求を出したConnectorへ登録順に届く。 */
static void test_same_request_watched_once(void)
{
    static const EIF_SubscriptionRequest raised_only[] = { { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U } };
    EIF_ConnectorDesc a = desc_of("A", k_error_subs, 2U, &s_a);
    EIF_ConnectorDesc b = desc_of("B", raised_only, 1U, &s_b);
    TEST_EQ(EIF_RegisterConnector(&a, &s_id_a), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&b, &s_id_b), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub.watch_calls, 2); /* 発生（A・B）と解消（A）の2組合せ */

    const stub_watch_t *raised = find_watch(EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U);
    const stub_watch_t *resolved = find_watch(EIF_EVT_ERROR_RESOLVED, EIF_SUBSCRIBE_ON_CHANGE, 0U);
    TEST_ASSERT((raised != NULL) && (resolved != NULL));
    notify(raised, EIF_EVT_ERROR_RAISED, "E1");
    TEST_EQ(s_a.events, 1);
    TEST_EQ(s_b.events, 1);
    notify(resolved, EIF_EVT_ERROR_RESOLVED, "E1");
    TEST_EQ(s_a.events, 2);
    TEST_EQ(s_a.last_event, EIF_EVT_ERROR_RESOLVED);
    TEST_EQ(s_b.events, 1); /* 解消を購読していないBには届かない */
}

/** @brief 周期違い：同じ公開イベントでも周期が違えば別に購読し、それぞれの購読要求を出したConnectorへだけ届く。 */
static void test_different_interval_watched_separately(void)
{
    static const EIF_SubscriptionRequest p1000[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U } };
    static const EIF_SubscriptionRequest p5000[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 5000U } };
    EIF_ConnectorDesc a = desc_of("A", p1000, 1U, &s_a);
    EIF_ConnectorDesc b = desc_of("B", p5000, 1U, &s_b);
    TEST_EQ(EIF_RegisterConnector(&a, &s_id_a), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&b, &s_id_b), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub.watch_calls, 2);

    const stub_watch_t *w = find_watch(EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U);
    TEST_ASSERT(w != NULL);
    notify(w, EIF_EVT_INK_LEVEL_CHANGED, "-");
    TEST_EQ(s_a.events, 1);
    TEST_EQ(s_b.events, 0);
}

/** @brief 機種が通知しない公開イベント：登録・開始はエラーにせず、購読しない（届かないだけ）。 */
static void test_unsupported_event_not_watched(void)
{
    EIF_ConnectorDesc a = desc_of("A", k_error_subs, 2U, &s_a);
    g_stub.unsupported_events = 1UL << (unsigned)EIF_EVT_ERROR_RESOLVED;
    TEST_EQ(EIF_RegisterConnector(&a, &s_id_a), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub.event_supported_calls, 2);
    TEST_EQ(g_stub.watch_calls, 1);
    TEST_ASSERT(find_watch(EIF_EVT_ERROR_RESOLVED, EIF_SUBSCRIBE_ON_CHANGE, 0U) == NULL);
}

/** @brief 上限：購読表の行数が8を超えたら BUSY。REFへの購読はせず、受付も開始しない。 */
static void test_subscription_table_limit(void)
{
    static const EIF_SubscriptionRequest a_subs[] = {
        { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
        { EIF_EVT_ERROR_RESOLVED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
        { EIF_EVT_WARNING_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
        { EIF_EVT_WARNING_RESOLVED, EIF_SUBSCRIBE_ON_CHANGE, 0U },
        { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U },
    };
    static const EIF_SubscriptionRequest b_subs[] = {
        { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 2000U },
        { EIF_EVT_JOB_PROGRESS, EIF_SUBSCRIBE_ON_CHANGE, 0U },
    };
    static const EIF_SubscriptionRequest c_subs[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 3000U } };
    static const EIF_SubscriptionRequest d_subs[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 4000U } };
    EIF_ConnectorDesc a = desc_of("A", a_subs, 5U, &s_a);
    EIF_ConnectorDesc b = desc_of("B", b_subs, 2U, &s_b);
    EIF_ConnectorDesc c = desc_of("C", c_subs, 1U, &s_c);
    EIF_ConnectorDesc d = desc_of("D", d_subs, 1U, &s_d);
    EIF_ConnectorId id = 0U;
    EIF_RequestHandle h = 0U;
    TEST_EQ(EIF_RegisterConnector(&a, &id), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&b, &id), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&c, &id), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&d, &id), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_ERR_BUSY);  /* 9組合せ */
    TEST_EQ(g_stub.watch_calls, 0);
    TEST_EQ(EIF_SubmitRequest(1U, EIF_REQ_PRINT, NULL, 0U, 1U, &h), EIF_ERR_UNAVAILABLE);
}

/** @brief 購読の失敗：Start は失敗を返して受付を開始しない。原因が直れば再度の Start で購読できる。 */
static void test_watch_failure_then_retry(void)
{
    EIF_ConnectorDesc a = desc_of("A", k_error_subs, 2U, &s_a);
    TEST_EQ(EIF_RegisterConnector(&a, &s_id_a), EIF_OK);
    g_stub.watch_result = EIF_ERR_INTERNAL;
    TEST_EQ(EIF_Start(), EIF_ERR_INTERNAL);
    g_stub.watch_result = EIF_OK;
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub.watch_count, 2);
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

/** @brief 配信：EIFは差分を求めず、届いた通知1件ごとに公開イベントを配る。購読していないConnectorには届かない。 */
static void test_event_delivered_per_notification(void)
{
    const stub_watch_t *raised = find_watch(EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U);
    TEST_ASSERT(raised != NULL);
    notify(raised, EIF_EVT_ERROR_RAISED, "E1");
    TEST_EQ(s_a.events, 1);
    TEST_EQ(s_a.last_event, EIF_EVT_ERROR_RAISED);
    TEST_ASSERT(strcmp(s_a.last_alarm, "E1") == 0);
    TEST_EQ(s_b.events, 0);           /* 購読していないConnectorには届かない */
    notify(raised, EIF_EVT_ERROR_RAISED, "E1");
    TEST_EQ(s_a.events, 2);           /* 前回値と比べない（同じ内容でも届いた分だけ配る） */
    notify(raised, EIF_EVT_ERROR_RESOLVED, "E1");
    TEST_EQ(s_a.events, 2);           /* 購読表の行と公開イベントが一致しなければ捨てる */
}

/**
 * @brief 試験の入口。
 * @return 失敗が無ければ0
 */
int main(void)
{
    printf("== Frame tests\n");
    RUN_TEST(setup_none, test_initialize_rejects_incomplete_accessors);
    RUN_TEST(setup_none, test_start_without_subscriptions);
    RUN_TEST(setup_initialized, test_register_rejects_invalid_subscriptions);
    RUN_TEST(setup_initialized, test_same_request_watched_once);
    RUN_TEST(setup_initialized, test_different_interval_watched_separately);
    RUN_TEST(setup_initialized, test_unsupported_event_not_watched);
    RUN_TEST(setup_initialized, test_subscription_table_limit);
    RUN_TEST(setup_initialized, test_watch_failure_then_retry);
    RUN_TEST(setup_started, test_not_exposed_request);
    RUN_TEST(setup_started, test_completion_routed_to_origin);
    RUN_TEST(setup_started, test_early_completion);
    RUN_TEST(setup_started, test_cancel_only_by_origin);
    RUN_TEST(setup_started, test_get_info);
    RUN_TEST(setup_started, test_patch_merges_public_model_in_frame);
    RUN_TEST(setup_started, test_patch_via_pcl_accessor_is_tracked);
    RUN_TEST(setup_started, test_event_delivered_per_notification);
    return TEST_SUMMARY();
}
