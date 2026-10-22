/**
 * @file eif_result_map.c
 * @brief 境界ごとの結果コード変換（詳細設計書 9章）。変換はこのファイルだけで行う。
 */
#include "eif_internal.h"

EIF_Result eif_map_uc(uc_result_t r)
{
    switch (r) {
    case UC_OK:
        return EIF_ACCEPTED;
    case UC_ERR_INVALID_PARAM:
        return EIF_ERR_INVALID_PARAM;
    case UC_ERR_BUSY:
        return EIF_ERR_BUSY;
    case UC_ERR_NOT_FOUND:
        return EIF_ERR_NOT_FOUND;
    case UC_ERR_SERVER_UNAVAILABLE:
        return EIF_ERR_UNAVAILABLE;
    default:
        return EIF_ERR_INTERNAL;
    }
}

EIF_Result eif_map_ri(RIStatus s)
{
    switch (s) {
    case RI_SUCCESS:
        return EIF_OK;
    case RI_NOT_INITIALIZED:
        return EIF_ERR_UNAVAILABLE;
    case RI_NO_DATA:
        return EIF_ERR_NOT_FOUND;
    case RI_INVALID_PARAMETER: /* EIFが渡すIDは表で検証済み。内部不整合として扱う */
    default:
        return EIF_ERR_INTERNAL;
    }
}

EIF_RequestOutcome eif_map_outcome(external_request_execution_response_state_t s)
{
    switch (s) {
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_ACCEPTED:
        return EIF_OUTCOME_COMPLETED;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_INVALID:
        return EIF_OUTCOME_INVALID;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_NOT_SUPPORTED:
        return EIF_OUTCOME_NOT_SUPPORTED;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_CANCELED:
        return EIF_OUTCOME_CANCELED;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_PENDING:
        return EIF_OUTCOME_PENDING;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_FAILED:
    default:
        return EIF_OUTCOME_FAILED;
    }
}
