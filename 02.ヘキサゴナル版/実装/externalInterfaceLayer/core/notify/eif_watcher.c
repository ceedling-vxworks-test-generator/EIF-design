/**
 * @file eif_watcher.c
 * @brief 情報変化の監視と差分（設計書_ヘキサゴナル版 7.1、7.3、11.4）。
 *
 * 通知定義表（機種によらない）に従って、システム情報ポートへ監視を依頼する。
 * 差分は公開モデル（アラーム一覧）に対して行うため、機種ごとの差分関数は要らない。
 * EIFはタイマーを持たない。定期通知が必要な情報は、REFアダプタの対応表で定期通知を指定する。
 */
#include <string.h>

#include "eif_core_internal.h"
#include "eif_log.h"

typedef enum {
    WATCH_LIST_DIFF = 0 /**< 一覧の差分（キー＝alarm_id）→ 発生／解消 */
} watch_kind_t;

typedef struct {
    eif_info_id_t info_id;
    watch_kind_t kind;
    eif_event_id_t raised;
    eif_event_id_t resolved;
} watch_def_t;

/** @brief 通知定義表。公開モデルが増えたときだけ追加する。 */
static const watch_def_t k_watch[] = {
    { EIF_INFO_ALARM_LIST, WATCH_LIST_DIFF, EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED },
};
#define WATCH_COUNT (sizeof(k_watch) / sizeof(k_watch[0]))

static bool contains(const eif_alarm_list_t *l, const eif_alarm_t *a)
{
    const unsigned n = (l->count < EIF_ALARM_LIST_MAX) ? l->count : EIF_ALARM_LIST_MAX;
    for (unsigned i = 0U; i < n; i++) {
        if (strncmp(l->items[i].alarm_id, a->alarm_id, EIF_ALARM_TEXT_LEN) == 0) {
            return true;
        }
    }
    return false;
}

static size_t emit(const eif_alarm_list_t *from, const eif_alarm_list_t *other, eif_event_id_t id,
                   EIF_Event *out, size_t pos)
{
    const unsigned n = (from->count < EIF_ALARM_LIST_MAX) ? from->count : EIF_ALARM_LIST_MAX;
    for (unsigned i = 0U; (i < n) && (pos < EIF_MAX_EVENTS_PER_NOTIFY); i++) {
        if ((other == NULL) || !contains(other, &from->items[i])) {
            out[pos].event_id = id;
            out[pos].payload.alarm = from->items[i];
            pos++;
        }
    }
    return pos;
}

/** @brief システム情報ポートから呼ばれる受け口（REFの通知ワーカーのcontext）。 */
static void on_info_changed(eif_info_id_t id, const void *public_model, size_t size, void *ctx)
{
    const watch_def_t *d = (const watch_def_t *)ctx;
    EIF_Event events[EIF_MAX_EVENTS_PER_NOTIFY];
    const void *prev = NULL;
    bool has_prev = false;
    size_t n = 0U;

    if ((d == NULL) || (d->info_id != id) || (size != sizeof(eif_alarm_list_t))) {
        EIF_LOG_WARN("unexpected info change: id=%d size=%zu", (int)id, size);
        return;
    }
    eif_cache_lock(id, &prev, &has_prev);
    n = emit((const eif_alarm_list_t *)public_model, has_prev ? (const eif_alarm_list_t *)prev : NULL,
             d->raised, events, 0U);
    if (has_prev) {
        n = emit((const eif_alarm_list_t *)prev, (const eif_alarm_list_t *)public_model, d->resolved, events, n);
    }
    eif_cache_store_and_unlock(id, public_model, size);

    for (size_t i = 0U; i < n; i++) {
        eif_dispatch_event(&events[i]);
    }
}

EIF_Result eif_watcher_start(void)
{
    const EIF_SystemInfoPort *info = eif_core_ports()->info;

    for (size_t i = 0U; i < WATCH_COUNT; i++) {
        const watch_def_t *d = &k_watch[i];
        if (!info->is_supported(d->info_id)) {
            continue; /* この機種では公開しない */
        }
        if (!eif_cache_allocate(d->info_id, sizeof(eif_alarm_list_t))) {
            EIF_LOG_ERROR("state cache capacity exceeded: info=%d", (int)d->info_id);
            return EIF_ERR_INTERNAL;
        }
        const EIF_Result r = info->watch(d->info_id, on_info_changed, (void *)d);
        if (r != EIF_OK) {
            EIF_LOG_ERROR("watch failed: info=%d result=%d", (int)d->info_id, (int)r);
            return r;
        }
    }
    return EIF_OK;
}

void eif_watcher_reset(void)
{
}
