/**
 * @file pcl_adapter.h
 * @brief PCLアダプタ（使われる側）。製品制御ポートを実装する（設計書_ヘキサゴナル版 9.2）。
 *
 * 共通処理（pcl_adapter.c）と、機種ごとの対応表（models/<MODEL>/）に分かれる。
 */
#ifndef PCL_ADAPTER_H
#define PCL_ADAPTER_H

#include <stddef.h>

#include "eif_driven_ports.h"
#include "product_control_api.h" /* PCL公開ヘッダー。参照してよいのはPCLアダプタだけ */

/** @brief 公開要求ID → PCL製品要求ID（機種ごとの対応表の1行）。 */
typedef struct {
    eif_request_id_t public_id;
    product_request_id_t product_request;
} EIF_PclRequestMap;

/** @brief 機種ごとの対応表（models/<MODEL>/ が定義する）。 */
extern const EIF_PclRequestMap g_pcl_request_map[];
extern const size_t g_pcl_request_map_count;

/** @brief 製品制御ポートの実装。 */
extern const EIF_ProductControlPort g_pcl_control_port;

/** @brief PCLの完了callback（PCL経由の設定アダプタと共用する）。 */
void pcl_adapter_on_completed(product_request_id_t id, uint64_t generation_id,
                              external_request_execution_response_state_t result);

/** @brief PCLの結果コード → EIF_Result。 */
EIF_Result pcl_adapter_map_uc(uc_result_t r);

#endif /* PCL_ADAPTER_H */
