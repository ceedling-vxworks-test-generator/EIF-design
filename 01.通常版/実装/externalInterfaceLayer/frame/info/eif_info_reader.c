/**
 * @file eif_info_reader.c
 * @brief 情報取得（詳細設計書 7.6）。REFのデータはコピーせず、ポインタとサイズのまま変換関数へ渡す。
 */
#include "eif_internal.h"
#include "util/eif_log.h"

EIF_Result eif_info_read_sources(const uint32_t *ids, const size_t *sizes, size_t count, EIF_SourceView *out_views)
{
    const EIF_Ports *ports = eif_core_ports();

    for (size_t i = 0U; i < count; i++) {
        RI_BINARY bin = {0};
        const RIStatus s = ports->info->get_binary(ids[i], &bin);
        if (s != RI_SUCCESS) {
            return eif_map_ri(s);
        }
        if ((bin.data == NULL) || ((size_t)bin.size != sizes[i])) {
            EIF_LOG_ERROR("REF data %u: size mismatch (expected %zu, actual %u)", (unsigned)ids[i], sizes[i],
                          (unsigned)bin.size);
            return EIF_ERR_INTERNAL;
        }
        out_views[i].data = bin.data;
        out_views[i].size = (size_t)bin.size;
    }
    return EIF_OK;
}

EIF_Result EIF_GetInfo(eif_info_id_t info_id, void *out, size_t out_size)
{
    EIF_SourceView views[EIF_MAX_SOURCES];

    if (out == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!eif_core_is_started()) {
        return EIF_ERR_UNAVAILABLE;
    }

    const EIF_ProductProfile *p = eif_core_profile();
    const EIF_InfoMapEntry *e = NULL;
    for (size_t i = 0U; i < p->info_count; i++) {
        if (p->infos[i].public_id == info_id) {
            e = &p->infos[i];
            break;
        }
    }
    if (e == NULL) {
        return EIF_ERR_NOT_EXPOSED;
    }
    if (out_size != e->public_size) {
        return EIF_ERR_INVALID_PARAM;
    }

    const EIF_Result r = eif_info_read_sources(e->source_ids, e->source_sizes, e->source_count, views);
    if (r != EIF_OK) {
        return r;
    }
    if (!e->convert(views, e->source_count, out, out_size)) {
        EIF_LOG_ERROR("info %d: convert failed", (int)info_id);
        return EIF_ERR_INTERNAL;
    }
    return EIF_OK;
}
