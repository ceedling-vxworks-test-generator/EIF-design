/**
 * @file ref_adapter.h
 * @brief REFアダプタ（使われる側）。システム情報ポートと設定保存ポートを実装する（設計書_ヘキサゴナル版 9.3、9.4）。
 *
 * 共通処理（ref_adapter.c）と、機種ごとの対応表（models/<MODEL>/）に分かれる。
 */
#ifndef REF_ADAPTER_H
#define REF_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_driven_ports.h"
#include "rim_api_common.h" /* REF製品非依存ヘッダー。対応表（models/）は製品別ヘッダーも使う */

#define EIF_REF_MAX_SOURCES (4U)

/** @brief REFから取得した値（コピーせずにポインタとサイズで渡す）。 */
typedef struct {
    const void *data;
    size_t size;
} EIF_RefView;

/** @brief REFの値 → 公開モデル。 */
typedef bool (*EIF_RefToPublicFn)(const EIF_RefView *src, size_t src_count, void *out_public, size_t out_size);

/** @brief 公開情報ID → REFのID群・通知契機・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_info_id_t info_id;
    uint32_t source_ids[EIF_REF_MAX_SOURCES];
    size_t source_sizes[EIF_REF_MAX_SOURCES];
    size_t source_count;
    size_t public_size;
    RI_NOTIFICATION_TRIGGER trigger;  /**< 変化通知／定期通知 */
    uint32_t interval_ms;
    EIF_RefToPublicFn to_public;
} EIF_RefInfoMap;

/**
 * @brief マージ済みの公開モデル → REFの型。
 * @param current_public 現在値（公開モデル）。変更できない項目が変わっていないかの確認に使う
 * @param current_ref    現在値（REFの型）。公開モデルに無いREFの項目を保つために使う
 */
typedef bool (*EIF_RefFromPublicFn)(const void *merged_public, const void *current_public, const void *current_ref,
                                    void *out_ref, size_t ref_size);

/** @brief 公開設定ID → 書込み先のREF ID・変換（機種ごとの対応表の1行）。 */
typedef struct {
    eif_setting_id_t setting_id;
    eif_info_id_t current_info;  /**< 現在値（公開モデル）を取る公開情報 */
    uint32_t ref_data_id;
    size_t ref_size;
    EIF_RefFromPublicFn from_public;
} EIF_RefSettingMap;

/* 機種ごとの対応表（models/<MODEL>/ が定義する） */
extern const EIF_RefInfoMap g_ref_info_map[];
extern const size_t g_ref_info_map_count;
extern const EIF_RefSettingMap g_ref_setting_map[];
extern const size_t g_ref_setting_map_count;

/* ポートの実装 */
extern const EIF_SystemInfoPort g_ref_info_port;
extern const EIF_SettingStorePort g_ref_setting_port;

/** @brief 試験専用。監視の登録を消す。 */
void ref_adapter_reset_for_test(void);

#endif /* REF_ADAPTER_H */
