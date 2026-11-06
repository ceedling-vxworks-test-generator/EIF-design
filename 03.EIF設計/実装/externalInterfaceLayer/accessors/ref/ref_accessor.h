/**
 * @file ref_accessor.h
 * @brief REF Accessor。システム情報IFと設定保存IFを実装する（詳細設計書 3.3.2〜3.3.5、4.3.6、4.3.7）。
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
} RefView;

/**
 * @brief 変換関数：REFの値 → 公開モデル（機種ごとの対応表が持つ）。
 *
 * @param[in]  src        対応表の ref_ids の順に取得したREFの値
 * @param[in]  src_count  src の要素数
 * @param[out] out_public 公開モデルの書込み先
 * @param[in]  out_size   out_public のサイズ
 * @return 変換できたら true（false なら out_public の内容は保証しない）
 */
typedef bool (*ToPublicFunction)(const RefView *src, size_t src_count, void *out_public, size_t out_size);

/** @brief 公開情報ID → REFのID群・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_info_id_t public_info_id;              /**< 公開情報ID */
    uint32_t ref_ids[EIF_REF_MAX_SOURCES];     /**< 取得するREFのData ID */
    size_t ref_sizes[EIF_REF_MAX_SOURCES];     /**< REFの型のsizeof（型の不一致を検出する） */
    size_t ref_count;                          /**< 使う ref_ids の数 */
    size_t public_size;                        /**< 公開モデルのsizeof */
    ToPublicFunction convert_func;            /**< 変換関数：REFの型 → 公開モデル */
} RefInfoMapping;

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
typedef bool (*FromPublicFunction)(const void *merged_public, const void *current_public, const void *current_ref,
                                    void *out_ref, size_t ref_size);

/** @brief 公開設定ID → 書込み先のREF ID・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_setting_id_t public_setting_id;  /**< 公開設定ID */
    eif_info_id_t current_info_id;       /**< 現在値（公開モデル）を取る公開情報ID */
    uint32_t ref_id;                     /**< 書込み先のREF Data ID */
    size_t ref_size;                     /**< REFの型のsizeof */
    FromPublicFunction convert_func;    /**< 変換関数：公開モデル → REFの型（変更不可項目の確認を含む） */
} RefSettingMapping;

/**
 * @brief 変換関数：REFの通知内容 → 公開イベント（機種ごとの対応表が持つ）。
 *
 * @param[in]  id          作る公開イベントID（対応表の行の public_event_id）
 * @param[in]  ref_message REFの通知内容（RIMが on_ref_notified() に渡したもの。呼出し中だけ有効）
 * @param[out] out         公開イベントの書込み先
 * @return 公開イベントを作ったら true。この公開イベントに当たらない通知（例：解消の購読に届いた発生）、
 *         通知内容が不正なら false（Frameへ渡さない）
 */
typedef bool (*ToEventFunction)(eif_event_id_t id, const void *ref_message, EIF_Event *out);

/** @brief 公開イベントID → 購読するREFの通知・指定できる通知方法・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_event_id_t public_event_id;  /**< 公開イベントID */
    uint32_t ref_source_id;          /**< 購読するREFの通知（REFのData ID） */
    bool allow_on_change;            /**< 変化時の通知を指定できる */
    bool allow_periodic;             /**< 定期通知を指定できる */
    ToEventFunction convert_func;   /**< 変換関数：REFの通知内容 → 公開イベント */
} RefEventMapping;

/* ---------------------------------------------------------------- 機種ごとの対応表（models/<MODEL>/ が定義する） */

/** @brief 情報の対応表。表に無い公開情報は公開しない。 */
extern const RefInfoMapping g_ref_info_map[];
/** @brief g_ref_info_map の行数。 */
extern const size_t g_ref_info_map_count;
/** @brief 設定の対応表。表に無い公開設定は公開しない。 */
extern const RefSettingMapping g_ref_setting_map[];
/** @brief g_ref_setting_map の行数。 */
extern const size_t g_ref_setting_map_count;
/** @brief イベントの対応表。表に無い公開イベントは通知しない。 */
extern const RefEventMapping g_ref_event_map[];
/** @brief g_ref_event_map の行数。 */
extern const size_t g_ref_event_map_count;

/* ---------------------------------------------------------------- 下位IFの実装 */

/** @brief システム情報IFの実装（EIF_AccessorSet::info に指定する）。 */
extern const EIF_SystemInfoIf g_ref_info_if;
/** @brief 設定保存IFの実装：REFへ直接書き込む（EIF_AccessorSet::setting に指定する）。 */
extern const EIF_SettingStoreIf g_ref_setting_if;

/** @brief 試験専用。監視の登録表を空にする（REF側の購読は解除しない）。 */
void ref_accessor_reset_for_test(void);

#endif /* REF_ACCESSOR_H */
