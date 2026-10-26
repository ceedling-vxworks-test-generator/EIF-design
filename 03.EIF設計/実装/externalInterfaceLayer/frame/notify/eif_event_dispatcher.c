/**
 * @file eif_event_dispatcher.c
 * @brief 公開イベントの配信（詳細設計書 7.8）。キューを持たず、呼出し元のcontextで直接配信する。
 */
#include "eif_frame_internal.h"

/**
 * @brief 公開イベントを、受信フィルタに一致するConnectorへ登録順に配信する。
 *
 * @param[in] event 公開イベント（呼出し中だけ有効。Connectorは必要ならコピーする）
 *
 * @note Watcher から、REFの通知ワーカーのcontext [C4] のまま呼ばれる。
 *       登録表は EIF_Start() 後は不変のため、ロックしない。
 *       範囲外のイベントIDは配信しない。
 */
void eif_dispatch_event(const EIF_Event *event)
{
    if ((event == NULL) || (event->event_id <= EIF_EVT_INVALID) || (event->event_id >= EIF_EVT_COUNT)) {
        return;
    }
    const uint32_t bit = 1UL << (unsigned)event->event_id;
    for (size_t i = 0U; i < EIF_MAX_CONNECTORS; i++) {
        const eif_connector_t *c = eif_registry_at(i);
        if (c == NULL) {
            break; /* 登録はここまで */
        }
        /* 受信フィルタに含めた公開イベントだけを、on_event を持つConnectorへ届ける */
        if (((c->event_mask & bit) != 0U) && (c->desc.on_event != NULL)) {
            c->desc.on_event(event, c->desc.user_context);
        }
    }
}
