/**
 * @file subscription_mapping.c
 * @brief 購読要求定義（詳細設計書 3.1.4）。
 */
#include "subscription_mapping.h"

/** @brief 購読要求定義。異常の発生／解消を変化時に受け取る。 */
const EIF_SubscriptionRequest g_subscription_mapping[] = {
    { .id = EIF_EVT_ERROR_RAISED,   .type = EIF_SUBSCRIBE_ON_CHANGE, .interval_ms = 0U },
    { .id = EIF_EVT_ERROR_RESOLVED, .type = EIF_SUBSCRIBE_ON_CHANGE, .interval_ms = 0U },
};
/** @brief 購読要求定義の要素数。 */
const size_t g_subscription_mapping_count = sizeof(g_subscription_mapping) / sizeof(g_subscription_mapping[0]);
