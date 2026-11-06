/**
 * @file event_mapping.h
 * @brief 外部イベント対応表（詳細設計書 3.1.3 EventMapping）。公開イベントID・終端結果 → WebSocketのイベント種別。
 */
#ifndef API_SERVER_EVENT_MAPPING_H
#define API_SERVER_EVENT_MAPPING_H

#include <stdbool.h>
#include <stddef.h>

#include "eif_api.h"
#include "server_api.h"

/** @brief 外部コンポーネント固有の通知の種別（APIサーバーの ws_event_type_t）。 */
typedef ws_event_type_t ExternalEventType;

/** @brief 外部イベント対応表の1行（イベント通知）。 */
typedef struct {
    eif_event_id_t public_event;       /**< EIFが定義する公開イベントID */
    ExternalEventType external_event;  /**< 外部コンポーネント固有の通知の種別 */
} EventMapping;

/** @brief 外部イベント対応表の1行（完了通知）。終端結果 → 外部の通知の種別。 */
typedef struct {
    EIF_RequestOutcome outcome;        /**< 終端結果 */
    ExternalEventType external_event;  /**< 外部コンポーネント固有の通知の種別 */
} OutcomeMapping;

/** @brief 外部イベント対応表（イベント通知）。表に無い公開イベントは送らない。 */
extern const EventMapping g_event_mapping[];
/** @brief g_event_mapping の行数。 */
extern const size_t g_event_mapping_count;
/** @brief 外部イベント対応表（完了通知）。表に無い終端結果は WS_EVENT_JOB_ERROR にする。 */
extern const OutcomeMapping g_outcome_mapping[];
/** @brief g_outcome_mapping の行数。 */
extern const size_t g_outcome_mapping_count;

/**
 * @brief 公開イベントID → 外部の通知の種別（詳細設計書 4.1.5）。
 * @param[in]  id  公開イベントID
 * @param[out] out 外部の通知の種別（見つかった場合だけ書く）
 * @return 対応表にあれば true（無ければ送らない）
 */
bool event_mapping_to_external(eif_event_id_t id, ExternalEventType *out);

/**
 * @brief 終端結果 → 外部の通知の種別（詳細設計書 4.1.5）。
 * @param[in] outcome 終端結果
 * @return 外部の通知の種別。表に無ければ WS_EVENT_JOB_ERROR
 */
ExternalEventType event_mapping_outcome_to_external(EIF_RequestOutcome outcome);

#endif /* API_SERVER_EVENT_MAPPING_H */
