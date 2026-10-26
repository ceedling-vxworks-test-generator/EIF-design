/**
 * @file eif_public_id.h
 * @brief EIFの公開ID（公開要求・公開情報・公開設定・公開イベント）。
 *
 * 全機種共通の値空間とする。値は末尾への追加だけを行い、既存の値は変更しない（ABI）。
 * 機種ごとに有効なIDは、Accessorの機種ごとの対応表（accessors/pcl|ref/models/<MODEL>/）で決まる。
 */
#ifndef EIF_PUBLIC_ID_H
#define EIF_PUBLIC_ID_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 公開要求ID（PCLの製品要求に対応する）。 */
typedef enum {
    EIF_REQ_INVALID = 0,             /**< 無効値（PCL経由の設定変更の完了通知でも使う） */
    EIF_REQ_PRINT,                   /**< 印刷 */
    EIF_REQ_TEST_PRINT,              /**< テスト印刷 */
    EIF_REQ_HEAD_CLEANING,           /**< ヘッドクリーニング */
    EIF_REQ_WIPER_MAINTENANCE,       /**< ワイパーメンテナンス */
    EIF_REQ_HUMIDIFIER_MAINTENANCE,  /**< 加湿器メンテナンス */
    EIF_REQ_INK_INTRODUCTION,        /**< インク導入 */
    EIF_REQ_TUBE_AIR_REMOVAL,        /**< チューブのエア抜き */
    EIF_REQ_CAP_PUMP_CHECK,          /**< キャップ・ポンプ確認 */
    EIF_REQ_PAUSE,                   /**< 一時停止（取消とは別の要求） */
    EIF_REQ_RESUME,                  /**< 再開 */
    EIF_REQ_COUNT                    /**< 個数（番兵） */
} eif_request_id_t;

/** @brief 公開情報ID（REFが保持するシステム情報のうち、外部へ公開するもの）。 */
typedef enum {
    EIF_INFO_INVALID = 0,            /**< 無効値 */
    EIF_INFO_NETWORK_SETTINGS,       /**< ネットワーク設定（eif_network_settings_t） */
    EIF_INFO_PRINT_SETTINGS,         /**< 印刷設定 */
    EIF_INFO_AMBIENT_STATUS,         /**< 環境状態 */
    EIF_INFO_MAINTENANCE_STATUS,     /**< メンテナンス状態 */
    EIF_INFO_DEVICE_INFORMATION,     /**< 機器情報 */
    EIF_INFO_SOFTWARE_INFORMATION,   /**< ソフトウェア情報 */
    EIF_INFO_JOB,                    /**< ジョブ情報（QA-05） */
    EIF_INFO_ALARM_LIST,             /**< 発生中のアラーム一覧（eif_alarm_list_t。差分の導出に使う） */
    EIF_INFO_COUNT                   /**< 個数（番兵） */
} eif_info_id_t;

/** @brief 公開設定ID（外部から変更できる設定）。 */
typedef enum {
    EIF_SETTING_INVALID = 0,         /**< 無効値 */
    EIF_SETTING_NETWORK,             /**< ネットワーク設定（パッチ：eif_network_settings_patch_t） */
    EIF_SETTING_PRINT,               /**< 印刷設定 */
    EIF_SETTING_COUNT                /**< 個数（番兵） */
} eif_setting_id_t;

/** @brief 公開イベントID。値は32未満とする（Connectorの受信フィルタをビット列で持つため）。 */
typedef enum {
    EIF_EVT_INVALID = 0,             /**< 無効値 */
    EIF_EVT_ERROR_RAISED,            /**< エラー発生（payload.alarm） */
    EIF_EVT_ERROR_RESOLVED,          /**< エラー解消（payload.alarm） */
    EIF_EVT_WARNING_RAISED,          /**< 警告発生（payload.alarm） */
    EIF_EVT_WARNING_RESOLVED,        /**< 警告解消（payload.alarm） */
    EIF_EVT_INK_LEVEL_CHANGED,       /**< インク残量の変化 */
    EIF_EVT_JOB_PROGRESS,            /**< ジョブの進捗（QA-05） */
    EIF_EVT_COUNT                    /**< 個数（番兵） */
} eif_event_id_t;

#ifdef __cplusplus
}
#endif

#endif /* EIF_PUBLIC_ID_H */
