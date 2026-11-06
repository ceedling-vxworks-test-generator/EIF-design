/**
 * @file subscription_mapping.h
 * @brief 購読要求定義（詳細設計書 3.1.4 SubscriptionMapping）。
 *
 * 購読要求は登録時に固定されるため、APIサーバーが受け取りうる通知をすべて並べる。
 * WebSocketのクライアントごとの選別は、Connectorの中で行う（詳細設計書 4.1.7）。
 */
#ifndef API_SERVER_SUBSCRIPTION_MAPPING_H
#define API_SERVER_SUBSCRIPTION_MAPPING_H

#include <stddef.h>

#include "eif_api.h"

/** @brief 購読要求定義（EIF_ConnectorDesc::subscriptions に指定する）。 */
extern const EIF_SubscriptionRequest g_subscription_mapping[];
/** @brief g_subscription_mapping の要素数。 */
extern const size_t g_subscription_mapping_count;

#endif /* API_SERVER_SUBSCRIPTION_MAPPING_H */
