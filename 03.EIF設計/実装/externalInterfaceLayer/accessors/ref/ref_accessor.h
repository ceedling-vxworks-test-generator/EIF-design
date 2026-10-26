/**
 * @file ref_accessor.h
 * @brief REF Accessor。システム情報IFと設定保存IFを実装する（詳細設計書 6.5.2、6.5.3）。
 *
 * 共通処理（ref_accessor.c）と、機種ごとの対応表（models/<MODEL>/）に分かれる。
 */
#ifndef REF_ACCESSOR_H
#define REF_ACCESSOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_accessor_if.h"
#include "rim_api_common.h" /* REF製品非依存ヘッダー。対応表（models/）は製品別ヘッダーも使う */

/** @brief 1つの公開情報が参照できるREFのIDの最大数。 */
#define EIF_REF_MAX_SOURCES (4U)

/** @brief REFから取得した値（コピーせずにポインタとサイズで渡す）。 */
typedef struct {
    const void *data;  /**< RIM_GetBinary() が返した領域（有効期間はQA-10） */
    size_t size;       /**< data のサイズ */
} EIF_RefView;

/**
 * @brief 変換関数：REFの値 → 公開モデル（機種ごとの対応表が持つ）。
 *
 * @param[in]  src        対応表の source_ids の順に取得したREFの値
 * @param[in]  src_count  src の要素数
 * @param[out] out_public 公開モデルの書込み先
 * @param[in]  out_size   out_public のサイズ
 * @return 変換できたら true（false なら out_public の内容は保証しない）
 */
typedef bool (*EIF_RefToPublicFn)(const EIF_RefView *src, size_t src_count, void *out_public, size_t out_size);

/** @brief 公開情報ID → REFのID群・通知契機・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_info_id_t info_id;                     /**< 公開情報ID */
    uint32_t source_ids[EIF_REF_MAX_SOURCES];  /**< 取得するREFのData ID（先頭が購読の対象） */
    size_t source_sizes[EIF_REF_MAX_SOURCES];  /**< REFの型のsizeof（型の不一致を検出する） */
    size_t source_count;                       /**< 使う source_ids の数 */
    size_t public_size;                        /**< 公開モデルのsizeof */
    RI_NOTIFICATION_TRIGGER trigger;           /**< 変化通知／定期通知 */
    uint32_t interval_ms;                      /**< 定期通知の周期（変化通知なら0） */
    EIF_RefToPublicFn to_public;               /**< 変換関数 */
} EIF_RefInfoMap;

/**
 * @brief 変換関数：マージ済みの公開モデル → REFの型（機種ごとの対応表が持つ）。
 *
 * @param[in]  merged_public  マージ済みの公開モデル
 * @param[in]  current_public 現在値（公開モデル）。変更できない項目が変わっていないかの確認に使う
 * @param[in]  current_ref    現在値（REFの型）。公開モデルに無いREFの項目を保つために使う
 * @param[out] out_ref        書き込むREFの値（呼出し前に current_ref のコピーが入っている）
 * @param[in]  ref_size       out_ref のサイズ
 * @return 変換できたら true。値が不正、変更できない項目の変更なら false
 */
typedef bool (*EIF_RefFromPublicFn)(const void *merged_public, const void *current_public, const void *current_ref,
                                    void *out_ref, size_t ref_size);

/** @brief 公開設定ID → 書込み先のREF ID・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_setting_id_t setting_id;      /**< 公開設定ID */
    eif_info_id_t current_info;       /**< 現在値（公開モデル）を取る公開情報 */
    uint32_t ref_data_id;             /**< 書込み先のREF Data ID */
    size_t ref_size;                  /**< REFの型のsizeof */
    EIF_RefFromPublicFn from_public;  /**< 変換関数 */
} EIF_RefSettingMap;

/* ---------------------------------------------------------------- 機種ごとの対応表（models/<MODEL>/ が定義する） */

/** @brief 情報の対応表。表に無い公開情報は公開しない。 */
extern const EIF_RefInfoMap g_ref_info_map[];
/** @brief g_ref_info_map の行数。 */
extern const size_t g_ref_info_map_count;
/** @brief 設定の対応表。表に無い公開設定は公開しない。 */
extern const EIF_RefSettingMap g_ref_setting_map[];
/** @brief g_ref_setting_map の行数。 */
extern const size_t g_ref_setting_map_count;

/* ---------------------------------------------------------------- 下位IFの実装 */

/** @brief システム情報IFの実装（EIF_AccessorSet::info に指定する）。 */
extern const EIF_SystemInfoIf g_ref_info_if;
/** @brief 設定保存IFの実装：REFへ直接書き込む（EIF_AccessorSet::setting に指定する）。 */
extern const EIF_SettingStoreIf g_ref_setting_if;

/** @brief 試験専用。監視の登録表を空にする（REF側の購読は解除しない）。 */
void ref_accessor_reset_for_test(void);

#endif /* REF_ACCESSOR_H */
