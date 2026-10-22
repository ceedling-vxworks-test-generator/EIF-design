/**
 * @file eif_subscription_manager.c
 * @brief 購読の登録と、REF通知の振り分け（詳細設計書 7.1.2、7.8）。
 *
 * REFへ登録するのは共通のトランポリン1つだけ。製品CFGはREFへ直接登録しない。
 */
#include <string.h>

#include "eif_internal.h"
#include "util/eif_log.h"

typedef struct {
    bool registered;
    uint64_t subscription_id;
} sub_runtime_t;

static sub_runtime_t s_runtime[EIF_MAX_SUBSCRIPTIONS];

EIF_Result eif_subscription_start(void)
{
    const EIF_ProductProfile *p = eif_core_profile();
    const EIF_Ports *ports = eif_core_ports();

    for (size_t i = 0U; i < p->subscription_count; i++) {
        const EIF_SubscriptionEntry *e = &p->subscriptions[i];
        RI_SUBSCRIPTION_REQUEST req;
        uint64_t id = 0U;

        memset(&req, 0, sizeof(req));
        req.targetType = e->target_type;
        req.targetId = e->target_id;
        req.trigger = e->trigger;
        req.method = RI_METHOD_CALLBACK;
        req.intervalMs = e->interval_ms;
        req.callback = eif_subscription_on_ref_notified;

        const RIStatus s = ports->info->subscribe(&req, &id);
        if (s != RI_SUCCESS) {
            /* REFに購読解除APIが無いため、登録済みの購読は残る（QA-07） */
            EIF_LOG_ERROR("subscribe[%zu] failed: target=%u status=%d", i, (unsigned)e->target_id, (int)s);
            return EIF_ERR_INTERNAL;
        }
        s_runtime[i].registered = true;
        s_runtime[i].subscription_id = id;
    }
    return EIF_OK;
}

void eif_subscription_on_ref_notified(uint64_t subscription_id, const void *notification_message)
{
    EIF_SourceView now[EIF_MAX_SOURCES];
    EIF_Event events[EIF_MAX_EVENTS_PER_NOTIFY];
    const void *prev = NULL;
    size_t prev_size = 0U;
    bool has_prev = false;
    size_t n;
    (void)notification_message;

    const EIF_ProductProfile *p = eif_core_profile();
    if (p == NULL) {
        return;
    }

    /* 1. 購読IDから購読エントリを引く（開始後は不変のためロック不要） */
    size_t index = p->subscription_count;
    for (size_t i = 0U; i < p->subscription_count; i++) {
        if (s_runtime[i].registered && (s_runtime[i].subscription_id == subscription_id)) {
            index = i;
            break;
        }
    }
    if (index >= p->subscription_count) {
        EIF_LOG_WARN("notification for unknown subscription %llu", (unsigned long long)subscription_id);
        return;
    }
    const EIF_SubscriptionEntry *e = &p->subscriptions[index];

    /* 2. 最新値を取得する */
    if (eif_info_read_sources(e->source_ids, e->source_sizes, e->source_count, now) != EIF_OK) {
        return; /* 取得失敗時はイベントを生成せず、前回値も更新しない */
    }

    /* 3〜5. 前回値と比べてイベントを作り、今回値を保存する */
    if (e->keep_previous) {
        eif_cache_lock(index, &prev, &prev_size, &has_prev);
    }
    n = e->build_events(now, e->source_count, prev, prev_size, has_prev, events, EIF_MAX_EVENTS_PER_NOTIFY);
    if (e->keep_previous) {
        eif_cache_store_and_unlock(index, now[0].data, now[0].size);
    }

    /* 6. 配信する（ロックは保持しない） */
    if (n > EIF_MAX_EVENTS_PER_NOTIFY) {
        n = EIF_MAX_EVENTS_PER_NOTIFY;
    }
    for (size_t i = 0U; i < n; i++) {
        eif_dispatch_event(&events[i]);
    }
}

void eif_subscription_reset(void)
{
    memset(s_runtime, 0, sizeof(s_runtime));
}
