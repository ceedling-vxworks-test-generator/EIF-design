/**
 * @file eif_product_network.c
 * @brief MiniPrinterのネットワーク設定：情報の表・設定の表と変換関数（現行 rest/settings.c の変換を移設）。
 */
#include <stdio.h>
#include <string.h>

#include "eif_product_internal.h"

/* ---------------------------------------------------------------- 変換の部品 */

static bool ipv4_to_string(const uint8_t a[4], char *out, size_t out_size)
{
    const int n = snprintf(out, out_size, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return (n > 0) && ((size_t)n < out_size);
}

static bool string_to_ipv4(const char *s, size_t s_size, uint8_t out[4])
{
    unsigned v[4];
    char tail;

    if ((s == NULL) || (memchr(s, '\0', s_size) == NULL)) {
        return false;
    }
    if (sscanf(s, "%u.%u.%u.%u%c", &v[0], &v[1], &v[2], &v[3], &tail) != 4) {
        return false;
    }
    for (size_t i = 0U; i < 4U; i++) {
        if (v[i] > 255U) {
            return false;
        }
        out[i] = (uint8_t)v[i];
    }
    return true;
}

static bool mac_to_string(const uint8_t m[6], char *out, size_t out_size)
{
    const int n = snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return (n > 0) && ((size_t)n < out_size);
}

/* ---------------------------------------------------------------- 変換関数 */

bool mp_convert_network_settings(const EIF_SourceView *src, size_t src_count, void *out_public, size_t out_size)
{
    eif_network_settings_t *out = (eif_network_settings_t *)out_public;

    if ((src_count != 3U) || (out_size != sizeof(*out))) {
        return false;
    }
    const RIM_network_info_t *net = (const RIM_network_info_t *)src[0].data;
    const uint8_t *hostname = (const uint8_t *)src[1].data;
    const uint8_t *mac = (const uint8_t *)src[2].data;

    memset(out, 0, sizeof(*out));
    out->dhcp = (net->dhcp != 0U);
    out->filtering = false; /* RIMは未対応 */
    if (!ipv4_to_string(net->ip, out->ip, sizeof(out->ip)) ||
        !ipv4_to_string(net->netmask, out->netmask, sizeof(out->netmask)) ||
        !ipv4_to_string(net->gateway, out->gateway, sizeof(out->gateway)) ||
        !mac_to_string(mac, out->macaddress, sizeof(out->macaddress))) {
        return false;
    }
    /* ホスト名は固定長（終端が無い場合がある）。公開モデル側で終端する */
    const size_t n = (src[1].size < sizeof(out->hostname)) ? src[1].size : (sizeof(out->hostname) - 1U);
    memcpy(out->hostname, hostname, n);
    out->hostname[sizeof(out->hostname) - 1U] = '\0';
    return true;
}

bool mp_merge_network_settings(const void *current, size_t current_size, const void *patch, void *out, size_t out_size)
{
    const eif_network_settings_patch_t *p = (const eif_network_settings_patch_t *)patch;
    RIM_network_info_t *net = (RIM_network_info_t *)out;

    if ((current_size != sizeof(RIM_network_info_t)) || (out_size != sizeof(RIM_network_info_t))) {
        return false;
    }
    if (out != current) {
        memcpy(out, current, out_size); /* 未指定の項目は現在値のまま */
    }
    if (p->has_filtering || p->has_hostname) {
        return false; /* このREF Dataでは変更できない項目 */
    }
    if (p->has_dhcp) {
        net->dhcp = p->dhcp ? 1U : 0U;
    }
    if (p->has_ip && !string_to_ipv4(p->ip, sizeof(p->ip), net->ip)) {
        return false;
    }
    if (p->has_netmask && !string_to_ipv4(p->netmask, sizeof(p->netmask), net->netmask)) {
        return false;
    }
    if (p->has_gateway && !string_to_ipv4(p->gateway, sizeof(p->gateway), net->gateway)) {
        return false;
    }
    return true;
}

/* ---------------------------------------------------------------- 表 */

const EIF_InfoMapEntry g_mp_infos[] = {
    { EIF_INFO_NETWORK_SETTINGS,
      { RI_DATA_NETWORK_INFO, RI_DATA_HOSTNAME, RI_DATA_MACADDRESS },
      { sizeof(RIM_network_info_t), RIM_HOSTNAME_LEN, RIM_MACADDRESS_LEN }, 3U,
      sizeof(eif_network_settings_t), mp_convert_network_settings },
};
const size_t g_mp_info_count = sizeof(g_mp_infos) / sizeof(g_mp_infos[0]);

const EIF_SettingMapEntry g_mp_settings[] = {
    { EIF_SETTING_NETWORK, RI_DATA_NETWORK_INFO, sizeof(RIM_network_info_t),
      sizeof(eif_network_settings_patch_t), mp_merge_network_settings,
      EIF_SINK_REF_DIRECT, PRODUCT_REQUEST_ID_INVALID },
};
const size_t g_mp_setting_count = sizeof(g_mp_settings) / sizeof(g_mp_settings[0]);
