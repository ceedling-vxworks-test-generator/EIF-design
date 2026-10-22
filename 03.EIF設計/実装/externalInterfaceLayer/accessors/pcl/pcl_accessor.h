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
    eif_request_id_t public_id;
    product_request_id_t product_request;
} EIF_PclRequestMap;

/** @brief 機種ごとの対応表（models/<MODEL>/ が定義する）。 */
extern const EIF_PclRequestMap g_pcl_request_map[];
extern const size_t g_pcl_request_map_count;

/** @brief 製品制御IFの実装。 */
extern const EIF_ProductControlIf g_pcl_control_if;

/** @brief PCLの完了callback（PCL経由の設定書込みAccessorと共用する）。 */
void pcl_accessor_on_completed(product_request_id_t id, uint64_t generation_id,
                              external_request_execution_response_state_t result);

/** @brief PCLの結果コード → EIF_Result。 */
EIF_Result pcl_accessor_map_uc(uc_result_t r);

#endif /* PCL_ACCESSOR_H */
