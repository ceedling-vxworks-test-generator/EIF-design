/**
 * @file eif_setting_merger.c
 * @brief 設定変更（詳細設計書 6.3.3、7.7）。
 *
 * マージは公開モデルに対して行うため、機種によらずFrameで1回だけ書く（設定定義表）。
 * 書込み先（REF直接／PCL経由）は、機種セットアップで設定保存IFにつないだAccessorで決まる。
 */
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"

/* ---------------------------------------------------------------- 公開モデルのマージ関数（機種によらない） */

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    const size_t n = strnlen(src, dst_size - 1U);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static bool merge_network(const void *current, const void *patch, void *out)
{
    const eif_network_settings_patch_t *p = (const eif_network_settings_patch_t *)patch;
    eif_network_settings_t *m = (eif_network_settings_t *)out;

    memcpy(m, current, sizeof(*m)); /* 未指定の項目は現在値のまま */
    if (p->has_dhcp) {
        m->dhcp = p->dhcp;
    }
    if (p->has_ip) {
        copy_text(m->ip, sizeof(m->ip), p->ip);
    }
    if (p->has_netmask) {
        copy_text(m->netmask, sizeof(m->netmask), p->netmask);
    }
    if (p->has_gateway) {
        copy_text(m->gateway, sizeof(m->gateway), p->gateway);
    }
    if (p->has_filtering) {
        m->filtering = p->filtering;
    }
    if (p->has_hostname) {
        copy_text(m->hostname, sizeof(m->hostname), p->hostname);
    }
    return true;
}

/* ---------------------------------------------------------------- 設定定義表（機種によらない） */

typedef struct {
    eif_setting_id_t setting_id;
    eif_info_id_t current_info;   /**< 現在値を取る公開情報 */
    size_t public_size;
    size_t patch_size;
    bool (*merge)(const void *current, const void *patch, void *out);
} setting_def_t;

static const setting_def_t k_settings[] = {
    { EIF_SETTING_NETWORK, EIF_INFO_NETWORK_SETTINGS, sizeof(eif_network_settings_t),
      sizeof(eif_network_settings_patch_t), merge_network },
};

static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER;
static _Alignas(max_align_t) unsigned char s_current[EIF_PUBLIC_MODEL_MAX];
static _Alignas(max_align_t) unsigned char s_merged[EIF_PUBLIC_MODEL_MAX];

EIF_Result EIF_PatchSetting(EIF_ConnectorId origin, eif_setting_id_t setting_id, const void *patch,
                            size_t patch_size, uint64_t origin_token, EIF_RequestHandle *out_handle)
{
    const setting_def_t *d = NULL;
    EIF_Ticket ticket = 0U;
    EIF_Result r;

    if ((patch == NULL) || (out_handle == NULL)) {
        return EIF_ERR_INVALID_PARAM;
    }
    *out_handle = 0U;
    if (!eif_frame_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (eif_registry_get(origin) == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    for (size_t i = 0U; i < sizeof(k_settings) / sizeof(k_settings[0]); i++) {
        if (k_settings[i].setting_id == setting_id) {
            d = &k_settings[i];
            break;
        }
    }
    const EIF_AccessorSet *acc = eif_frame_accessors();
    if ((d == NULL) || !acc->setting->is_supported(setting_id) || !acc->info->is_supported(d->current_info)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    if (patch_size != d->patch_size) {
        return EIF_ERR_INVALID_PARAM;
    }

    /* 現在値の読込み〜保存の区間を排他する（同時PATCHで更新が消えないようにする） */
    eif_mutex_lock(&s_lock);
    r = acc->info->read(d->current_info, s_current, d->public_size);
    if (r == EIF_OK) {
        (void)d->merge(s_current, patch, s_merged);
        r = acc->setting->store(setting_id, s_merged, d->public_size, &ticket);
    }
    eif_mutex_unlock(&s_lock);

    if (r == EIF_ACCEPTED) {
        /* PCL経由のAccessorにつないだ場合：結果は製品制御の完了と同じ経路で要求元へ届く */
        return eif_gateway_track(ticket, origin, EIF_REQ_INVALID, origin_token, out_handle);
    }
    return r;
}
