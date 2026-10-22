/**
 * @file eif_compose.c
 * @brief 組立ての雛形。何も公開しないポートをつなぐ。新機種はこのフォルダと各アダプタの models/ をコピーして作る。
 */
#include "eif_core_api.h"

static bool none_request(eif_request_id_t id) { (void)id; return false; }
static EIF_Result none_submit(eif_request_id_t id, EIF_Ticket *out) { (void)id; (void)out; return EIF_ERR_NOT_EXPOSED; }
static EIF_Result none_cancel(EIF_Ticket t) { (void)t; return EIF_ERR_NOT_FOUND; }
static EIF_Result none_query(EIF_Ticket t, EIF_RequestOutcome *o) { (void)t; (void)o; return EIF_ERR_NOT_FOUND; }
static void none_done(EIF_ControlDoneFn fn) { (void)fn; }
static bool none_info(eif_info_id_t id) { (void)id; return false; }
static size_t none_size(eif_info_id_t id) { (void)id; return 0U; }
static EIF_Result none_read(eif_info_id_t id, void *o, size_t s) { (void)id; (void)o; (void)s; return EIF_ERR_NOT_EXPOSED; }
static EIF_Result none_watch(eif_info_id_t id, EIF_InfoChangedFn f, void *c) { (void)id; (void)f; (void)c; return EIF_ERR_NOT_EXPOSED; }
static bool none_setting(eif_setting_id_t id) { (void)id; return false; }
static EIF_Result none_store(eif_setting_id_t id, const void *m, size_t s, EIF_Ticket *t)
{
    (void)id; (void)m; (void)s; (void)t;
    return EIF_ERR_NOT_EXPOSED;
}

static const EIF_ProductControlPort s_control = { none_request, none_submit, none_cancel, none_query, none_done };
static const EIF_SystemInfoPort s_info = { none_info, none_size, none_read, none_watch };
static const EIF_SettingStorePort s_setting = { none_setting, none_store };

EIF_Result EIF_Setup(void)
{
    static const EIF_DrivenPorts ports = { &s_control, &s_info, &s_setting };
    return EIF_Core_Initialize(&ports);
}
