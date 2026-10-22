/**
 * @file fake_ports.c
 * @brief 試験用の偽物アダプタ。
 */
#include <string.h>

#include "fake_ports.h"

fake_t g_fake;

static bool c_is_supported(eif_request_id_t id) { return id == g_fake.supported_request; }

static EIF_Result c_submit(eif_request_id_t id, EIF_Ticket *out)
{
    (void)id;
    if (g_fake.submit_result != EIF_ACCEPTED) {
        return g_fake.submit_result;
    }
    *out = ++g_fake.next_ticket;
    if (g_fake.complete_inline) {
        g_fake.done(*out, EIF_OUTCOME_COMPLETED);
    }
    return EIF_ACCEPTED;
}

static EIF_Result c_cancel(EIF_Ticket t) { g_fake.last_cancel = t; return EIF_OK; }
static EIF_Result c_query(EIF_Ticket t, EIF_RequestOutcome *o) { (void)t; *o = EIF_OUTCOME_PENDING; return EIF_OK; }
static void c_set_done(EIF_ControlDoneFn fn) { g_fake.done = fn; }

static bool i_is_supported(eif_info_id_t id) { return (id == EIF_INFO_NETWORK_SETTINGS) || (id == EIF_INFO_ALARM_LIST); }

static size_t i_size(eif_info_id_t id)
{
    return (id == EIF_INFO_NETWORK_SETTINGS) ? sizeof(eif_network_settings_t) : sizeof(eif_alarm_list_t);
}

static EIF_Result i_read(eif_info_id_t id, void *out, size_t size)
{
    if (g_fake.read_result != EIF_OK) {
        return g_fake.read_result;
    }
    if ((id != EIF_INFO_NETWORK_SETTINGS) || (size != sizeof(g_fake.network))) {
        return EIF_ERR_INTERNAL;
    }
    memcpy(out, &g_fake.network, size);
    return EIF_OK;
}

static EIF_Result i_watch(eif_info_id_t id, EIF_InfoChangedFn fn, void *ctx)
{
    (void)id;
    g_fake.watch_fn = fn;
    g_fake.watch_ctx = ctx;
    g_fake.watch_calls++;
    return EIF_OK;
}

static bool s_is_supported(eif_setting_id_t id) { return id == EIF_SETTING_NETWORK; }

static EIF_Result s_store(eif_setting_id_t id, const void *merged, size_t size, EIF_Ticket *out_ticket)
{
    (void)id;
    g_fake.store_calls++;
    memcpy(&g_fake.stored, merged, size);
    if (g_fake.store_result == EIF_ACCEPTED) {
        *out_ticket = ++g_fake.next_ticket;
    }
    return g_fake.store_result;
}

static const EIF_ProductControlPort s_control = { c_is_supported, c_submit, c_cancel, c_query, c_set_done };
static const EIF_SystemInfoPort s_info = { i_is_supported, i_size, i_read, i_watch };
static const EIF_SettingStorePort s_setting = { s_is_supported, s_store };
const EIF_DrivenPorts g_fake_ports = { &s_control, &s_info, &s_setting };

void fake_reset(void)
{
    memset(&g_fake, 0, sizeof(g_fake));
    g_fake.supported_request = EIF_REQ_PRINT;
    g_fake.submit_result = EIF_ACCEPTED;
    g_fake.read_result = EIF_OK;
    g_fake.store_result = EIF_OK;
    g_fake.network.dhcp = true;
    (void)strcpy(g_fake.network.ip, "10.0.0.2");
    (void)strcpy(g_fake.network.hostname, "mp");
}
