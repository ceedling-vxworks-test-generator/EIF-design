/**
 * @file ref_maps.c
 * @brief 試験用の機種（TestModel）のREF対応表。製品の対応表ではない（試験用の仮実装）。
 *
 * MiniPrinterの対応表に無い組合せ（定期通知を指定できる公開イベント等）を、REF Accessorの共通処理で
 * 確かめるために使う。REFのIDは、手元にあるMiniPrinterのREFヘッダーのものを仮に借りる。
 * ここに書いたIDと通知内容は、実機のREFの仕様ではない。
 */
#include <string.h>

#include "ref_accessor.h"
#include "rim_api.h"

/** @brief 試験用の通知内容（仮）。REFが通知に載せる値の代わり。 */
typedef struct {
    uint32_t code;   /**< 仮の識別子 */
    uint32_t value;  /**< 仮の値 */
} TestNotification;

bool tm_network_to_public(const RefView *src, size_t n, void *out_public, size_t size);
bool tm_network_from_public(const void *merged, const void *current_public, const void *current_ref, void *out_ref,
                            size_t ref_size);
bool tm_to_event(eif_event_id_t id, const void *ref_message, EIF_Event *out);

/**
 * @brief 仮の変換関数：REFの値 → 公開モデル（中身は試験で使わないため0で埋める）。
 * @param[in]  src        REFの値
 * @param[in]  n          src の要素数
 * @param[out] out_public 公開モデル
 * @param[in]  size       out_public のサイズ
 * @return 常に true
 */
bool tm_network_to_public(const RefView *src, size_t n, void *out_public, size_t size)
{
    (void)src;
    (void)n;
    memset(out_public, 0, size);
    return true;
}

/**
 * @brief 仮の変換関数：公開モデル → REFの型（何も変えない）。
 * @param[in]  merged         マージ済みの公開モデル
 * @param[in]  current_public 現在値（公開モデル）
 * @param[in]  current_ref    現在値（REFの型）
 * @param[out] out_ref        書き込むREFの値
 * @param[in]  ref_size       out_ref のサイズ
 * @return 常に true
 */
bool tm_network_from_public(const void *merged, const void *current_public, const void *current_ref, void *out_ref,
                            size_t ref_size)
{
    (void)merged;
    (void)current_public;
    (void)current_ref;
    (void)out_ref;
    (void)ref_size;
    return true;
}

/**
 * @brief 仮の変換関数：試験用の通知内容 → 公開イベント。
 *
 * code が 0 の通知は「この公開イベントに当たらない通知」として捨てる（変換関数が false を返す経路の確認用）。
 *
 * @param[in]  id          公開イベントID
 * @param[in]  ref_message TestNotification
 * @param[out] out         公開イベント（payload.alarm.alarm_id に "T<code>"）
 * @return code が 0 でなければ true
 */
bool tm_to_event(eif_event_id_t id, const void *ref_message, EIF_Event *out)
{
    const TestNotification *n = (const TestNotification *)ref_message;
    if (n->code == 0U) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->event_id = id;
    out->payload.alarm.alarm_id[0] = 'T';
    out->payload.alarm.alarm_id[1] = (char)('0' + (int)(n->code % 10U));
    return true;
}

/** @brief 情報の対応表（仮）。 */
const RefInfoMapping g_ref_info_map[] = {
    { EIF_INFO_NETWORK_SETTINGS, { RI_DATA_NETWORK_INFO }, { sizeof(RIM_network_info_t) }, 1U,
      sizeof(eif_network_settings_t), tm_network_to_public },
};
/** @brief 情報の対応表の行数。 */
const size_t g_ref_info_map_count = sizeof(g_ref_info_map) / sizeof(g_ref_info_map[0]);

/** @brief 設定の対応表（仮）。 */
const RefSettingMapping g_ref_setting_map[] = {
    { EIF_SETTING_NETWORK, EIF_INFO_NETWORK_SETTINGS, RI_DATA_NETWORK_INFO, sizeof(RIM_network_info_t),
      tm_network_from_public },
};
/** @brief 設定の対応表の行数。 */
const size_t g_ref_setting_map_count = sizeof(g_ref_setting_map) / sizeof(g_ref_setting_map[0]);

/**
 * @brief イベントの対応表（仮）。
 * - エラーの発生：変化時のみ
 * - インク残量の変化：変化時・定期の両方（定期通知の変換の確認用）
 * - ジョブの進捗：定期のみ（変化時を許さない行の確認用）
 */
const RefEventMapping g_ref_event_map[] = {
    { EIF_EVT_ERROR_RAISED,      RI_DATA_FAULT_INFO_LIST,             true,  false, tm_to_event },
    { EIF_EVT_INK_LEVEL_CHANGED, RI_DATA_SUB_POUCH_SENSER_VALUE_LIST, true,  true,  tm_to_event },
    { EIF_EVT_JOB_PROGRESS,      RI_DATA_PRINT_JOB_INFO_LIST,         false, true,  tm_to_event },
};
/** @brief イベントの対応表の行数。 */
const size_t g_ref_event_map_count = sizeof(g_ref_event_map) / sizeof(g_ref_event_map[0]);
