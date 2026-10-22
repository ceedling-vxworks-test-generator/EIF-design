/**
 * @file test_core.c
 * @brief コアの単体試験。偽物アダプタだけで動かす（PCL・REFのヘッダーは一切使わない）。
 */
#include <string.h>

#include "eif_api.h"
#include "eif_core_api.h"
#include "eif_test.h"
#include "fake_ports.h"

int g_test_failures = 0;
int g_test_runs = 0;

typedef struct {
    int completed;
    EIF_RequestHandle handle;
    uint64_t token;
    EIF_RequestOutcome outcome;
    int events;
    eif_event_id_t last_event;
    char last_alarm[EIF_ALARM_TEXT_LEN];
} conn_t;

static conn_t s_a;
static conn_t s_b;
static EIF_ConnectorId s_id_a;
static EIF_ConnectorId s_id_b;

static void on_completed(EIF_RequestHandle h, eif_request_id_t r, uint64_t token, EIF_RequestOutcome o, void *ctx)
{
    conn_t *c = (conn_t *)ctx;
    (void)r;
    c->completed++;
    c->handle = h;
    c->token = token;
    c->outcome = o;
}

static void on_event(const EIF_Event *e, void *ctx)
{
    conn_t *c = (conn_t *)ctx;
    c->events++;
    c->last_event = e->event_id;
    memcpy(c->last_alarm, e->payload.alarm.alarm_id, EIF_ALARM_TEXT_LEN);
}

static void setup_none(void)
{
    EIF_Core_ResetForTest();
    fake_reset();
    memset(&s_a, 0, sizeof(s_a));
    memset(&s_b, 0, sizeof(s_b));
}

static void setup_started(void)
{
    static const eif_event_id_t filter[] = { EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED };
    EIF_ConnectorDesc a = { "A", on_completed, on_event, filter, 2U, &s_a };
    EIF_ConnectorDesc b = { "B", on_completed, on_event, NULL, 0U, &s_b };
    setup_none();
    (void)EIF_Core_Initialize(&g_fake_ports);
    (void)EIF_RegisterConnector(&a, &s_id_a);
    (void)EIF_RegisterConnector(&b, &s_id_b);
    (void)EIF_Start();
}

static void push_alarms(const char *const *ids, unsigned n)
{
    eif_alarm_list_t l;
    memset(&l, 0, sizeof(l));
    for (unsigned i = 0U; i < n; i++) {
        (void)strcpy(l.items[i].alarm_id, ids[i]);
    }
    l.count = n;
    g_fake.watch_fn(EIF_INFO_ALARM_LIST, &l, sizeof(l), g_fake.watch_ctx); /* REFアダプタからの変化通知を再現 */
}

/* ---------------------------------------------------------------- */

static void test_initialize_rejects_incomplete_ports(void)
{
    EIF_DrivenPorts p = g_fake_ports;
    p.setting = NULL;
    TEST_EQ(EIF_Core_Initialize(&p), EIF_ERR_INVALID_PARAM);
    TEST_EQ(EIF_Start(), EIF_ERR_UNAVAILABLE);
}

static void test_start_watches_only_supported_info(void)
{
    TEST_EQ(EIF_Core_Initialize(&g_fake_ports), EIF_OK);
    TEST_EQ(EIF_Start(), EIF_OK);
    TEST_EQ(g_fake.watch_calls, 1);
    TEST_ASSERT(g_fake.done != NULL); /* 完了の受け口がつながっている */
}

static void test_not_exposed_request(void)
{
    EIF_RequestHandle h = 7U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_HEAD_CLEANING, NULL, 0U, 1U, &h), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(h, 0U);
}

static void test_completion_routed_to_origin(void)
{
    EIF_RequestHandle ha = 0U;
    EIF_RequestHandle hb = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 11U, &ha), EIF_ACCEPTED); /* ticket 1 */
    TEST_EQ(EIF_SubmitRequest(s_id_b, EIF_REQ_PRINT, NULL, 0U, 22U, &hb), EIF_ACCEPTED); /* ticket 2 */
    g_fake.done(2U, EIF_OUTCOME_FAILED);
    TEST_EQ(s_b.completed, 1);
    TEST_EQ(s_b.token, 22U);
    TEST_EQ(s_a.completed, 0);
    g_fake.done(1U, EIF_OUTCOME_COMPLETED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.handle, ha);
    TEST_EQ(s_a.outcome, EIF_OUTCOME_COMPLETED);
}

static void test_early_completion(void)
{
    EIF_RequestHandle h = 0U;
    g_fake.complete_inline = true;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 3U, &h), EIF_ACCEPTED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.handle, h);
}

static void test_cancel_only_by_origin(void)
{
    EIF_RequestHandle h = 0U;
    TEST_EQ(EIF_SubmitRequest(s_id_a, EIF_REQ_PRINT, NULL, 0U, 1U, &h), EIF_ACCEPTED);
    TEST_EQ(EIF_CancelRequest(s_id_b, h), EIF_ERR_NOT_FOUND);
    TEST_EQ(EIF_CancelRequest(s_id_a, h), EIF_OK);
    TEST_EQ(g_fake.last_cancel, 1U);
}

static void test_get_info(void)
{
    eif_network_settings_t m;
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, sizeof(m)), EIF_OK);
    TEST_ASSERT(strcmp(m.ip, "10.0.0.2") == 0);
    TEST_EQ(EIF_GetInfo(EIF_INFO_JOB, &m, sizeof(m)), EIF_ERR_NOT_EXPOSED);
    TEST_EQ(EIF_GetInfo(EIF_INFO_NETWORK_SETTINGS, &m, 3U), EIF_ERR_INVALID_PARAM);
}

static void test_patch_merges_public_model_in_core(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 5U;
    memset(&p, 0, sizeof(p));
    p.has_ip = true;
    (void)strcpy(p.ip, "10.0.0.9");
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_NETWORK, &p, sizeof(p), 1U, &h), EIF_OK);
    TEST_EQ(h, 0U);
    TEST_ASSERT(strcmp(g_fake.stored.ip, "10.0.0.9") == 0);
    TEST_ASSERT(g_fake.stored.dhcp);                          /* 未指定は現在値 */
    TEST_ASSERT(strcmp(g_fake.stored.hostname, "mp") == 0);   /* 未指定は現在値 */
}

static void test_patch_via_pcl_adapter_is_tracked(void)
{
    eif_network_settings_patch_t p;
    EIF_RequestHandle h = 0U;
    memset(&p, 0, sizeof(p));
    g_fake.store_result = EIF_ACCEPTED; /* 設定保存ポートにPCL経由のアダプタをつないだ場合 */
    TEST_EQ(EIF_PatchSetting(s_id_a, EIF_SETTING_NETWORK, &p, sizeof(p), 77U, &h), EIF_ACCEPTED);
    TEST_ASSERT(h != 0U);
    g_fake.done(g_fake.next_ticket, EIF_OUTCOME_COMPLETED);
    TEST_EQ(s_a.completed, 1);
    TEST_EQ(s_a.token, 77U);
}

static void test_alarm_list_diff_in_core(void)
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

int main(void)
{
    printf("== Core tests (ヘキサゴナル版)\n");
    RUN_TEST(setup_none, test_initialize_rejects_incomplete_ports);
    RUN_TEST(setup_none, test_start_watches_only_supported_info);
    RUN_TEST(setup_started, test_not_exposed_request);
    RUN_TEST(setup_started, test_completion_routed_to_origin);
    RUN_TEST(setup_started, test_early_completion);
    RUN_TEST(setup_started, test_cancel_only_by_origin);
    RUN_TEST(setup_started, test_get_info);
    RUN_TEST(setup_started, test_patch_merges_public_model_in_core);
    RUN_TEST(setup_started, test_patch_via_pcl_adapter_is_tracked);
    RUN_TEST(setup_started, test_alarm_list_diff_in_core);
    return TEST_SUMMARY();
}
