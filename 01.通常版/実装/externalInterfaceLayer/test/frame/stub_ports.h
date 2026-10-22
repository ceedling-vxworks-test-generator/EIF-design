/**
 * @file stub_ports.h
 * @brief 試験用のPort（PCL・REFの代わり）。
 */
#ifndef STUB_PORTS_H
#define STUB_PORTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_port.h"

extern const EIF_Ports g_stub_ports;

/* PCL */
typedef struct {
    uc_result_t submit_result;          /**< submit が返す api_result */
    bool complete_inline;               /**< submit の中で完了callbackを呼ぶ（先着完了の再現） */
    uint64_t next_generation;
    product_request_id_t last_product_request;
    external_request_completion_cb_t last_cb;
    uint64_t last_cancel_generation;
    int cancel_calls;
    external_request_execution_response_state_t get_result_state;
} stub_pcl_t;
extern stub_pcl_t g_stub_pcl;

/** @brief PCLが要求を完了したことにする（保存した完了callbackを呼ぶ）。 */
void stub_pcl_complete(uint64_t generation_id, external_request_execution_response_state_t state);

/* REF */
#define STUB_REF_MAX_DATA (8U)
#define STUB_REF_MAX_SUBS (8U)
typedef struct {
    RIStatus get_status;                /**< RI_SUCCESS 以外なら get_binary が失敗する */
    RIStatus subscribe_status;
    int set_calls;
    int subscribe_calls;
    RI_SUBSCRIPTION_REQUEST subs[STUB_REF_MAX_SUBS];
} stub_ref_t;
extern stub_ref_t g_stub_ref;

void stub_ref_put(uint32_t data_id, const void *data, size_t size);
const void *stub_ref_peek(uint32_t data_id, size_t *out_size);
/** @brief REFが変化を通知したことにする（index番目に登録された購読のcallbackを呼ぶ）。 */
void stub_ref_notify(size_t subscription_index);

void stub_ports_reset(void);

#endif /* STUB_PORTS_H */
