/**
 * @file request_mapping.h
 * @brief 外部要求対応表（詳細設計書 3.1.1 RequestMapping）。APIサーバーのジョブ種別 ⇔ 公開要求ID。
 *
 * 本対応表は機種によらない。その機種で公開しているかは、Frameが判定する（詳細設計書 4.2.1）。
 */
#ifndef API_SERVER_REQUEST_MAPPING_H
#define API_SERVER_REQUEST_MAPPING_H

#include <stdbool.h>
#include <stddef.h>

#include "eif_api.h"
#include "server_api.h"

/** @brief 外部コンポーネント固有の要求識別子（APIサーバーのジョブ種別）。 */
typedef job_type_t ExternalRequestType;

/** @brief 外部要求対応表の1行。 */
typedef struct {
    ExternalRequestType external_request;  /**< 外部コンポーネント固有の要求識別子 */
    eif_request_id_t public_request;       /**< EIFが定義する公開要求ID */
} RequestMapping;

/** @brief 外部要求対応表。 */
extern const RequestMapping g_request_mapping[];
/** @brief g_request_mapping の行数。 */
extern const size_t g_request_mapping_count;

/**
 * @brief 外部要求 → 公開要求ID（詳細設計書 4.1.1、4.1.6）。
 * @param[in]  external 外部要求
 * @param[out] out      公開要求ID（見つかった場合だけ書く）
 * @return 対応表にあれば true（無ければ未対応の外部要求）
 */
bool request_mapping_to_public(ExternalRequestType external, eif_request_id_t *out);

/**
 * @brief 公開要求ID → 外部要求（逆引き。完了通知の変換に使う。詳細設計書 4.1.5）。
 * @param[in]  public_request 公開要求ID
 * @param[out] out            外部要求（見つかった場合だけ書く）
 * @return 対応表にあれば true
 */
bool request_mapping_to_external(eif_request_id_t public_request, ExternalRequestType *out);

#endif /* API_SERVER_REQUEST_MAPPING_H */
