/**
 * @file pcl_accessor.c
 * @brief PCL Accessorの共通処理。PCLのAPIを呼び、PCLの型とEIFの型を相互に変換する。
 *
 * チケット＝PCLの受付ID（generation ID）。Frameはチケットの中身を解釈しない。
 * 機種ごとの違い（公開要求ID → 製品要求ID）は models/<MODEL>/ の対応表だけに置く。
 */
#include <stdatomic.h>

#include "pcl_accessor.h"

/** @brief 完了時に呼ぶFrameの関数（set_done_handler() で受け取る。PCLの完了contextから読むためatomic）。 */
static _Atomic(EIF_ControlDoneFn) s_done = NULL;

/**
 * @brief 対応表から公開要求IDの行を探す。
 * @param[in] id 公開要求ID
 * @return 対応表の行。無ければNULL（＝この機種では公開しない）
 */
static const PclRequestMapping *find(eif_request_id_t id)
{
    for (size_t i = 0U; i < g_pcl_request_map_count; i++) {
        if (g_pcl_request_map[i].public_id == id) {
            return &g_pcl_request_map[i];
        }
    }
    return NULL;
}

/**
 * @brief PCLの結果コード → EIF_Result（詳細設計書 9.1）。
 * @param[in] r PCLの結果コード
 * @return 変換後の結果コード（UC_OK は EIF_ACCEPTED）
 */
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

/**
 * @brief PCLの終端状態 → EIFの終端結果（仕様設計書 10.2.2）。
 * @param[in] s PCLの終端状態
 * @return EIFの終端結果（未知の値は EIF_OUTCOME_FAILED）
 */
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

/**
 * @brief PCLの完了callback。結果を変換してFrameの完了トランポリンへ渡す。
 *
 * @param[in] id            PCLの製品要求ID（使わない）
 * @param[in] generation_id PCLの受付ID（＝チケット）
 * @param[in] result        PCLの終端状態
 *
 * @note PCLの要求処理context [C3] から呼ばれる。ExternalRequestIngressSubmit() の戻りより先に呼ばれることがある。
 */
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

/**
 * @brief この機種で公開している要求か。
 * @param[in] id 公開要求ID
 * @return 対応表に行があれば true
 */
static bool ctl_is_supported(eif_request_id_t id)
{
    return find(id) != NULL;
}

/**
 * @brief 対応表で製品要求IDへ変換し、PCLへ送信する。
 *
 * @param[in]  id  公開要求ID
 * @param[out] out 受付成功時にチケット（PCLの受付ID）
 *
 * @retval EIF_ACCEPTED        PCLが受け付けた
 * @retval EIF_ERR_NOT_EXPOSED 対応表に無い、out がNULL
 * @retval その他              PCLの結果コードの変換結果
 *
 * @note Connectorのスレッド [C2] から呼ばれる。PCLは受付で即時に戻る。
 */
static EIF_Result ctl_submit(eif_request_id_t id, EIF_Ticket *out)
{
    const PclRequestMapping *m = find(id);
    if ((m == NULL) || (out == NULL)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    /* 完了時に呼んでもらう関数として pcl_accessor_on_completed を渡す */
    const external_request_result_t r = ExternalRequestIngressSubmit(m->pcl_id, pcl_accessor_on_completed);
    if (r.api_result != UC_OK) {
        return pcl_accessor_map_uc(r.api_result);
    }
    *out = (EIF_Ticket)r.generation_id;
    return EIF_ACCEPTED;
}

/**
 * @brief PCLへ取消を依頼する。
 * @param[in] ticket 取り消す要求のチケット（PCLの受付ID）
 * @retval EIF_OK 取消を受け付けた
 * @retval その他 PCLの結果コードの変換結果
 */
static EIF_Result ctl_cancel(EIF_Ticket ticket)
{
    const EIF_Result r = pcl_accessor_map_uc(ExternalRequestIngressCancelRequestedByOwnerGeneration((uint64_t)ticket));
    return (r == EIF_ACCEPTED) ? EIF_OK : r; /* 取消の受付は EIF_OK で返す */
}

/**
 * @brief PCLへ結果を照会する。
 * @param[in]  ticket 照会する要求のチケット（PCLの受付ID）
 * @param[out] out    終端結果、または EIF_OUTCOME_PENDING
 * @retval EIF_OK 照会できた
 * @retval その他 PCLの結果コードの変換結果（UC_OK以外）
 */
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

/**
 * @brief 完了時に呼ぶFrameの関数を受け取る。
 * @param[in] fn Frameの完了トランポリン（eif_tracker_on_done）
 * @note EIF_Frame_Initialize() から1回だけ呼ばれる。
 */
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
