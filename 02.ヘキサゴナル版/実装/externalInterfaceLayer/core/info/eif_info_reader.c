/**
 * @file eif_info_reader.c
 * @brief 情報取得（設計書_ヘキサゴナル版 11.2）。REFの型への変換はREFアダプタが行い、コアは公開モデルだけを扱う。
 */
#include "eif_core_internal.h"

EIF_Result EIF_GetInfo(eif_info_id_t info_id, void *out, size_t out_size)
{
    if (out == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_core_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }
    const EIF_SystemInfoPort *info = eif_core_ports()->info;
    if (!info->is_supported(info_id)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    if (out_size != info->public_size(info_id)) {
        return EIF_ERR_INVALID_PARAM;
    }
    return info->read(info_id, out, out_size);
}
