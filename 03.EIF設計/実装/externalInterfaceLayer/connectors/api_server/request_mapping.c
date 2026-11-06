/**
 * @file request_mapping.c
 * @brief 外部要求対応表（詳細設計書 3.1.1）。APIサーバーのジョブ種別 ⇔ 公開要求ID。
 */
#include "request_mapping.h"

/** @brief 外部要求対応表。どの機種でも同じ。公開範囲は機種ごとの対応表で決まる。 */
const RequestMapping g_request_mapping[] = {
    { JOB_TYPE_PRINT,                  EIF_REQ_PRINT },
    { JOB_TYPE_TEST_PRINT,             EIF_REQ_TEST_PRINT },
    { JOB_TYPE_HEAD_CLEANING,          EIF_REQ_HEAD_CLEANING },
    { JOB_TYPE_WIPER_MAINTENANCE,      EIF_REQ_WIPER_MAINTENANCE },
    { JOB_TYPE_HUMIDIFIER_MAINTENANCE, EIF_REQ_HUMIDIFIER_MAINTENANCE },
    { JOB_TYPE_INK_INTRODUCTION,       EIF_REQ_INK_INTRODUCTION },
    { JOB_TYPE_TUBE_AIR_REMOVAL,       EIF_REQ_TUBE_AIR_REMOVAL },
    { JOB_TYPE_CAP_PUMP_CHECK,         EIF_REQ_CAP_PUMP_CHECK },
};
/** @brief 外部要求対応表の行数。 */
const size_t g_request_mapping_count = sizeof(g_request_mapping) / sizeof(g_request_mapping[0]);

bool request_mapping_to_public(ExternalRequestType external, eif_request_id_t *out)
{
    for (size_t i = 0U; i < g_request_mapping_count; i++) {
        if (g_request_mapping[i].external_request == external) {
            *out = g_request_mapping[i].public_request;
            return true;
        }
    }
    return false;
}

bool request_mapping_to_external(eif_request_id_t public_request, ExternalRequestType *out)
{
    for (size_t i = 0U; i < g_request_mapping_count; i++) {
        if (g_request_mapping[i].public_request == public_request) {
            *out = g_request_mapping[i].external_request;
            return true;
        }
    }
    return false;
}
