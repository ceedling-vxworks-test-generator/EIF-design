/**
 * @file test_miniprinter.c
 * @brief MiniPrinter製品CFGの変換関数の単体試験。純粋関数なので、REF・PCLなしで試験できる。
 */
#include <string.h>

#include "eif_product_internal.h"
#include "eif_test.h"

int g_test_failures = 0;
int g_test_runs = 0;

static void setup(void)
{
}

static void test_convert_network_settings(void)
{
    const RIM_network_info_t net = { 1U, { 192U, 168U, 0U, 10U }, { 255U, 255U, 255U, 0U }, { 192U, 168U, 0U, 1U } };
    uint8_t host[RIM_HOSTNAME_LEN];
    const uint8_t mac[RIM_MACADDRESS_LEN] = { 0x00U, 0x1BU, 0xA9U, 0x01U, 0x02U, 0xFFU };
    eif_network_settings_t out;

    memset(host, 0, sizeof(host));
    memcpy(host, "mini-printer", 12U);
    const EIF_SourceView src[3] = { { &net, sizeof(net) }, { host, sizeof(host) }, { mac, sizeof(mac) } };

    TEST_ASSERT(mp_convert_network_settings(src, 3U, &out, sizeof(out)));
    TEST_ASSERT(out.dhcp);
    TEST_ASSERT(strcmp(out.ip, "192.168.0.10") == 0);
    TEST_ASSERT(strcmp(out.netmask, "255.255.255.0") == 0);
    TEST_ASSERT(strcmp(out.gateway, "192.168.0.1") == 0);
    TEST_ASSERT(strcmp(out.hostname, "mini-printer") == 0);
    TEST_ASSERT(strcmp(out.macaddress, "00:1B:A9:01:02:FF") == 0);
}

static void test_merge_network_keeps_unspecified(void)
{
    const RIM_network_info_t cur = { 1U, { 10U, 0U, 0U, 2U }, { 255U, 0U, 0U, 0U }, { 10U, 0U, 0U, 1U } };
    RIM_network_info_t out;
    eif_network_settings_patch_t p;

    memset(&p, 0, sizeof(p));
    p.has_ip = true;
    (void)strcpy(p.ip, "172.16.1.20");
    TEST_ASSERT(mp_merge_network_settings(&cur, sizeof(cur), &p, &out, sizeof(out)));
    TEST_EQ(out.ip[0], 172U);
    TEST_EQ(out.ip[3], 20U);
    TEST_EQ(out.dhcp, 1U);          /* 未指定 */
    TEST_EQ(out.gateway[3], 1U);    /* 未指定 */
}

static void test_merge_network_rejects_invalid(void)
{
    const RIM_network_info_t cur = { 0U, { 0U }, { 0U }, { 0U } };
    RIM_network_info_t out;
    eif_network_settings_patch_t p;

    memset(&p, 0, sizeof(p));
    p.has_ip = true;
    (void)strcpy(p.ip, "300.1.1.1");
    TEST_ASSERT(!mp_merge_network_settings(&cur, sizeof(cur), &p, &out, sizeof(out)));
    memset(&p, 0, sizeof(p));
    p.has_filtering = true; /* RIMは未対応 */
    TEST_ASSERT(!mp_merge_network_settings(&cur, sizeof(cur), &p, &out, sizeof(out)));
}

static void test_fault_events_initial_and_diff(void)
{
    RIM_FaultInfoList prev;
    RIM_FaultInfoList now;
    EIF_Event ev[8];
    eif_alarm_t expect;

    memset(&now, 0, sizeof(now));
    now.count = 2U;
    now.items[0].code = (RIM_FaultCode)1;
    now.items[0].state = RIM_FAULT_STATE_ACTIVE;
    now.items[1].code = (RIM_FaultCode)2;
    now.items[1].state = RIM_FAULT_STATE_HEALED; /* 解消済みは発生として扱わない */
    EIF_SourceView v = { &now, sizeof(now) };

    TEST_EQ(mp_build_fault_events(&v, 1U, NULL, 0U, false, ev, 8U), 1U);
    TEST_EQ(ev[0].event_id, EIF_EVT_ERROR_RAISED);
    mp_fault_to_alarm((RIM_FaultCode)1, &expect);
    TEST_ASSERT(strcmp(ev[0].payload.alarm.alarm_id, expect.alarm_id) == 0);

    prev = now;
    memset(&now, 0, sizeof(now));
    now.count = 1U;
    now.items[0].code = (RIM_FaultCode)3;
    now.items[0].state = RIM_FAULT_STATE_ACTIVE;
    TEST_EQ(mp_build_fault_events(&v, 1U, &prev, sizeof(prev), true, ev, 8U), 2U);
    TEST_EQ(ev[0].event_id, EIF_EVT_ERROR_RAISED);   /* 3 が発生 */
    TEST_EQ(ev[1].event_id, EIF_EVT_ERROR_RESOLVED); /* 1 が解消 */
}

static void test_tables_are_consistent(void)
{
    TEST_EQ(g_mp_infos[0].source_sizes[0], sizeof(RIM_network_info_t));
    TEST_EQ(g_mp_settings[0].sink, EIF_SINK_REF_DIRECT);
    TEST_EQ(g_mp_subscriptions[0].target_id, (uint32_t)RI_DATA_FAULT_INFO_LIST);
    for (size_t i = 0U; i < g_mp_request_count; i++) {
        TEST_ASSERT(g_mp_requests[i].product_request != PRODUCT_REQUEST_ID_INVALID);
    }
}

int main(void)
{
    printf("== Product CFG tests (MiniPrinter, 通常版)\n");
    RUN_TEST(setup, test_convert_network_settings);
    RUN_TEST(setup, test_merge_network_keeps_unspecified);
    RUN_TEST(setup, test_merge_network_rejects_invalid);
    RUN_TEST(setup, test_fault_events_initial_and_diff);
    RUN_TEST(setup, test_tables_are_consistent);
    return TEST_SUMMARY();
}
