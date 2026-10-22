/**
 * @file eif_internal.h
 * @brief Frame内部の部品間IF。Frameの外（Connector・製品CFG）からは使わない。
 */
#ifndef EIF_INTERNAL_H
#define EIF_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_frame_api.h"

/* ---------------------------------------------------------------- core */

/** @brief 初期化済みの表（未初期化ならNULL）。 */
const EIF_ProductProfile *eif_core_profile(void);
/** @brief 初期化済みのPort（未初期化ならNULL）。 */
const EIF_Ports *eif_core_ports(void);
/** @brief EIF_Start()済みか。 */
bool eif_core_is_started(void);

/* ---------------------------------------------------------------- connector */

/** @brief 登録済みConnectorの情報。 */
typedef struct {
    EIF_ConnectorId id;
    EIF_ConnectorDesc desc;
    uint32_t event_mask; /**< event_filter をビット列にしたもの */
} eif_connector_t;

/** @brief Connectorの登録を締め切る（EIF_Start()で呼ぶ）。 */
void eif_registry_close(void);
/** @brief IDからConnectorを得る。未登録ならNULL。 */
const eif_connector_t *eif_registry_get(EIF_ConnectorId id);
/** @brief i番目（0始まり）の登録済みConnector。範囲外ならNULL。 */
const eif_connector_t *eif_registry_at(size_t index);
void eif_registry_reset(void);

/* ---------------------------------------------------------------- notify */

/** @brief 公開イベントを、受信フィルタに一致するConnectorへ登録順に配信する。 */
void eif_dispatch_event(const EIF_Event *event);

/* ---------------------------------------------------------------- request */

/** @brief 要求台帳への登録結果。 */
typedef struct {
    bool early_completed;            /**< 先着完了があった */
    EIF_RequestOutcome early_outcome;
} eif_track_result_t;

/** @brief 要求ハンドルを採番する（単調増加、0は払い出さない）。 */
EIF_RequestHandle eif_tracker_next_handle(void);
/** @brief 受付済み要求を台帳へ登録する。満杯なら EIF_ERR_BUSY。 */
EIF_Result eif_tracker_register(uint64_t generation_id, EIF_ConnectorId origin, uint64_t origin_token,
                                eif_request_id_t request_id, EIF_RequestHandle handle, eif_track_result_t *out);
/** @brief 要求ハンドルから受付IDと要求元を引く。 */
bool eif_tracker_find(EIF_RequestHandle handle, uint64_t *out_generation_id, EIF_ConnectorId *out_origin);
/** @brief PCLへ渡す共通の完了トランポリン。 */
void eif_tracker_on_pcl_completed(product_request_id_t product_request_id, uint64_t generation_id,
                                  external_request_execution_response_state_t result);
/** @brief 要求元Connectorへ完了を通知する。 */
void eif_tracker_notify(EIF_ConnectorId origin, EIF_RequestHandle handle, eif_request_id_t request_id,
                        uint64_t origin_token, EIF_RequestOutcome outcome);
void eif_tracker_reset(void);

/**
 * @brief 製品要求をPCLへ送り、台帳へ登録する（要求受付と、PCL経由の設定変更で共用）。
 */
EIF_Result eif_gateway_submit_product(EIF_ConnectorId origin, product_request_id_t product_request,
                                      eif_request_id_t request_id, uint64_t origin_token,
                                      EIF_RequestHandle *out_handle);

/* ---------------------------------------------------------------- info */

/** @brief 指定したREF Data群を取得し、サイズを検証する。 */
EIF_Result eif_info_read_sources(const uint32_t *ids, const size_t *sizes, size_t count, EIF_SourceView *out_views);

/** @brief 購読表をREFへ登録する（EIF_Start()で呼ぶ）。 */
EIF_Result eif_subscription_start(void);
/** @brief REFへ渡す共通の通知トランポリン。 */
void eif_subscription_on_ref_notified(uint64_t subscription_id, const void *notification_message);
void eif_subscription_reset(void);

/* ---------------------------------------------------------------- state cache */

/** @brief 購読エントリにスロットを割り当てる。容量不足ならfalse。 */
bool eif_cache_allocate(size_t entry_index, size_t size);
/** @brief スロットをロックし、前回値を得る。 */
void eif_cache_lock(size_t entry_index, const void **out_prev, size_t *out_size, bool *out_has_prev);
/** @brief 今回値を保存してロックを放す（data=NULLなら保存しない）。 */
void eif_cache_store_and_unlock(size_t entry_index, const void *data, size_t size);
void eif_cache_reset(void);

/* ---------------------------------------------------------------- result map */

EIF_Result eif_map_uc(uc_result_t r);
EIF_Result eif_map_ri(RIStatus s);
EIF_RequestOutcome eif_map_outcome(external_request_execution_response_state_t s);

#endif /* EIF_INTERNAL_H */
