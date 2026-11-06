/**
 * @file event_mapping.c
 * @brief 外部イベント対応表（詳細設計書 3.1.3）。
 */
#include "event_mapping.h"

/** @brief 外部イベント対応表（イベント通知）。 */
const EventMapping g_event_mapping[] = {
    { EIF_EVT_ERROR_RAISED,   WS_EVENT_ERROR_RAISED },
    { EIF_EVT_ERROR_RESOLVED, WS_EVENT_ERROR_RESOLVED },
};
/** @brief 外部イベント対応表（イベント通知）の行数。 */
const size_t g_event_mapping_count = sizeof(g_event_mapping) / sizeof(g_event_mapping[0]);

/** @brief 外部イベント対応表（完了通知）。上記以外は job error。 */
const OutcomeMapping g_outcome_mapping[] = {
    { EIF_OUTCOME_COMPLETED, WS_EVENT_JOB_COMPLETED },
    { EIF_OUTCOME_CANCELED,  WS_EVENT_JOB_CANCELED },
};
/** @brief 外部イベント対応表（完了通知）の行数。 */
const size_t g_outcome_mapping_count = sizeof(g_outcome_mapping) / sizeof(g_outcome_mapping[0]);

bool event_mapping_to_external(eif_event_id_t id, ExternalEventType *out)
{
    for (size_t i = 0U; i < g_event_mapping_count; i++) {
        if (g_event_mapping[i].public_event == id) {
            *out = g_event_mapping[i].external_event;
            return true;
        }
    }
    return false;
}

ExternalEventType event_mapping_outcome_to_external(EIF_RequestOutcome outcome)
{
    for (size_t i = 0U; i < g_outcome_mapping_count; i++) {
        if (g_outcome_mapping[i].outcome == outcome) {
            return g_outcome_mapping[i].external_event;
        }
    }
    return WS_EVENT_JOB_ERROR; /* 上記以外 */
}
