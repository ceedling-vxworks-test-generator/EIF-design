/**
 * @file eif_public_id.h
 * @brief EIFの公開ID（公開要求・公開情報・公開設定・公開イベント）。
 *
 * 全機種共通の値空間とする。値は末尾への追加だけを行い、既存の値は変更しない（ABI）。
 * 機種ごとに有効なIDは製品CFGの表で決まる。
 */
#ifndef EIF_PUBLIC_ID_H
#define EIF_PUBLIC_ID_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 公開要求ID。 */
typedef enum {
    EIF_REQ_INVALID = 0,
    EIF_REQ_PRINT,
    EIF_REQ_TEST_PRINT,
    EIF_REQ_HEAD_CLEANING,
    EIF_REQ_WIPER_MAINTENANCE,
    EIF_REQ_HUMIDIFIER_MAINTENANCE,
    EIF_REQ_INK_INTRODUCTION,
    EIF_REQ_TUBE_AIR_REMOVAL,
    EIF_REQ_CAP_PUMP_CHECK,
    EIF_REQ_PAUSE,
    EIF_REQ_RESUME,
    EIF_REQ_COUNT
} eif_request_id_t;

/** @brief 公開情報ID。 */
typedef enum {
    EIF_INFO_INVALID = 0,
    EIF_INFO_NETWORK_SETTINGS,
    EIF_INFO_PRINT_SETTINGS,
    EIF_INFO_AMBIENT_STATUS,
    EIF_INFO_MAINTENANCE_STATUS,
    EIF_INFO_DEVICE_INFORMATION,
    EIF_INFO_SOFTWARE_INFORMATION,
    EIF_INFO_JOB,
    EIF_INFO_ALARM_LIST,          /**< 発生中のアラーム一覧（差分の導出に使う） */
    EIF_INFO_COUNT
} eif_info_id_t;

/** @brief 公開設定ID。 */
typedef enum {
    EIF_SETTING_INVALID = 0,
    EIF_SETTING_NETWORK,
    EIF_SETTING_PRINT,
    EIF_SETTING_COUNT
} eif_setting_id_t;

/** @brief 公開イベントID。値は32未満とする（Connectorの受信フィルタをビット列で持つため）。 */
typedef enum {
    EIF_EVT_INVALID = 0,
    EIF_EVT_ERROR_RAISED,
    EIF_EVT_ERROR_RESOLVED,
    EIF_EVT_WARNING_RAISED,
    EIF_EVT_WARNING_RESOLVED,
    EIF_EVT_INK_LEVEL_CHANGED,
    EIF_EVT_JOB_PROGRESS,
    EIF_EVT_COUNT
} eif_event_id_t;

#ifdef __cplusplus
}
#endif

#endif /* EIF_PUBLIC_ID_H */
