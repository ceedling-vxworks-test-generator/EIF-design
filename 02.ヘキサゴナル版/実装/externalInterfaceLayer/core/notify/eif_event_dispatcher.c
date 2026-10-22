/**
 * @file eif_event_dispatcher.c
 * @brief 公開イベントの配信（詳細設計書 7.8 手順6）。キューを持たず、呼出し元のcontextで直接配信する。
 */
#include "eif_core_internal.h"

void eif_dispatch_event(const EIF_Event *event)
{
    if ((event == NULL) || (event->event_id <= EIF_EVT_INVALID) || (event->event_id >= EIF_EVT_COUNT)) {
        return;
    }
    const uint32_t bit = 1UL << (unsigned)event->event_id;
    for (size_t i = 0U; i < EIF_MAX_CONNECTORS; i++) {
        const eif_connector_t *c = eif_registry_at(i);
        if (c == NULL) {
            break;
        }
        if (((c->event_mask & bit) != 0U) && (c->desc.on_event != NULL)) {
            c->desc.on_event(event, c->desc.user_context);
        }
    }
}
