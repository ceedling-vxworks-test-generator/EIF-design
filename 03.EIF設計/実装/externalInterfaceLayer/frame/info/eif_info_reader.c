/**
 * @file eif_info_reader.c
 * @brief 情報取得（詳細設計書 7.6）。REFの型からの変換はREF Accessorが行い、Frameは公開モデルだけを扱う。
 */
#include "eif_frame_internal.h"

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
        return EIF_ERR_INVALID_PARAM;
    }
    return info->read(info_id, out, out_size);
}
