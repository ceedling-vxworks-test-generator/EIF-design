/**
 * @file eif_info_reader.c
 * @brief 情報取得（詳細設計書 7.6）。REFの型からの変換はREF Accessorが行い、Frameは公開モデルだけを扱う。
 */
#include "eif_frame_internal.h"

/**
 * @brief 公開情報を取得する。システム情報IFの read() へ委譲する（同期）。
 *
 * 手順：引数検証 → 公開範囲の確認 → サイズの照合 → read()。
 *
 * @param[in]  info_id  公開情報ID
 * @param[out] out      公開モデルの書込み先
 * @param[in]  out_size out のサイズ（公開モデルのsizeofと一致すること）
 *
 * @retval EIF_OK                取得した
 * @retval EIF_ERR_INVALID_PARAM out がNULL、サイズ不一致
 * @retval EIF_ERR_UNAVAILABLE   未開始
 * @retval EIF_ERR_NOT_EXPOSED   この機種で公開していない
 * @retval その他                read() の結果（REFの取得失敗、変換失敗等）
 *
 * @note Connectorのスレッド [C2] から呼ばれる。EIFは値を保持せず、毎回REFから取得する。
 */
EIF_Result EIF_GetInfo(eif_info_id_t info_id, void *out, size_t out_size)
{
    if (out == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_frame_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    const EIF_SystemInfoIf *info = eif_frame_accessors()->info;
    if (!info->is_supported(info_id)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    if (out_size != info->public_size(info_id)) {
        return EIF_ERR_INVALID_PARAM; /* 公開モデルの型の取り違えを検出する */
    }
    return info->read(info_id, out, out_size);
}
