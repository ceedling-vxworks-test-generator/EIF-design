/**
 * @file eif_event_dispatcher.c
 * @brief 公開イベントの配信（詳細設計書 4.2.5）。キューを持たず、呼出し元のcontextで直接配信する。
 */
#include "eif_frame_internal.h"

/**
 * @brief 公開イベントを、connector_mask に含まれるConnectorへ登録順に配信する。
 *
 * @param[in] event          公開イベント（呼出し中だけ有効。Connectorは必要ならコピーする）
 * @param[in] connector_mask 配信先（bit (ID-1) ＝ Connector ID）。購読表の行の値
 *
 * @note Watcher から、REFの通知ワーカーのcontext [C4] のまま呼ばれる。
 *       登録表は EIF_Start() 後は不変のため、ロックしない。
 *       範囲外のイベントIDは配信しない。
 */
void eif_dispatch_event(const EIF_Event *event, uint32_t connector_mask)
{
    if ((event == NULL) || (event->event_id <= EIF_EVT_INVALID) || (event->event_id >= EIF_EVT_COUNT)) {
        return;
    }
    for (size_t i = 0U; (i < EIF_MAX_CONNECTORS) && (connector_mask != 0U); i++) {
        const ConnectorEntry *c = eif_registry_at(i);
        if (c == NULL) {
            break; /* 登録はここまで */
        }
        /* 購読要求を出したConnectorのうち、on_event を持つものへ届ける */
        const uint32_t bit = 1UL << ((unsigned)c->id - 1U);
        if (((connector_mask & bit) != 0U) && (c->on_event != NULL)) {
            c->on_event(event, c->user_context);
        }
    }
}
