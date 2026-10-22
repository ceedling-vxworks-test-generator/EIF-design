/**
 * @file test_frame.c
 * @brief Frameの単体試験（詳細設計書 12.1）。スタブPortと試験用の製品CFGで動かす。
 */
#include <string.h>

#include "eif_api.h"
#include "eif_frame_api.h"
#include "eif_test.h"
#include "stub_ports.h"

int g_test_failures = 0;
int g_test_runs = 0;

/* ================================================================ 試験用の製品CFG */

#define T_REF_NET   (1U)
#define T_REF_LIST  (2U)

typedef struct { uint8_t dhcp; uint8_t ip0; } t_net_t;
typedef struct { uint32_t count; uint32_t codes[4]; } t_list_t;

static bool t_convert(const EIF_SourceView *src, size_t n, void *out, size_t size)
{
    const t_net_t *net = (const t_net_t *)src[0].data;
    eif_network_settings_t *m = (eif_network_settings_t *)out;
    if ((n != 1U) || (size != sizeof(*m))) {
        return false;
    }
    memset(m, 0, sizeof(*m));
    m->dhcp = (net->dhcp != 0U);
    m->ip[0] = (char)('0' + net->ip0);
    return true;
}

static bool t_merge(const void *cur, size_t cur_size, const void *patch, void *out, size_t out_size)
{
    const eif_network_settings_patch_t *p = (const eif_network_settings_patch_t *)patch;
    t_net_t *net = (t_net_t *)out;
    (void)cur;
    if ((cur_size != sizeof(t_net_t)) || (out_size != sizeof(t_net_t)) || p->has_hostname) {
        return false;
    }
    if (p->has_dhcp) {
        net->dhcp = p->dhcp ? 1U : 0U;
    }
    return true;
}

static bool contains(const t_list_t *l, uint32_t code)
{
    for (uint32_t i = 0U; i < l->count; i++) {
        if (l->codes[i] == code) {
            return true;
        }
    }
    return false;
}

static size_t t_build(const EIF_SourceView *now, size_t n, const void *prev, size_t prev_size, bool has_prev,
                      EIF_Event *out, size_t max)
{
    const t_list_t *cur = (const t_list_t *)now[0].data;
    const t_list_t *old = has_prev ? (const t_list_t *)prev : NULL;
    size_t pos = 0U;
    (void)n;
    (void)prev_size;
    for (uint32_t i = 0U; (i < cur->count) && (pos < max); i++) {
        if ((old == NULL) || !contains(old, cur->codes[i])) {
            out[pos].event_id = EIF_EVT_ERROR_RAISED;
            (void)snprintf(out[pos].payload.alarm.alarm_id, EIF_ALARM_TEXT_LEN, "A%u", (unsigned)cur->codes[i]);
            pos++;
        }
    }
    for (uint32_t i = 0U; (old != NULL) && (i < old->count) && (pos < max); i++) {
        if (!contains(cur, old->codes[i])) {
            out[pos].event_id = EIF_EVT_ERROR_RESOLVED;
            (void)snprintf(out[pos].payload.alarm.alarm_id, EIF_ALARM_TEXT_LEN, "A%u", (unsigned)old->codes[i]);
            pos++;
        }
    }
    return pos;
}

static const EIF_RequestMapEntry t_requests[] = {
    { EIF_REQ_PRINT, PRODUCT_REQUEST_ID_PRINT },
    { EIF_REQ_HEAD_CLEANING, PRODUCT_REQUEST_ID_NORMAL_PURGE_ALL },
};
static const EIF_InfoMapEntry t_infos[] = {
    { EIF_INFO_NETWORK_SETTINGS, { T_REF_NET }, { sizeof(t_net_t) }, 1U, sizeof(eif_network_settings_t), t_convert },
};
static const EIF_SettingMapEntry t_settings[] = {
    { EIF_SETTING_NETWORK, T_REF_NET, sizeof(t_net_t), sizeof(eif_network_settings_patch_t), t_merge,
      EIF_SINK_REF_DIRECT, PRODUCT_REQUEST_ID_INVALID },
};
static const EIF_SubscriptionEntry t_subs[] = {
    { RI_TARGET_DATA, T_REF_LIST, RI_TRIGGER_ON_CHANGE, 0U, { T_REF_LIST }, { sizeof(t_list_t) }, 1U, true, t_build },
};
static const EIF_ProductProfile t_profile = {
    "Test", t_requests, 2U, t_infos, 1U, t_settings, 1U, t_subs, 1U,
};

/* ================================================================ 試験用のConnector */

typedef struct {
    int completed;
    EIF_RequestHandle handle;
    eif_request_id_t request_id;
    uint64_t token;
    EIF_RequestOutcome outcome;
    int events;
    eif_event_id_t last_event;
    char last_alarm[EIF_ALARM_TEXT_LEN];
} t_conn_t;

static t_conn_t s_a;
static t_conn_t s_b;
static EIF_ConnectorId s_id_a;
static EIF_ConnectorId s_id_b;

static void t_on_completed(EIF_RequestHandle h, eif_request_id_t r, uint64_t token, EIF_RequestOutcome o, void *ctx)
{
    t_conn_t *c = (t_conn_t *)ctx;
    c->completed++;
    c->handle = h;
    c->request_id = r;
    c->token = token;
    c->outcome = o;
}

static void t_on_event(const EIF_Event *e, void *ctx)
{
    t_conn_t *c = (t_conn_t *)ctx;
    c->events++;
    c->last_event = e->event_id;
    memcpy(c->last_alarm, e->payload.alarm.alarm_id, EIF_ALARM_TEXT_LEN);
}

static void register_two_connectors(void)
{
    static const eif_event_id_t filter_a[] = { EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED };
    EIF_ConnectorDesc a = { "A", t_on_completed, t_on_event, filter_a, 2U, &s_a };
    EIF_ConnectorDesc b = { "B", t_on_completed, t_on_event, NULL, 0U, &s_b }; /* イベントを受け取らない */
    (void)EIF_RegisterConnector(&a, &s_id_a);
    (void)EIF_RegisterConnector(&b, &s_id_b);
}

/* ================================================================ setup */

static void setup_none(void)
{
    EIF_Frame_ResetForTest();
    stub_ports_reset();
    memset(&s_a, 0, sizeof(s_a));
    memset(&s_b, 0, sizeof(s_b));
}

static void setup_started(void)
{
    const t_net_t net = { 1U, 7U };
    const t_list_t list = { 0U, { 0U } };
    setup_none();
    stub_ref_put(T_REF_NET, &net, sizeof(net));
    stub_ref_put(T_REF_LIST, &list, sizeof(list));
    (void)EIF_Initialize(&t_profile, &g_stub_ports);
    register_two_connectors();
    (void)EIF_Start();
}

/* ================================================================ 初期化 */

static void test_init_rejects_duplicated_id(void)
{
    static const EIF_RequestMapEntry dup[] = {
        { EIF_REQ_PRINT, PRODUCT_REQUEST_ID_PRINT },
        { EIF_REQ_PRINT, PRODUCT_REQUEST_ID_WIPE_ALL },
    };
    EIF_ProductProfile p = t_profile;
    p.requests = dup;
    p.request_count = 2U;
    TEST_EQ(EIF_Initialize(&p, &g_stub_ports), EIF_ERR_INVALID_CONFIG);
    TEST_EQ(EIF_Start(), EIF_ERR_UNAVAILABLE);
}

static void test_init_rejects_pcl_sink_until_qa02(void)
{
    EIF_SettingMapEntry s = t_settings[0];
    EIF_ProductProfile p = t_profile;
    s.sink = EIF_SINK_PCL_REQUEST;
    p.settings = &s;
    TEST_EQ(EIF_Initialize(&p, &g_stub_ports), EIF_ERR_INVALID_CONFIG);
}

static void test_start_registers_subscriptions_with_trampoline(void)
{
    TEST_EQ(EIF_Initialize(&t_profile, &g_stub_ports), EIF_OK);
    TEST_EQ(EIF_Initialize(&t_profile, &g_stub_ports), EIF_OK); /* 多重呼出し */
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_stub_ref.subscribe_calls, 1);
    TEST_EQ(g_stub_ref.subs[0].targetId, T_REF_LIST);
    TEST_EQ(g_stub_ref.subs[0].method, RI_METHOD_CALLBACK);
    TEST_ASSERT(g_stub_ref.subs[0].callback != NULL);
}

static void test_register_after_start_is_busy(void)
{
    EIF_ConnectorId id = 0U;
    EIF_ConnectorDesc c = { "late", t_on_completed, NULL, NULL, 0U, &s_a };
    TEST_EQ(EIF_RegisterConnector(&c, &id), EIF_ERR_BUSY);
}

static void test_api_before_start_is_unavailable(void)
{
    EIF_RequestHandle h = 0U;
    eif_network_settings_t m;
    TEST_EQ(EIF_SubmitRequest(1U, EIF_REQ_PRINT, NULL, 0U, 0U, &h), EIF_ERR_UNAVAILABLE);
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m)), EIF_ERR_UNAVAILABLE);
}

/* ================================================================ 要求 */

static void test_submit_not_exposed_does_not_call_pcl(void)
{
    EIF_RequestHandle h = 99U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_CAP_PUMP_CHECK, NULL, 0U, 1U, &h), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(h, 0U);
    TEST_EQ(g_stub_pcl.next_generation, 0U);
}

static void test_submit_maps_public_id_to_product_request(void)
{
    EIF_RequestHandle h = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_HEAD_CLEANING, NULL, 0U, 1U, &h), EIF_ACCEPTED);
    TEST_ASSERT(h != 0U);
    TEST_EQ(g_stub_pcl.last_product_request, PRODUCT_REQUEST_ID_NORMAL_PURGE_ALL);
}

static void test_completion_is_routed_to_origin_even_if_out_of_order(void)
{
    EIF_RequestHandle ha = 0U;
    EIF_RequestHandle hb = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 111U, &ha), EIF_ACCEPTED);     /* generation 1 */
    TEST_EQ(EIF_SubmitRequest(s_id_b, EIF_REQ_HEAD_CLEANING, NULL, 0U, 222U, &hb), EIF_ACCEPTED); /* generation 2 */

    stub_pcl_complete(2U, EXTERNAL_REQUEST_EXECUTION_RESPONSE_FAILED); /* Bが先に終わる */
    TEST_EQ(s_b.completed, 1);
    TEST_EQ(s_a.completed, 0);
    TEST_EQ(s_b.token, 222U);
    TEST_EQ(s_b.handle, hb);
    TEST_EQ(s_b.outcome, EIF_OUTCOME_FAILED);

    stub_pcl_complete(1U, EXTERNAL_REQUEST_EXECUTION_RESPONSE_ACCEPTED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.token, 111U);
    TEST_EQ(s_a.request_id, EIF_REQ_PRINT);
    TEST_EQ(s_a.outcome, EIF_OUTCOME_COMPLETED);
    TEST_EQ(s_b.completed, 1);

    stub_pcl_complete(1U, EXTERNAL_REQUEST_EXECUTION_RESPONSE_ACCEPTED); /* 2回目は通知しない */
    TEST_EQ(s_a.completed, 1);
}

static void test_early_completion_is_delivered_once(void)
{
    EIF_RequestHandle h = 0U;
    g_stub_pcl.complete_inline = true; /* Submitの中で完了が届く */
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 5U, &h), EIF_ACCEPTED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.handle, h);
    TEST_EQ(s_a.token, 5U);
    TEST_EQ(s_a.outcome, EIF_OUTCOME_COMPLETED);
}

static void test_pcl_reject_is_returned_and_not_tracked(void)
{
    EIF_RequestHandle h = 0U;
    EIF_RequestOutcome o;
    g_stub_pcl.submit_result = UC_ERR_BUSY;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 1U, &h), EIF_ERR_BUSY);
    TEST_EQ(h, 0U);
    TEST_EQ(EIF_GetRequestResult(1U, &o), EIF_ERR_NOT_FOUND);
}

static void test_cancel_only_by_origin(void)
{
    EIF_RequestHandle h = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 1U, &h), EIF_ACCEPTED);
    TEST_EQ(EIF_CancelRequest(s_id_b, h), EIF_ERR_NOT_FOUND);
    TEST_EQ(g_stub_pcl.cancel_calls, 0);
    TEST_EQ(EIF_CancelRequest(s_id_a, h), EIF_OK);
    TEST_EQ(g_stub_pcl.last_cancel_generation, 1U);
}

static void test_get_result_delegates_to_pcl(void)
{
    EIF_RequestHandle h = 0U;
    EIF_RequestOutcome o = EIF_OUTCOME_FAILED;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 1U, &h), EIF_ACCEPTED);
    TEST_EQ(EIF_GetRequestResult(h, &o), EIF_OK);
    TEST_EQ(o, EIF_OUTCOME_PENDING);
}

/* ================================================================ 情報・設定 */

static void test_get_info_converts_ref_value(void)
{
    eif_network_settings_t m;
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m)), EIF_OK);
    TEST_ASSERT(m.dhcp);
    TEST_EQ(m.ip[0], '7');
}

static void test_get_info_errors(void)
{
    eif_network_settings_t m;
    TEST_EQ(EIF_GetInfo(EIF_INFO_JOB, &m, sizeof(m)), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m) - 1U), EIF_ERR_INVALID_PARAM);
    g_stub_ref.get_status = RI_NOT_INITIALIZED;
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m)), EIF_ERR_UNAVAILABLE);
}

static void test_patch_keeps_unspecified_fields_and_writes_ref(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 9U;
    size_t size = 0U;
    memset(&p, 0, sizeof(p));
    p.has_dhcp = true;
    p.dhcp = false;
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_NETWORK, &p, sizeof(p), 1U, &h), EIF_OK);
    TEST_EQ(h, 0U);
    TEST_EQ(g_stub_ref.set_calls, 1);
    const t_net_t *net = (const t_net_t *)stub_ref_peek(T_REF_NET, &size);
    TEST_EQ(net->dhcp, 0U);
    TEST_EQ(net->ip0, 7U); /* 未指定項目は現在値のまま */
}

static void test_patch_invalid_value_does_not_write(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 0U;
    memset(&p, 0, sizeof(p));
    p.has_hostname = true;
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_NETWORK, &p, sizeof(p), 1U, &h), EIF_ERR_INVALID_PARAM);
    TEST_EQ(g_stub_ref.set_calls, 0);
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_PRINT, &p, sizeof(p), 1U, &h), EIF_ERR_NOT_EXPOSED);
}

/* ================================================================ 購読・差分 */

static void test_subscription_diff_and_event_filter(void)
{
    const t_list_t first = { 2U, { 10U, 20U } };
    const t_list_t second = { 1U, { 20U } };

    stub_ref_put(T_REF_LIST, &first, sizeof(first));
    stub_ref_notify(0U); /* 初回：前回値なし → 10,20 が発生 */
    TEST_EQ(s_a.events, 2);
    TEST_EQ(s_a.last_event, EIF_EVT_ERROR_RAISED);
    TEST_EQ(s_b.events, 0); /* filterに含めていないConnectorには届かない */

    stub_ref_put(T_REF_LIST, &second, sizeof(second));
    stub_ref_notify(0U); /* 10 が解消 */
    TEST_EQ(s_a.events, 3);
    TEST_EQ(s_a.last_event, EIF_EVT_ERROR_RESOLVED);
    TEST_ASSERT(strcmp(s_a.last_alarm, "A10") == 0);

    stub_ref_notify(0U); /* 変化なし → イベントなし */
    TEST_EQ(s_a.events, 3);
}

static void test_subscription_ref_failure_keeps_previous(void)
{
    const t_list_t first = { 1U, { 10U } };
    stub_ref_put(T_REF_LIST, &first, sizeof(first));
    stub_ref_notify(0U);
    TEST_EQ(s_a.events, 1);
    g_stub_ref.get_status = RI_INTERNAL_ERROR;
    stub_ref_notify(0U);
    TEST_EQ(s_a.events, 1);
    g_stub_ref.get_status = RI_SUCCESS;
    stub_ref_notify(0U); /* 前回値が保たれているので、同じ内容なら発生しない */
    TEST_EQ(s_a.events, 1);
}

int main(void)
{
    printf("== Frame tests (通常版)\n");
    RUN_TEST(setup_none, test_init_rejects_duplicated_id);
    RUN_TEST(setup_none, test_init_rejects_pcl_sink_until_qa02);
    RUN_TEST(setup_none, test_start_registers_subscriptions_with_trampoline);
    RUN_TEST(setup_started, test_register_after_start_is_busy);
    RUN_TEST(setup_none, test_api_before_start_is_unavailable);
    RUN_TEST(setup_started, test_submit_not_exposed_does_not_call_pcl);
    RUN_TEST(setup_started, test_submit_maps_public_id_to_product_request);
    RUN_TEST(setup_started, test_completion_is_routed_to_origin_even_if_out_of_order);
    RUN_TEST(setup_started, test_early_completion_is_delivered_once);
    RUN_TEST(setup_started, test_pcl_reject_is_returned_and_not_tracked);
    RUN_TEST(setup_started, test_cancel_only_by_origin);
    RUN_TEST(setup_started, test_get_result_delegates_to_pcl);
    RUN_TEST(setup_started, test_get_info_converts_ref_value);
    RUN_TEST(setup_started, test_get_info_errors);
    RUN_TEST(setup_started, test_patch_keeps_unspecified_fields_and_writes_ref);
    RUN_TEST(setup_started, test_patch_invalid_value_does_not_write);
    RUN_TEST(setup_started, test_subscription_diff_and_event_filter);
    RUN_TEST(setup_started, test_subscription_ref_failure_keeps_previous);
    return TEST_SUMMARY();
}
