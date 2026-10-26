/**
 * @file eif_model.h
 * @brief EIFの公開モデル（公開情報・公開設定・イベントのデータ型）。
 *
 * 外部IF仕様（OpenAPI／AsyncAPI）のスキーマと同じ形とする。機種に依存しない。
 * 文字列はすべて終端（'\0'）付き。
 */
#ifndef EIF_MODEL_H
#define EIF_MODEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IPv4アドレス文字列の長さ（"255.255.255.255"＋終端）。 */
#define EIF_IPV4_STR_LEN     (16U)
/** @brief ホスト名の長さ（終端を含む）。 */
#define EIF_HOSTNAME_LEN     (64U)
/** @brief MACアドレス文字列の長さ（"XX:XX:XX:XX:XX:XX"＋終端）。 */
#define EIF_MACADDR_STR_LEN  (18U)
/** @brief アラームID・アラームコードの長さ（終端を含む）。 */
#define EIF_ALARM_TEXT_LEN   (32U)

/** @brief ネットワーク設定（EIF_INFO_NETWORK_SETTINGS）。 */
typedef struct {
    bool dhcp;                              /**< DHCPを使うか */
    char ip[EIF_IPV4_STR_LEN];              /**< IPv4アドレス（例 "192.168.0.10"） */
    char netmask[EIF_IPV4_STR_LEN];         /**< サブネットマスク */
    char gateway[EIF_IPV4_STR_LEN];         /**< デフォルトゲートウェイ */
    bool filtering;                         /**< フィルタリングの有効／無効 */
    char hostname[EIF_HOSTNAME_LEN];        /**< ホスト名 */
    char macaddress[EIF_MACADDR_STR_LEN];   /**< MACアドレス（読取り専用） */
} eif_network_settings_t;

/** @brief ネットワーク設定の部分更新（EIF_SETTING_NETWORK）。has_* が true の項目だけを更新する。 */
typedef struct {
    bool has_dhcp;                          /**< dhcp を更新するか */
    bool dhcp;                              /**< 新しい値 */
    bool has_ip;                            /**< ip を更新するか */
    char ip[EIF_IPV4_STR_LEN];              /**< 新しい値 */
    bool has_netmask;                       /**< netmask を更新するか */
    char netmask[EIF_IPV4_STR_LEN];         /**< 新しい値 */
    bool has_gateway;                       /**< gateway を更新するか */
    char gateway[EIF_IPV4_STR_LEN];         /**< 新しい値 */
    bool has_filtering;                     /**< filtering を更新するか（機種によっては変更不可） */
    bool filtering;                         /**< 新しい値 */
    bool has_hostname;                      /**< hostname を更新するか（機種によっては変更不可） */
    char hostname[EIF_HOSTNAME_LEN];        /**< 新しい値 */
} eif_network_settings_patch_t;

/** @brief アラーム（エラー・警告の発生／解消イベントのペイロード）。 */
typedef struct {
    char alarm_id[EIF_ALARM_TEXT_LEN];      /**< 公開アラームID（差分のキー） */
    char alarm_code[EIF_ALARM_TEXT_LEN];    /**< 表示用のアラームコード */
} eif_alarm_t;

/** @brief アラーム一覧の最大件数。 */
#define EIF_ALARM_LIST_MAX (20U)

/** @brief 発生中のアラーム一覧（EIF_INFO_ALARM_LIST）。alarm_id を差分のキーとする。 */
typedef struct {
    unsigned count;                         /**< 有効な件数（EIF_ALARM_LIST_MAX 以下） */
    eif_alarm_t items[EIF_ALARM_LIST_MAX];  /**< 先頭から count 件が有効 */
} eif_alarm_list_t;

#ifdef __cplusplus
}
#endif

#endif /* EIF_MODEL_H */
