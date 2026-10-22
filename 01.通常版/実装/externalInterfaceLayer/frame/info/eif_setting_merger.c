/**
 * @file eif_setting_merger.c
 * @brief 設定変更（詳細設計書 7.7）。書込み先は設定の表の sink 列で切り替える。
 *
 * 読込み〜書込みの区間は排他する（同時PATCHで更新が消えないようにする）。
 * 設定変更は頻度が低いため、排他は設定IDごとではなく1つのmutexで行う。
 */
#include <string.h>

#include "eif_internal.h"
#include "util/eif_lock.h"
#include "util/eif_log.h"

static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER;
static unsigned char s_work[EIF_SETTING_WORK_BYTES];

static EIF_Result patch_ref_direct(const EIF_SettingMapEntry *e, const void *patch)
{
    EIF_SourceView current;
    EIF_Result r;

    eif_mutex_lock(&s_lock);
    r = eif_info_read_sources(&e->ref_data_id, &e->ref_size, 1U, &current);
    if (r == EIF_OK) {
        memcpy(s_work, current.data, e->ref_size); /* 現在値を作業用バッファへ写してから反映する */
        if (!e->merge(current.data, e->ref_size, patch, s_work, e->ref_size)) {
            r = EIF_ERR_INVALID_PARAM;
        } else {
            r = eif_map_ri(eif_core_ports()->setting->set_binary(e->ref_data_id, s_work, e->ref_size));
        }
    }
    eif_mutex_unlock(&s_lock);
    return r;
}

EIF_Result EIF_PatchSetting(EIF_ConnectorId origin, eif_setting_id_t setting_id, const void *patch,
                            size_t patch_size, uint64_t origin_token, EIF_RequestHandle *out_handle)
{
    (void)origin_token;

    if ((patch == NULL) || (out_handle == NULL)) {
        return EIF_ERR_INVALID_PARAM;
    }
    *out_handle = 0U;
    if (!eif_core_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (eif_registry_get(origin) == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }

    const EIF_ProductProfile *p = eif_core_profile();
    const EIF_SettingMapEntry *e = NULL;
    for (size_t i = 0U; i < p->setting_count; i++) {
        if (p->settings[i].public_id == setting_id) {
            e = &p->settings[i];
            break;
        }
    }
    if (e == NULL) {
        return EIF_ERR_NOT_EXPOSED;
    }
    if (patch_size != e->patch_size) {
        return EIF_ERR_INVALID_PARAM;
    }

    switch (e->sink) {
    case EIF_SINK_REF_DIRECT:
        return patch_ref_direct(e, patch);
    case EIF_SINK_PCL_REQUEST:
    default:
        /* 表の検証で拒否済み（QA-02の対応後に eif_gateway_submit_product() で送る） */
        return EIF_ERR_INTERNAL;
    }
}
