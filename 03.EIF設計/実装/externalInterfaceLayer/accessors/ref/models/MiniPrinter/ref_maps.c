/**
 * @file ref_maps.c
 * @brief MiniPrinterの対応表：公開情報・公開設定 ⇔ REFのID・型（詳細設計書 6.5.2、6.5.3）。
 *
 * REFの製品別ヘッダー（rim_api.h：MiniPrinter）を参照してよいのは、このフォルダだけ。
 * 変換関数は純粋関数とする（登録・送信・排他をしない。入力から出力を作るだけ）。
 */
#include <stdio.h>
#include <string.h>

#include "ref_accessor.h"
#include "rim_api.h"

/* ---------------------------------------------------------------- 変換の部品 */

/**
 * @brief IPv4アドレス（4バイト）→ 文字列（"a.b.c.d"）。
 * @param[in]  a        アドレス
 * @param[out] out      文字列の書込み先
 * @param[in]  out_size out のサイズ
 * @return 書き込めたら true（切り詰めが起きたら false）
 */
static bool ipv4_to_string(const uint8_t a[4], char *out, size_t out_size)
{
    const int n = snprintf(out, out_size, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return (n > 0) && ((size_t)n < out_size);
}

/**
 * @brief 文字列（"a.b.c.d"）→ IPv4アドレス（4バイト）。
 * @param[in]  s      文字列（s_size 以内に終端があること）
 * @param[in]  s_size s の領域のサイズ
 * @param[out] out    アドレス（成功時だけ書く）
 * @return 形式が正しく、各値が0〜255なら true
 */
static bool string_to_ipv4(const char *s, size_t s_size, uint8_t out[4])
{
    unsigned v[4];
    char tail;
    if ((s == NULL) || (memchr(s, '\0', s_size) == NULL) ||
        (sscanf(s, "%u.%u.%u.%u%c", &v[0], &v[1], &v[2], &v[3], &tail) != 4)) {
        return false; /* 終端が無い、形式違い、後ろに余計な文字がある */
    }
    for (size_t i = 0U; i < 4U; i++) {
        if (v[i] > 255U) {
            return false;
        }
        out[i] = (uint8_t)v[i];
    }
    return true;
}

/* ---------------------------------------------------------------- ネットワーク設定 */

bool mp_network_to_public(const EIF_RefView *src, size_t n, void *out_public, size_t size);
bool mp_network_from_public(const void *merged, const void *current_public, const void *current_ref, void *out_ref,
                            size_t ref_size);
bool mp_alarm_list_to_public(const EIF_RefView *src, size_t n, void *out_public, size_t size);

/**
 * @brief ネットワーク設定：REFの値 → 公開モデル（EIF_RefToPublicFn）。
 *
 * @param[in]  src        [0] RIM_network_info_t、[1] ホスト名、[2] MACアドレス（6バイト）
 * @param[in]  n          src の要素数（3であること）
 * @param[out] out_public eif_network_settings_t
 * @param[in]  size       out_public のサイズ
 * @return 変換できたら true
 */
bool mp_network_to_public(const EIF_RefView *src, size_t n, void *out_public, size_t size)
{
    eif_network_settings_t *out = (eif_network_settings_t *)out_public;
    if ((n != 3U) || (size != sizeof(*out))) {
        return false;
    }
    const RIM_network_info_t *net = (const RIM_network_info_t *)src[0].data;
    const uint8_t *m = (const uint8_t *)src[2].data;

    memset(out, 0, sizeof(*out));
    out->dhcp = (net->dhcp != 0U);
    out->filtering = false; /* RIMは未対応 */
    if (!ipv4_to_string(net->ip, out->ip, sizeof(out->ip)) ||
        !ipv4_to_string(net->netmask, out->netmask, sizeof(out->netmask)) ||
        !ipv4_to_string(net->gateway, out->gateway, sizeof(out->gateway))) {
        return false;
    }
    const int w = snprintf(out->macaddress, sizeof(out->macaddress), "%02X:%02X:%02X:%02X:%02X:%02X",
                           m[0], m[1], m[2], m[3], m[4], m[5]);
    if ((w < 0) || ((size_t)w >= sizeof(out->macaddress))) {
        return false;
    }
    /* ホスト名はREFの領域長でコピーし、終端を保証する */
    const size_t hn = (src[1].size < sizeof(out->hostname)) ? src[1].size : (sizeof(out->hostname) - 1U);
    memcpy(out->hostname, src[1].data, hn);
    out->hostname[sizeof(out->hostname) - 1U] = '\0';
    return true;
}

/**
 * @brief ネットワーク設定：マージ済みの公開モデル → REFの型（EIF_RefFromPublicFn）。
 *
 * @param[in]  merged         マージ済みの eif_network_settings_t
 * @param[in]  current_public 現在値の eif_network_settings_t
 * @param[in]  current_ref    現在値の RIM_network_info_t（使わない。out_ref に同じ内容が入っている）
 * @param[out] out_ref        RIM_network_info_t（dhcp・ip・netmask・gateway を上書きする）
 * @param[in]  ref_size       out_ref のサイズ
 * @return 変換できたら true。IPの形式が不正、hostname・filtering を変えようとしたら false
 */
bool mp_network_from_public(const void *merged, const void *current_public, const void *current_ref, void *out_ref,
                            size_t ref_size)
{
    const eif_network_settings_t *m = (const eif_network_settings_t *)merged;
    const eif_network_settings_t *c = (const eif_network_settings_t *)current_public;
    RIM_network_info_t *net = (RIM_network_info_t *)out_ref;
    (void)current_ref;

    if (ref_size != sizeof(*net)) {
        return false;
    }
    /* このREF Dataでは変更できない項目（ホスト名は別のREF Data、フィルタリングはRIM未対応） */
    if ((m->filtering != c->filtering) || (strncmp(m->hostname, c->hostname, sizeof(m->hostname)) != 0)) {
        return false;
    }
    net->dhcp = m->dhcp ? 1U : 0U;
    return string_to_ipv4(m->ip, sizeof(m->ip), net->ip) &&
           string_to_ipv4(m->netmask, sizeof(m->netmask), net->netmask) &&
           string_to_ipv4(m->gateway, sizeof(m->gateway), net->gateway);
}

/* ---------------------------------------------------------------- アラーム一覧 */

/**
 * @brief 内部の異常コード → 公開アラーム（暫定の対応。正式な割当ては別途確定する）。
 * @param[in]  code 内部の異常コード
 * @param[out] out  公開アラーム
 */
static void fault_to_alarm(RIM_FaultCode code, eif_alarm_t *out)
{
    memset(out, 0, sizeof(*out));
    (void)snprintf(out->alarm_id, sizeof(out->alarm_id), "MP%04u", (unsigned)code);
    (void)snprintf(out->alarm_code, sizeof(out->alarm_code), "E%05u", (unsigned)code);
}

/**
 * @brief アラーム一覧：REFの異常情報リスト → 公開アラーム一覧（EIF_RefToPublicFn）。
 *
 * 発生中（RIM_FAULT_STATE_ACTIVE）の異常だけを、EIF_ALARM_LIST_MAX 件まで詰める。
 * 発生／解消の差分は、この一覧をもとにFrameが求める。
 *
 * @param[in]  src        [0] RIM_FaultInfoList
 * @param[in]  n          src の要素数（1であること）
 * @param[out] out_public eif_alarm_list_t
 * @param[in]  size       out_public のサイズ
 * @return 変換できたら true
 */
bool mp_alarm_list_to_public(const EIF_RefView *src, size_t n, void *out_public, size_t size)
{
    eif_alarm_list_t *out = (eif_alarm_list_t *)out_public;
    if ((n != 1U) || (size != sizeof(*out))) {
        return false;
    }
    const RIM_FaultInfoList *list = (const RIM_FaultInfoList *)src[0].data;
    const uint32_t cnt = (list->count < RIM_FAULT_INFO_LIST_NUM) ? list->count : RIM_FAULT_INFO_LIST_NUM;

    memset(out, 0, sizeof(*out));
    for (uint32_t i = 0U; (i < cnt) && (out->count < EIF_ALARM_LIST_MAX); i++) {
        if (list->items[i].state == RIM_FAULT_STATE_ACTIVE) {
            fault_to_alarm(list->items[i].code, &out->items[out->count]);
            out->count++;
        }
    }
    return true;
}

/* ---------------------------------------------------------------- 対応表 */

/** @brief 情報の対応表。1行＝1公開情報。 */
const EIF_RefInfoMap g_ref_info_map[] = {
    { EIF_INFO_NETWORK_SETTINGS,
      { RI_DATA_NETWORK_INFO, RI_DATA_HOSTNAME, RI_DATA_MACADDRESS },
      { sizeof(RIM_network_info_t), RIM_HOSTNAME_LEN, RIM_MACADDRESS_LEN }, 3U,
      sizeof(eif_network_settings_t), RI_TRIGGER_ON_CHANGE, 0U, mp_network_to_public },
    { EIF_INFO_ALARM_LIST,
      { RI_DATA_FAULT_INFO_LIST }, { sizeof(RIM_FaultInfoList) }, 1U,
      sizeof(eif_alarm_list_t), RI_TRIGGER_ON_CHANGE, 0U, mp_alarm_list_to_public },
};
/** @brief 情報の対応表の行数。 */
const size_t g_ref_info_map_count = sizeof(g_ref_info_map) / sizeof(g_ref_info_map[0]);

/** @brief 設定の対応表。1行＝1公開設定。 */
const EIF_RefSettingMap g_ref_setting_map[] = {
    { EIF_SETTING_NETWORK, EIF_INFO_NETWORK_SETTINGS, RI_DATA_NETWORK_INFO, sizeof(RIM_network_info_t),
      mp_network_from_public },
};
/** @brief 設定の対応表の行数。 */
const size_t g_ref_setting_map_count = sizeof(g_ref_setting_map) / sizeof(g_ref_setting_map[0]);
