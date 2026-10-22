/**
 * @file eif_product_internal.h
 * @brief MiniPrinter製品CFGの内部宣言。表の実体と変換関数。
 */
#ifndef EIF_PRODUCT_MINIPRINTER_INTERNAL_H
#define EIF_PRODUCT_MINIPRINTER_INTERNAL_H

#include "eif_profile.h"
#include "rim_api.h" /* REF製品別ヘッダー（MiniPrinter）。製品CFGだけが参照してよい */

/* 表の実体 */
extern const EIF_RequestMapEntry g_mp_requests[];
extern const size_t g_mp_request_count;
extern const EIF_InfoMapEntry g_mp_infos[];
extern const size_t g_mp_info_count;
extern const EIF_SettingMapEntry g_mp_settings[];
extern const size_t g_mp_setting_count;
extern const EIF_SubscriptionEntry g_mp_subscriptions[];
extern const size_t g_mp_subscription_count;

/* 変換関数（純粋関数） */
bool mp_convert_network_settings(const EIF_SourceView *src, size_t src_count, void *out_public, size_t out_size);
bool mp_merge_network_settings(const void *current, size_t current_size, const void *patch, void *out, size_t out_size);
size_t mp_build_fault_events(const EIF_SourceView *now, size_t now_count, const void *prev, size_t prev_size,
                             bool has_prev, EIF_Event *out, size_t max);

/** @brief 内部の異常コード → 公開アラーム（暫定の対応。正式な割当てはQA表で確定する）。 */
void mp_fault_to_alarm(RIM_FaultCode code, eif_alarm_t *out);

#endif /* EIF_PRODUCT_MINIPRINTER_INTERNAL_H */
