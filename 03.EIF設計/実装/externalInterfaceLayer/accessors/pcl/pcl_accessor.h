/**
 * @file pcl_accessor.h
 * @brief PCL Accessor。製品制御IFを実装する（詳細設計書 6.5.1）。
 *
 * 共通処理（pcl_accessor.c）と、機種ごとの対応表（models/<MODEL>/）に分かれる。
 */
#ifndef PCL_ACCESSOR_H
#define PCL_ACCESSOR_H

#include <stddef.h>

#include "eif_accessor_if.h"
#include "product_control_api.h" /* PCL公開ヘッダー。参照してよいのはPCL Accessorだけ */

/** @brief 公開要求ID → PCL製品要求ID（機種ごとの対応表の1行）。 */
typedef struct {
    eif_request_id_t public_id;            /**< 公開要求ID */
    product_request_id_t pcl_id;           /**< その機種の製品要求ID */
} PclRequestMapping;

/** @brief 機種ごとの対応表（models/<MODEL>/ が定義する）。表に無い要求は公開しない。 */
extern const PclRequestMapping g_pcl_request_map[];
/** @brief g_pcl_request_map の行数。 */
extern const size_t g_pcl_request_map_count;

/** @brief 製品制御IFの実装（機種セットアップが EIF_AccessorSet::control に指定する）。 */
extern const EIF_ProductControlIf g_pcl_control_if;

/**
 * @brief PCLの完了callback。PCLの結果をEIFの終端結果へ変換し、Frameの完了トランポリンへ渡す。
 *
 * ExternalRequestIngressSubmit() の引数としてPCLへ渡す（PCL経由の設定書込みAccessorと共用する）。
 *
 * @param[in] id            PCLの製品要求ID（使わない）
 * @param[in] generation_id PCLの受付ID（＝チケット）
 * @param[in] result        PCLの終端状態
 *
 * @note PCLの要求処理context [C3] から呼ばれる。タスクを挟まずにFrameへ渡す。
 */
void pcl_accessor_on_completed(product_request_id_t id, uint64_t generation_id,
                              external_request_execution_response_state_t result);

/**
 * @brief PCLの結果コード → EIF_Result。
 * @param[in] r PCLの結果コード
 * @return UC_OK は EIF_ACCEPTED。その他は詳細設計書 9.1 の対応表のとおり
 */
EIF_Result pcl_accessor_map_uc(uc_result_t r);

#endif /* PCL_ACCESSOR_H */
