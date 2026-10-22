/**
 * @file eif_request_gateway.c
 * @brief 要求受付・取消・結果照会（詳細設計書 7.3、7.5）。
 *
 * 公開範囲は製品制御IFの is_supported()（＝PCL Accessorの対応表）で決まる。
 */
#include "eif_frame_internal.h"
#include "eif_log.h"

EIF_Result eif_gateway_track(EIF_Ticket ticket, EIF_ConnectorId origin, eif_request_id_t request_id,
                             uint64_t origin_token, EIF_RequestHandle *out_handle)
{
    eif_track_result_t track;
    const EIF_RequestHandle handle = eif_tracker_next_handle();

    if (eif_tracker_register(ticket, origin, origin_token, request_id, handle, &track) != EIF_OK) {
        EIF_LOG_ERROR("tracker full: cancel ticket %llu", (unsigned long long)ticket);
        (void)eif_frame_accessors()->control->cancel(ticket);
        return EIF_ERR_BUSY;
    }
    *out_handle = handle;
    if (track.early_completed) {
        eif_tracker_notify(origin, handle, request_id, origin_token, track.early_outcome);
    }
    return EIF_ACCEPTED;
}

EIF_Result EIF_SubmitRequest(EIF_ConnectorId origin, eif_request_id_t request_id, const void *param,
                             size_t param_size, uint64_t origin_token, EIF_RequestHandle *out_handle)
{
    EIF_Ticket ticket = 0U;

    if (out_handle == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    *out_handle = 0U;
    if (!eif_frame_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if ((eif_registry_get(origin) == NULL) || (param != NULL) || (param_size != 0U)) {
        return EIF_ERR_INVALID_PARAM; /* 要求パラメータはQA-02の対応後 */
    }

    const EIF_ProductControlIf *control = eif_frame_accessors()->control;
    if (!control->is_supported(request_id)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    const EIF_Result r = control->submit(request_id, &ticket);
    if (r != EIF_ACCEPTED) {
        return r;
    }
    return eif_gateway_track(ticket, origin, request_id, origin_token, out_handle);
}

EIF_Result EIF_CancelRequest(EIF_ConnectorId origin, EIF_RequestHandle handle)
{
    EIF_Ticket ticket = 0U;
    EIF_ConnectorId owner = 0U;

    if (handle == 0U) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_frame_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (!eif_tracker_find(handle, &ticket, &owner) || (owner != origin)) {
        return EIF_ERR_NOT_FOUND;
    }
    return eif_frame_accessors()->control->cancel(ticket);
}

EIF_Result EIF_GetRequestResult(EIF_RequestHandle handle, EIF_RequestOutcome *out_outcome)
{
    EIF_Ticket ticket = 0U;
    EIF_ConnectorId owner = 0U;

    if ((handle == 0U) || (out_outcome == NULL)) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_frame_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    if (!eif_tracker_find(handle, &ticket, &owner)) {
        return EIF_ERR_NOT_FOUND;
    }
    return eif_frame_accessors()->control->query(ticket, out_outcome);
}
