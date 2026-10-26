/**
 * @file eif_request_gateway.c
 * @brief 要求受付・取消・結果照会（詳細設計書 7.3、7.5）。
 *
 * 公開範囲は製品制御IFの is_supported()（＝PCL Accessorの対応表）で決まる。
 * EIFは受付可否を判定しない。PCLの受付結果をそのまま返す。
 */
#include "eif_frame_internal.h"
#include "eif_log.h"

/**
 * @brief チケットを要求台帳へ登録し、要求ハンドルを払い出す。先着完了なら要求元へ通知する。
 *
 * 要求受付（EIF_SubmitRequest）と、PCL経由の設定変更（EIF_PatchSetting が EIF_ACCEPTED の場合）で共用する。
 *
 * @param[in]  ticket       Accessorが払い出したチケット
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  request_id   公開要求ID（設定変更では EIF_REQ_INVALID）
 * @param[in]  origin_token 要求元トークン
 * @param[out] out_handle   払い出した要求ハンドル（台帳が満杯なら変更しない）
 *
 * @retval EIF_ACCEPTED 登録した（先着完了なら、この関数の中で完了callbackを呼び終えている）
 * @retval EIF_ERR_BUSY 台帳が満杯（PCLへ取消を依頼済み）
 */
EIF_Result eif_gateway_track(EIF_Ticket ticket, EIF_ConnectorId origin, eif_request_id_t request_id,
                             uint64_t origin_token, EIF_RequestHandle *out_handle)
{
    eif_track_result_t track;
    const EIF_RequestHandle handle = eif_tracker_next_handle();

    if (eif_tracker_register(ticket, origin, origin_token, request_id, handle, &track) != EIF_OK) {
        /* PCLは受け付け済みだが結果を返す先を覚えられない。取り消して受付失敗として返す */
        EIF_LOG_ERROR("tracker full: cancel ticket %llu", (unsigned long long)ticket);
        (void)eif_frame_accessors()->control->cancel(ticket);
        return EIF_ERR_BUSY;
    }
    *out_handle = handle;
    if (track.early_completed) {
        /* 完了が登録より先に届いていた。ここで要求元へ通知する（Submitの戻りより先に完了callbackが呼ばれる） */
        eif_tracker_notify(origin, handle, request_id, origin_token, track.early_outcome);
    }
    return EIF_ACCEPTED;
}

/**
 * @brief 公開要求を受け付け、製品制御IF（PCL Accessor）へ転送する。
 *
 * 手順：引数検証 → 公開範囲の確認 → submit() → 要求台帳へ登録。
 *
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  request_id   公開要求ID
 * @param[in]  param        要求パラメータ（NULLのみ。QA-02）
 * @param[in]  param_size   param のサイズ（0のみ）
 * @param[in]  origin_token 完了callbackでそのまま返す値
 * @param[out] out_handle   受付成功時に要求ハンドル、失敗時は0
 *
 * @retval EIF_ACCEPTED          受付完了
 * @retval EIF_ERR_INVALID_PARAM 引数不正、未登録のorigin、param指定
 * @retval EIF_ERR_UNAVAILABLE   未開始
 * @retval EIF_ERR_NOT_EXPOSED   この機種で公開していない（PCLを呼ばない）
 * @retval その他                PCLの受付結果（EIF_ERR_BUSY 等）、台帳満杯
 *
 * @note Connectorのスレッド [C2] から呼ばれる。PCLの受付で即時に戻り、実行完了は待たない。
 */
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
        return EIF_ERR_NOT_EXPOSED; /* 対応表に無い要求はPCLへ送らない */
    }
    const EIF_Result r = control->submit(request_id, &ticket);
    if (r != EIF_ACCEPTED) {
        return r; /* PCLの判定をそのまま返す。台帳へは登録しない */
    }
    return eif_gateway_track(ticket, origin, request_id, origin_token, out_handle);
}

/**
 * @brief 取消を要求する。台帳のエントリは残し、最終結果は元の要求の完了callbackで届く。
 *
 * @param[in] origin 要求元のConnector ID（要求を出したConnectorと一致すること）
 * @param[in] handle 取り消す要求の要求ハンドル
 *
 * @retval EIF_OK                取消を受け付けた
 * @retval EIF_ERR_INVALID_PARAM handle が0
 * @retval EIF_ERR_UNAVAILABLE   未開始
 * @retval EIF_ERR_NOT_FOUND     台帳に無い、または origin が要求元と異なる
 * @retval その他                PCLの結果（EIF_ERR_BUSY 等）
 *
 * @note Connectorのスレッド [C2] から呼ばれる。
 */
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
        return EIF_ERR_NOT_FOUND; /* 他のConnectorの要求は取り消せない */
    }
    return eif_frame_accessors()->control->cancel(ticket);
}

/**
 * @brief 要求の結果を照会する（製品制御IFの query() へ委譲。EIFは結果を保持しない）。
 *
 * @param[in]  handle      要求ハンドル
 * @param[out] out_outcome 未完了なら EIF_OUTCOME_PENDING、終端済みなら終端結果
 *
 * @retval EIF_OK                照会できた
 * @retval EIF_ERR_INVALID_PARAM handle が0、out_outcome がNULL
 * @retval EIF_ERR_UNAVAILABLE   未開始
 * @retval EIF_ERR_NOT_FOUND     台帳に無い（終端済みでエントリを解放した、または未登録）
 *
 * @note Connectorのスレッド [C2] から呼ばれる。
 */
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
