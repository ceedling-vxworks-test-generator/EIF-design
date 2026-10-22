/**
 * @file eif_request_gateway.c
 * @brief 要求受付・取消・結果照会（詳細設計書 7.3、7.5）。
 */
#include "eif_internal.h"
#include "util/eif_log.h"

static const EIF_RequestMapEntry *find_request(const EIF_ProductProfile *p, eif_request_id_t id)
{
    for (size_t i = 0U; i < p->request_count; i++) {
        if (p->requests[i].public_id == id) {
            return &p->requests[i];
        }
    }
    return NULL;
}

EIF_Result eif_gateway_submit_product(EIF_ConnectorId origin, product_request_id_t product_request,
                                      eif_request_id_t request_id, uint64_t origin_token,
                                      EIF_RequestHandle *out_handle)
{
    const EIF_Ports *ports = eif_core_ports();
    eif_track_result_t track;

    /* 1. PCLへ転送する（受付可否はPCLが判定する） */
    const external_request_result_t r = ports->pcl->submit(product_request, eif_tracker_on_pcl_completed);
    if (r.api_result != UC_OK) {
        return eif_map_uc(r.api_result);
    }

    /* 2. 台帳へ登録する */
    const EIF_RequestHandle handle = eif_tracker_next_handle();
    if (eif_tracker_register(r.generation_id, origin, origin_token, request_id, handle, &track) != EIF_OK) {
        EIF_LOG_ERROR("tracker full: cancel generation %llu", (unsigned long long)r.generation_id);
        (void)ports->pcl->cancel(r.generation_id);
        return EIF_ERR_BUSY;
    }
    *out_handle = handle;

    /* 3. 先着完了なら、ここで要求元へ通知する（Submitの戻りより先にcallbackが呼ばれる） */
    if (track.early_completed) {
        eif_tracker_notify(origin, handle, request_id, origin_token, track.early_outcome);
    }
    return EIF_ACCEPTED;
}

EIF_Result EIF_SubmitRequest(EIF_ConnectorId origin, eif_request_id_t request_id, const void *param,
                             size_t param_size, uint64_t origin_token, EIF_RequestHandle *out_handle)
{
    if (out_handle == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    *out_handle = 0U;

    if (!eif_core_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (eif_registry_get(origin) == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    if ((param != NULL) || (param_size != 0U)) {
        return EIF_ERR_INVALID_PARAM; /* PCLが要求パラメータを受け付けるまでは不可（QA-02） */
    }

    const EIF_RequestMapEntry *e = find_request(eif_core_profile(), request_id);
    if (e == NULL) {
        return EIF_ERR_NOT_EXPOSED;
    }
    return eif_gateway_submit_product(origin, e->product_request, request_id, origin_token, out_handle);
}

EIF_Result EIF_CancelRequest(EIF_ConnectorId origin, EIF_RequestHandle handle)
{
    uint64_t generation_id = 0U;
    EIF_ConnectorId owner = 0U;

    if (handle == 0U) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_core_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (!eif_tracker_find(handle, &generation_id, &owner) || (owner != origin)) {
        return EIF_ERR_NOT_FOUND;
    }
    const EIF_Result r = eif_map_uc(eif_core_ports()->pcl->cancel(generation_id));
    return (r == EIF_ACCEPTED) ? EIF_OK : r;
}

EIF_Result EIF_GetRequestResult(EIF_RequestHandle handle, EIF_RequestOutcome *out_outcome)
{
    uint64_t generation_id = 0U;
    EIF_ConnectorId owner = 0U;
    external_request_result_t r;

    if ((handle == 0U) || (out_outcome == NULL)) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_core_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (!eif_tracker_find(handle, &generation_id, &owner)) {
        return EIF_ERR_NOT_FOUND; /* 終端済み。EIFは結果を保持しない */
    }
    const uc_result_t u = eif_core_ports()->pcl->get_result(generation_id, &r);
    if (u != UC_OK) {
        const EIF_Result m = eif_map_uc(u);
        return (m == EIF_ACCEPTED) ? EIF_ERR_INTERNAL : m;
    }
    *out_outcome = eif_map_outcome(r.execution_result);
    return EIF_OK;
}
