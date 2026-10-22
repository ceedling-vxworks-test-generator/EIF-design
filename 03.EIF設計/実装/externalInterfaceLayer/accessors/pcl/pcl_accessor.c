/**
 * @file pcl_accessor.c
 * @brief PCL Accessorの共通処理。PCLのAPIを呼び、PCLの型とEIFの型を相互に変換する。
 *
 * チケット＝PCLの受付ID（generation ID）。Frameはチケットの中身を解釈しない。
 */
#include <stdatomic.h>

#include "pcl_accessor.h"

static _Atomic(EIF_ControlDoneFn) s_done = NULL;

static const EIF_PclRequestMap *find(eif_request_id_t id)
{
    for (size_t i = 0U; i < g_pcl_request_map_count; i++) {
        if (g_pcl_request_map[i].public_id == id) {
            return &g_pcl_request_map[i];
        }
    }
    return NULL;
}

EIF_Result pcl_accessor_map_uc(uc_result_t r)
{
    switch (r) {
    case UC_OK:
        return EIF_ACCEPTED;
    case UC_ERR_INVALID_PARAM:
        return EIF_ERR_INVALID_PARAM;
    case UC_ERR_BUSY:
        return EIF_ERR_BUSY;
    case UC_ERR_NOT_FOUND:
        return EIF_ERR_NOT_FOUND;
    case UC_ERR_SERVER_UNAVAILABLE:
        return EIF_ERR_UNAVAILABLE;
    default:
        return EIF_ERR_INTERNAL;
    }
}

static EIF_RequestOutcome map_outcome(external_request_execution_response_state_t s)
{
    switch (s) {
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_ACCEPTED:
        return EIF_OUTCOME_COMPLETED;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_INVALID:
        return EIF_OUTCOME_INVALID;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_NOT_SUPPORTED:
        return EIF_OUTCOME_NOT_SUPPORTED;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_CANCELED:
        return EIF_OUTCOME_CANCELED;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_PENDING:
        return EIF_OUTCOME_PENDING;
    case EXTERNAL_REQUEST_EXECUTION_RESPONSE_FAILED:
    default:
        return EIF_OUTCOME_FAILED;
    }
}

void pcl_accessor_on_completed(product_request_id_t id, uint64_t generation_id,
                              external_request_execution_response_state_t result)
{
    (void)id;
    const EIF_ControlDoneFn fn = atomic_load(&s_done);
    if (fn != NULL) {
        fn((EIF_Ticket)generation_id, map_outcome(result)); /* PCLの要求処理contextのままFrameへ渡す */
    }
}

/* ---------------------------------------------------------------- 製品制御IF（EIF_ProductControlIf）の実装 */

static bool ctl_is_supported(eif_request_id_t id)
{
    return find(id) != NULL;
}

static EIF_Result ctl_submit(eif_request_id_t id, EIF_Ticket *out)
{
    const EIF_PclRequestMap *m = find(id);
    if ((m == NULL) || (out == NULL)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    const external_request_result_t r = ExternalRequestIngressSubmit(m->product_request, pcl_accessor_on_completed);
    if (r.api_result != UC_OK) {
        return pcl_accessor_map_uc(r.api_result);
    }
    *out = (EIF_Ticket)r.generation_id;
    return EIF_ACCEPTED;
}

static EIF_Result ctl_cancel(EIF_Ticket ticket)
{
    const EIF_Result r = pcl_accessor_map_uc(ExternalRequestIngressCancelRequestedByOwnerGeneration((uint64_t)ticket));
    return (r == EIF_ACCEPTED) ? EIF_OK : r;
}

static EIF_Result ctl_query(EIF_Ticket ticket, EIF_RequestOutcome *out)
{
    external_request_result_t r;
    const uc_result_t u = ExternalRequestIngressGetResult((uint64_t)ticket, &r);
    if (u != UC_OK) {
        const EIF_Result m = pcl_accessor_map_uc(u);
        return (m == EIF_ACCEPTED) ? EIF_ERR_INTERNAL : m;
    }
    *out = map_outcome(r.execution_result);
    return EIF_OK;
}

static void ctl_set_done_handler(EIF_ControlDoneFn fn)
{
    atomic_store(&s_done, fn);
}

const EIF_ProductControlIf g_pcl_control_if = {
    .is_supported = ctl_is_supported,
    .submit = ctl_submit,
    .cancel = ctl_cancel,
    .query = ctl_query,
    .set_done_handler = ctl_set_done_handler,
};
