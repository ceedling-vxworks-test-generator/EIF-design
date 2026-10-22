/**
 * @file ref_accessor.c
 * @brief REF Accessorの共通処理。RIMのAPIを呼び、対応表の変換関数でREFの型と公開モデルを相互に変換する。
 */
#include <stdalign.h>
#include <string.h>

#include "ref_accessor.h"

/** @brief 公開モデル・REFの型を一時的に置く領域の大きさ。 */
#define REF_WORK_BYTES (2048U)
/** @brief 監視できる公開情報の数。 */
#define REF_MAX_WATCH (8U)

typedef struct {
    bool used;
    uint64_t subscription_id;
    const EIF_RefInfoMap *map;
    EIF_InfoChangedFn fn;
    void *ctx;
} watch_t;

static watch_t s_watch[REF_MAX_WATCH];

static EIF_Result map_ri(RIStatus s)
{
    switch (s) {
    case RI_SUCCESS:
        return EIF_OK;
    case RI_NOT_INITIALIZED:
        return EIF_ERR_UNAVAILABLE;
    case RI_NO_DATA:
        return EIF_ERR_NOT_FOUND;
    default:
        return EIF_ERR_INTERNAL;
    }
}

static const EIF_RefInfoMap *find_info(eif_info_id_t id)
{
    for (size_t i = 0U; i < g_ref_info_map_count; i++) {
        if (g_ref_info_map[i].info_id == id) {
            return &g_ref_info_map[i];
        }
    }
    return NULL;
}

static const EIF_RefSettingMap *find_setting(eif_setting_id_t id)
{
    for (size_t i = 0U; i < g_ref_setting_map_count; i++) {
        if (g_ref_setting_map[i].setting_id == id) {
            return &g_ref_setting_map[i];
        }
    }
    return NULL;
}

static EIF_Result read_sources(const uint32_t *ids, const size_t *sizes, size_t count, EIF_RefView *out)
{
    for (size_t i = 0U; i < count; i++) {
        RI_BINARY bin = {0};
        const RIStatus s = RIM_GetBinary(ids[i], &bin);
        if (s != RI_SUCCESS) {
            return map_ri(s);
        }
        if ((bin.data == NULL) || ((size_t)bin.size != sizes[i])) {
            return EIF_ERR_INTERNAL; /* 型の不一致 */
        }
        out[i].data = bin.data;
        out[i].size = (size_t)bin.size;
    }
    return EIF_OK;
}

static EIF_Result read_public(const EIF_RefInfoMap *m, void *out, size_t size)
{
    EIF_RefView v[EIF_REF_MAX_SOURCES];
    if (size != m->public_size) {
        return EIF_ERR_INVALID_PARAM;
    }
    const EIF_Result r = read_sources(m->source_ids, m->source_sizes, m->source_count, v);
    if (r != EIF_OK) {
        return r;
    }
    return m->to_public(v, m->source_count, out, size) ? EIF_OK : EIF_ERR_INTERNAL;
}

/* ---------------------------------------------------------------- システム情報IF（EIF_SystemInfoIf）の実装 */

static bool info_is_supported(eif_info_id_t id)
{
    return find_info(id) != NULL;
}

static size_t info_public_size(eif_info_id_t id)
{
    const EIF_RefInfoMap *m = find_info(id);
    return (m != NULL) ? m->public_size : 0U;
}

static EIF_Result info_read(eif_info_id_t id, void *out, size_t size)
{
    const EIF_RefInfoMap *m = find_info(id);
    return (m != NULL) ? read_public(m, out, size) : EIF_ERR_NOT_EXPOSED;
}

/** @brief RIMへ登録する共通の通知トランポリン（REFの通知ワーカーのcontext）。 */
static void on_ref_notified(uint64_t subscription_id, const void *message)
{
    static alignas(max_align_t) unsigned char buf[REF_WORK_BYTES]; /* 通知は直列に来る前提（QA-11） */
    (void)message;

    for (size_t i = 0U; i < REF_MAX_WATCH; i++) {
        const watch_t *w = &s_watch[i];
        if (w->used && (w->subscription_id == subscription_id)) {
            if (read_public(w->map, buf, w->map->public_size) == EIF_OK) {
                w->fn(w->map->info_id, buf, w->map->public_size, w->ctx);
            }
            return;
        }
    }
}

static EIF_Result info_watch(eif_info_id_t id, EIF_InfoChangedFn fn, void *ctx)
{
    const EIF_RefInfoMap *m = find_info(id);
    RI_SUBSCRIPTION_REQUEST req;
    uint64_t sub_id = 0U;

    if ((m == NULL) || (fn == NULL)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    if (m->public_size > REF_WORK_BYTES) {
        return EIF_ERR_INTERNAL;
    }
    for (size_t i = 0U; i < REF_MAX_WATCH; i++) {
        if (!s_watch[i].used) {
            memset(&req, 0, sizeof(req));
            req.targetType = RI_TARGET_DATA;
            req.targetId = m->source_ids[0];
            req.trigger = m->trigger;
            req.method = RI_METHOD_CALLBACK;
            req.intervalMs = m->interval_ms;
            req.callback = on_ref_notified;
            const RIStatus s = RIM_Subscribe(&req, &sub_id);
            if (s != RI_SUCCESS) {
                return map_ri(s);
            }
            s_watch[i].subscription_id = sub_id;
            s_watch[i].map = m;
            s_watch[i].fn = fn;
            s_watch[i].ctx = ctx;
            s_watch[i].used = true;
            return EIF_OK;
        }
    }
    return EIF_ERR_BUSY;
}

const EIF_SystemInfoIf g_ref_info_if = {
    .is_supported = info_is_supported,
    .public_size = info_public_size,
    .read = info_read,
    .watch = info_watch,
};

/* ---------------------------------------------------------------- 設定保存IF（EIF_SettingStoreIf）の実装：REFへ直接書き込む */

static bool setting_is_supported(eif_setting_id_t id)
{
    return find_setting(id) != NULL;
}

static EIF_Result setting_store(eif_setting_id_t id, const void *merged, size_t size, EIF_Ticket *out_ticket)
{
    static alignas(max_align_t) unsigned char current_public[REF_WORK_BYTES];
    static alignas(max_align_t) unsigned char out_ref[REF_WORK_BYTES];
    const EIF_RefSettingMap *s = find_setting(id);
    EIF_RefView cur;

    (void)out_ticket;
    if (s == NULL) {
        return EIF_ERR_NOT_EXPOSED;
    }
    const EIF_RefInfoMap *m = find_info(s->current_info);
    if ((m == NULL) || (size != m->public_size) || (size > REF_WORK_BYTES) || (s->ref_size > REF_WORK_BYTES)) {
        return EIF_ERR_INTERNAL;
    }
    /* Frameの設定マージは排他区間内で呼ぶため、ここでの静的領域の利用は直列になる */
    EIF_Result r = read_public(m, current_public, size);
    if (r == EIF_OK) {
        r = read_sources(&s->ref_data_id, &s->ref_size, 1U, &cur);
    }
    if (r != EIF_OK) {
        return r;
    }
    memcpy(out_ref, cur.data, s->ref_size); /* 公開モデルに無いREFの項目を保つ */
    if (!s->from_public(merged, current_public, cur.data, out_ref, s->ref_size)) {
        return EIF_ERR_INVALID_PARAM;
    }
    return map_ri(RIM_SetBinary(s->ref_data_id, out_ref, s->ref_size));
}

const EIF_SettingStoreIf g_ref_setting_if = {
    .is_supported = setting_is_supported,
    .store = setting_store,
};

void ref_accessor_reset_for_test(void)
{
    memset(s_watch, 0, sizeof(s_watch));
}
