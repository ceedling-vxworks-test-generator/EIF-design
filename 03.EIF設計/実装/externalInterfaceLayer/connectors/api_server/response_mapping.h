/**
 * @file response_mapping.h
 * @brief 結果コード対応表（詳細設計書 3.1.2 ResultMapping）。EIF_Result → APIサーバーの結果コード。
 */
#ifndef API_SERVER_RESPONSE_MAPPING_H
#define API_SERVER_RESPONSE_MAPPING_H

#include <stddef.h>

#include "eif_api.h"
#include "server_api.h"

/** @brief 外部コンポーネント固有の結果コード（APIサーバーの uc_result_t）。 */
typedef uc_result_t ExternalResult;

/** @brief 結果コード対応表の1行。 */
typedef struct {
    EIF_Result eif_result;           /**< EIFの結果コード */
    ExternalResult external_result;  /**< 外部コンポーネント固有の結果コード */
} ResultMapping;

/** @brief 結果コード対応表。 */
extern const ResultMapping g_result_mapping[];
/** @brief g_result_mapping の行数。 */
extern const size_t g_result_mapping_count;

/**
 * @brief EIF_Result → 外部の結果コード（詳細設計書 4.1.1）。
 * @param[in] r EIFの結果コード
 * @return 外部の結果コード。表に無ければ UC_ERR_INTERNAL
 */
ExternalResult response_mapping_to_external(EIF_Result r);

#endif /* API_SERVER_RESPONSE_MAPPING_H */
