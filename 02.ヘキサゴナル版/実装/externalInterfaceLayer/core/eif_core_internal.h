/**
 * @file eif_core_internal.h
 * @brief コア内部の部品間IF。
 */
#ifndef EIF_CORE_INTERNAL_H
#define EIF_CORE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_core_api.h"

/** @brief 前回値（StateCache）として保持できる合計バイト数。 */
#define EIF_STATE_CACHE_BYTES (4096U)
/** @brief 1回の変化で生成できるイベントの最大数。 */
#define EIF_MAX_EVENTS_PER_NOTIFY (8U)
/** @brief 公開モデルの最大サイズ（作業用バッファ）。 */
#define EIF_PUBLIC_MODEL_MAX (2048U)

/* core */
const EIF_DrivenPorts *eif_core_ports(void);
bool eif_core_is_started(void);

/* connector registry（通常版と同じ） */
typedef struct {
    EIF_ConnectorId id;
    EIF_ConnectorDesc desc;
    uint32_t event_mask;
} eif_connector_t;

void eif_registry_close(void);
const eif_connector_t *eif_registry_get(EIF_ConnectorId id);
const eif_connector_t *eif_registry_at(size_t index);
void eif_registry_reset(void);
void eif_dispatch_event(const EIF_Event *event);

/* request */
EIF_RequestHandle eif_tracker_next_handle(void);
typedef struct {
    bool early_completed;
    EIF_RequestOutcome early_outcome;
} eif_track_result_t;
EIF_Result eif_tracker_register(EIF_Ticket ticket, EIF_ConnectorId origin, uint64_t origin_token,
                                eif_request_id_t request_id, EIF_RequestHandle handle, eif_track_result_t *out);
bool eif_tracker_find(EIF_RequestHandle handle, EIF_Ticket *out_ticket, EIF_ConnectorId *out_origin);
void eif_tracker_on_done(EIF_Ticket ticket, EIF_RequestOutcome outcome);
void eif_tracker_notify(EIF_ConnectorId origin, EIF_RequestHandle handle, eif_request_id_t request_id,
                        uint64_t origin_token, EIF_RequestOutcome outcome);
void eif_tracker_reset(void);
/** @brief チケットを台帳へ登録し、先着完了なら要求元へ通知する（要求受付とPCL経由の設定で共用）。 */
EIF_Result eif_gateway_track(EIF_Ticket ticket, EIF_ConnectorId origin, eif_request_id_t request_id,
                             uint64_t origin_token, EIF_RequestHandle *out_handle);

/* watcher（購読と差分） */
EIF_Result eif_watcher_start(void);
void eif_watcher_reset(void);

/* state cache */
bool eif_cache_allocate(eif_info_id_t id, size_t size);
void eif_cache_lock(eif_info_id_t id, const void **out_prev, bool *out_has_prev);
void eif_cache_store_and_unlock(eif_info_id_t id, const void *data, size_t size);
void eif_cache_reset(void);

#endif /* EIF_CORE_INTERNAL_H */
