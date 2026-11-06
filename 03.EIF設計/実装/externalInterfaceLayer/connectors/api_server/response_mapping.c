/**
 * @file response_mapping.c
 * @brief 結果コード対応表（詳細設計書 3.1.2）。
 */
#include "response_mapping.h"

/** @brief 結果コード対応表。表に無い結果コードは UC_ERR_INTERNAL にする。 */
const ResultMapping g_result_mapping[] = {
    { EIF_OK,                UC_OK },
    { EIF_ACCEPTED,          UC_OK },
    { EIF_ERR_INVALID_PARAM, UC_ERR_INVALID_PARAM },
    { EIF_ERR_NOT_EXPOSED,   UC_ERR_NOT_IMPLEMENTED },
    { EIF_ERR_NOT_FOUND,     UC_ERR_NOT_FOUND },
    { EIF_ERR_BUSY,          UC_ERR_BUSY },
    { EIF_ERR_UNAVAILABLE,   UC_ERR_SERVER_UNAVAILABLE },
};
/** @brief 結果コード対応表の行数。 */
const size_t g_result_mapping_count = sizeof(g_result_mapping) / sizeof(g_result_mapping[0]);

ExternalResult response_mapping_to_external(EIF_Result r)
{
    for (size_t i = 0U; i < g_result_mapping_count; i++) {
        if (g_result_mapping[i].eif_result == r) {
            return g_result_mapping[i].external_result;
        }
    }
    return UC_ERR_INTERNAL; /* その他 */
}
