/**
 * @file test_ref_accessor_testmodel.c
 * @brief REF Accessorの購読（詳細設計書 3.3.4、3.3.5、4.3.6、4.3.7）を、試験用の機種（TestModel）の対応表で確かめる。
 *
 * MiniPrinterに無い定期通知の購読（RI_TRIGGER_PERIODIC・周期）、通知方法の可否、監視登録表の上限を確認する。
 * 製品制御IFと設定保存IFはスタブAccessor、システム情報IFは本物のREF Accessor（対応表は test/models/TestModel）。
 * RIMの関数はこのファイルで仮実装する。
 */
#include <string.h>

#include "eif_api.h"
#include "eif_frame_api.h"
#include "eif_test.h"
#include "ref_accessor.h"
#include "rim_api.h"
#include "stub_accessors.h"

int g_test_failures = 0;
int g_test_runs = 0;

/** @brief 試験用の通知内容（test/models/TestModel/ref_maps.c と同じ形）。 */
typedef struct {
    uint32_t code;   /**< 仮の識別子（0は「当たらない通知」） */
    uint32_t value;  /**< 仮の値 */
} TestNotification;

/* ================================================================ RIMの仮実装 */

static RI_SUBSCRIPTION_REQUEST s_req[16];  /**< 購読で渡された購読要求 */
static int s_subs;                         /**< 購読の数 */

/** @brief 仮実装：取得は使わない。 @param[in] id Data ID @param[out] out 値 @return RI_NO_DATA */
RIStatus RIM_GetBinary(uint32_t id, RI_BINARY *out)
{
    (void)id;
    (void)out;
    return RI_NO_DATA;
}

/** @brief 仮実装：書込みは使わない。 @param[in] id Data ID @param[in] d 値 @param[in] n サイズ @return RI_NO_DATA */
RIStatus RIM_SetBinary(uint32_t id, const void *d, size_t n)
{
    (void)id;
    (void)d;
    (void)n;
    return RI_NO_DATA;
}

/**
 * @brief 仮実装：購読要求を記録し、購読ID（700から）を払い出す。
 * @param[in]  req    購読要求
 * @param[out] out_id 購読ID
 * @return RI_SUCCESS
 */
RIStatus RIM_Subscribe(const RI_SUBSCRIPTION_REQUEST *req, uint64_t *out_id)
{
    s_req[s_subs] = *req;
    *out_id = 700U + (uint64_t)s_subs;
    s_subs++;
    return RI_SUCCESS;
}

/* ================================================================ 試験用のConnector */

static int s_ev[3];               /**< Connector A・B・C が受け取ったイベントの数 */
static int s_who[3] = { 0, 1, 2 };/**< user_context（どのConnectorか） */

/**
 * @brief 完了callback（使わない）。
 * @param[in] h 要求ハンドル @param[in] r 公開要求ID @param[in] t トークン @param[in] o 終端結果 @param[in] c 使わない
 */
static void on_completed(EIF_RequestHandle h, eif_request_id_t r, uint64_t t, EIF_RequestOutcome o, void *c)
{
    (void)h; (void)r; (void)t; (void)o; (void)c;
}

/** @brief イベントcallback。 @param[in] e 公開イベント @param[in] c どのConnectorか（int*） */
static void on_event(const EIF_Event *e, void *c)
{
    (void)e;
    s_ev[*(const int *)c]++;
}

/** @brief 下位IF一式：システム情報IFだけ本物のREF Accessor。 */
static EIF_AccessorSet s_acc;

/** @brief 前処理：状態を戻し、Frameを初期化する（Connectorの登録は試験が行う）。 */
static void setup(void)
{
    EIF_Frame_ResetForTest();
    ref_accessor_reset_for_test();
    stub_reset();
    memset(s_req, 0, sizeof(s_req));
    memset(s_ev, 0, sizeof(s_ev));
    s_subs = 0;
    s_acc = g_stub_accessors;
    s_acc.info = &g_ref_info_if;
    (void)EIF_Frame_Initialize(&s_acc);
}

/**
 * @brief REF（RIM）からの通知を再現する。
 * @param[in] i    購読の番号（RIM_Subscribe の順）
 * @param[in] code 仮の識別子
 */
static void rim_notify(int i, uint32_t code)
{
    const TestNotification n = { code, 0U };
    s_req[i].callback(700U + (uint64_t)i, &n);
}

/* ---------------------------------------------------------------- 試験 */

/** @brief 対応有無（4.3.7）：対応表に行があり、指定の通知方法が許されていれば true。 */
static void test_is_event_supported(void)
{
    const EIF_SubscriptionRequest err_on = { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U };
    const EIF_SubscriptionRequest err_per = { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_PERIODIC, 1000U };
    const EIF_SubscriptionRequest ink_per = { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U };
    const EIF_SubscriptionRequest job_on = { EIF_EVT_JOB_PROGRESS, EIF_SUBSCRIBE_ON_CHANGE, 0U };
    const EIF_SubscriptionRequest warn = { EIF_EVT_WARNING_RAISED, EIF_SUBSCRIBE_ON_CHANGE, 0U };
    TEST_ASSERT(g_ref_info_if.is_event_supported(&err_on));
    TEST_ASSERT(!g_ref_info_if.is_event_supported(&err_per)); /* allow_periodic＝false */
    TEST_ASSERT(g_ref_info_if.is_event_supported(&ink_per));
    TEST_ASSERT(!g_ref_info_if.is_event_supported(&job_on));  /* allow_on_change＝false */
    TEST_ASSERT(!g_ref_info_if.is_event_supported(&warn));    /* 対応表に無い */
}

/** @brief 購読の変換（4.3.6）：定期は RI_TRIGGER_PERIODIC と周期、変化時は RI_TRIGGER_ON_CHANGE。同じ組合せは1回だけ。 */
static void test_periodic_subscription(void)
{
    static const EIF_SubscriptionRequest p1000[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U } };
    static const EIF_SubscriptionRequest on[] = { { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_ON_CHANGE, 0U } };
    EIF_ConnectorDesc a = { "A", on_completed, on_event, p1000, 1U, &s_who[0] };
    EIF_ConnectorDesc b = { "B", on_completed, on_event, p1000, 1U, &s_who[1] };
    EIF_ConnectorDesc c = { "C", on_completed, on_event, on, 1U, &s_who[2] };
    EIF_ConnectorId id = 0U;
    TEST_EQ(EIF_RegisterConnector(&a, &id), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&b, &id), EIF_OK);
    TEST_EQ(EIF_RegisterConnector(&c, &id), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(s_subs, 2); /* 定期1000ms（A・B）と変化時（C） */
    TEST_EQ(s_req[0].targetType, RI_TARGET_DATA);
    TEST_EQ(s_req[0].targetId, (uint32_t)RI_DATA_SUB_POUCH_SENSER_VALUE_LIST);
    TEST_EQ(s_req[0].trigger, RI_TRIGGER_PERIODIC);
    TEST_EQ(s_req[0].intervalMs, 1000U);
    TEST_EQ(s_req[0].method, RI_METHOD_CALLBACK);
    TEST_EQ(s_req[1].trigger, RI_TRIGGER_ON_CHANGE);
    TEST_EQ(s_req[1].intervalMs, 0U);

    rim_notify(0, 3U); /* 定期の通知 → A・B */
    TEST_EQ(s_ev[0], 1);
    TEST_EQ(s_ev[1], 1);
    TEST_EQ(s_ev[2], 0);
    rim_notify(1, 3U); /* 変化時の通知 → C */
    TEST_EQ(s_ev[2], 1);
    rim_notify(0, 0U); /* 変換関数が false の通知は配らない */
    TEST_EQ(s_ev[0], 1);
}

/** @brief 監視登録表（3.3.5）：上限（8）を超える購読は EIF_ERR_BUSY。指定できない通知方法は EIF_ERR_NOT_EXPOSED。 */
static void test_watch_table_limit(void)
{
    const EIF_SubscriptionRequest ink_per = { EIF_EVT_INK_LEVEL_CHANGED, EIF_SUBSCRIBE_PERIODIC, 1000U };
    const EIF_SubscriptionRequest err_per = { EIF_EVT_ERROR_RAISED, EIF_SUBSCRIBE_PERIODIC, 1000U };
    static int ctx;
    TEST_EQ(g_ref_info_if.watch_event(&err_per, on_event, &ctx), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(g_ref_info_if.watch_event(&ink_per, NULL, &ctx), EIF_ERR_NOT_EXPOSED);
    for (int i = 0; i < 8; i++) {
        TEST_EQ(g_ref_info_if.watch_event(&ink_per, on_event, &ctx), EIF_OK);
    }
    TEST_EQ(g_ref_info_if.watch_event(&ink_per, on_event, &ctx), EIF_ERR_BUSY);
    TEST_EQ(s_subs, 8);
}

/**
 * @brief 試験の入口。
 * @return 失敗が無ければ0
 */
int main(void)
{
    printf("== Integration tests: frame + REF accessor（試験用の機種 TestModel の対応表）\n");
    RUN_TEST(setup, test_is_event_supported);
    RUN_TEST(setup, test_periodic_subscription);
    RUN_TEST(setup, test_watch_table_limit);
    return TEST_SUMMARY();
}
