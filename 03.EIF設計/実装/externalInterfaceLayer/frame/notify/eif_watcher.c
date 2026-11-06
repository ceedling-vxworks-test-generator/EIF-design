/**
 * @file eif_watcher.c
 * @brief 情報変化の購読とイベント化（詳細設計書 3.2.2、4.2.8、4.2.9）。
 *
 * EIF_Start() で全Connectorの購読要求をまとめ、「公開イベント・通知方法・周期」の組合せごとに
 * 購読表の行を作り、システム情報IFへ購読を1回だけ依頼する。
 * EIFは前回値を持たず、差分も求めない。発生／解消の判定はREF（RIM）等が行い、
 * REF Accessorが通知を公開イベントへ変換して on_info_event() へ渡す。
 * EIFはタイマーを持たない。定期通知の周期の計時はREF（RIM）が行う。
 */
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_log.h"

/** @brief 購読表の1行（詳細設計書 3.2.2 SubscriptionEntry）。 */
typedef struct {
    EIF_SubscriptionRequest request; /**< 公開イベント・通知方法・周期 */
    uint32_t connector_mask;         /**< bit (ID-1) ＝ その購読要求を出したConnector */
    bool watching;                   /**< 下位IFへ監視を依頼済み */
} SubscriptionEntry;

static SubscriptionEntry s_subs[EIF_MAX_SUBSCRIPTIONS]; /**< 購読表（EIF_Start() で作り、以降は不変） */
static size_t s_sub_count = 0U;                            /**< 購読表の行数 */
static bool s_built = false;                               /**< 購読表を作った（再試行では作り直さない） */

/**
 * @brief 2つの購読要求が同じ組合せ（公開イベント・通知方法・周期）か。
 * @param[in] a 購読要求
 * @param[in] b 購読要求
 * @return 同じなら true
 */
static bool same_request(const EIF_SubscriptionRequest *a, const EIF_SubscriptionRequest *b)
{
    return (a->id == b->id) && (a->type == b->type) && (a->interval_ms == b->interval_ms);
}

/**
 * @brief 全Connectorの購読要求を登録順に見て、購読表を作る。同じ組合せは1行にまとめる。
 * @retval EIF_OK       作った
 * @retval EIF_ERR_BUSY 行数が上限（EIF_MAX_SUBSCRIPTIONS）を超える
 */
static EIF_Result build_table(void)
{
    s_sub_count = 0U;
    for (size_t i = 0U; i < EIF_MAX_CONNECTORS; i++) {
        const ConnectorEntry *c = eif_registry_at(i);
        if (c == NULL) {
            break; /* 登録はここまで */
        }
        const uint32_t bit = 1UL << ((unsigned)c->id - 1U);
        for (size_t k = 0U; k < c->subscription_count; k++) {
            const EIF_SubscriptionRequest *r = &c->subscriptions[k];
            size_t row = 0U;
            while ((row < s_sub_count) && !same_request(&s_subs[row].request, r)) {
                row++;
            }
            if (row == s_sub_count) {
                if (s_sub_count >= EIF_MAX_SUBSCRIPTIONS) {
                    EIF_LOG_ERROR("subscription table full: event=%d", (int)r->id);
                    return EIF_ERR_BUSY;
                }
                s_subs[row].request = *r;
                s_subs[row].connector_mask = 0U;
                s_subs[row].watching = false;
                s_sub_count++;
            }
            s_subs[row].connector_mask |= bit;
        }
    }
    return EIF_OK;
}

/**
 * @brief システム情報IFから呼ばれる受け口。公開イベントを、購読表の行の配信先へ渡す。
 *
 * @param[in] event 公開イベント（REFの通知1件分。呼出し中だけ有効）
 * @param[in] ctx   watch_event() で渡した購読表の行（SubscriptionEntry*）
 *
 * @note REF Accessorから、REFの通知ワーカーのcontext [C4] のまま呼ばれる。
 *       購読表は EIF_Start() 後は不変のため、ロックしない。通知1件につき公開イベント1件。
 */
static void on_info_event(const EIF_Event *event, void *ctx)
{
    const SubscriptionEntry *e = (const SubscriptionEntry *)ctx;

    if ((event == NULL) || (e == NULL) || (event->event_id <= EIF_EVT_INVALID) ||
        (event->event_id >= EIF_EVT_COUNT) || (event->event_id != e->request.id)) {
        EIF_LOG_WARN("unexpected event: id=%d", (event != NULL) ? (int)event->event_id : -1);
        return;
    }
    eif_dispatch_event(event, e->connector_mask);
}

/**
 * @brief 購読表を作り、行ごとにシステム情報IFへ購読を依頼する（詳細設計書 4.2.8）。
 *
 * 機種が通知しない購読要求（is_event_supported() が false）は監視しない（その行のConnectorには届かない）。
 *
 * @retval EIF_OK       依頼した
 * @retval EIF_ERR_BUSY 購読表の行数が上限を超える（REFへの購読はしない）
 * @retval その他       watch_event() の失敗（その時点で中断。登録済みの購読は残る）
 *
 * @note EIF_Start() から、起動処理のスレッド [C1] で呼ばれる。
 */
EIF_Result eif_watcher_start(void)
{
    const EIF_SystemInfoIf *info = eif_frame_accessors()->info;

    EIF_Result r;
    if (!s_built) {
        /* 登録表は締切り済みで不変。再試行（前回の失敗後）では、依頼済みの行を残すため作り直さない */
        r = build_table();
        if (r != EIF_OK) {
            s_sub_count = 0U;
            return r;
        }
        s_built = true;
    }
    for (size_t i = 0U; i < s_sub_count; i++) {
        SubscriptionEntry *e = &s_subs[i];
        if (e->watching) {
            continue; /* 前回の EIF_Start() で依頼済み（失敗後の再試行） */
        }
        if (!info->is_event_supported(&e->request)) {
            continue; /* この機種では通知しない */
        }
        /* 通知時に on_info_event を呼んでもらう。ctx に購読表の行を渡し、受け口で配信先に使う */
        r = info->watch_event(&e->request, on_info_event, e);
        if (r != EIF_OK) {
            EIF_LOG_ERROR("watch_event failed: event=%d result=%d", (int)e->request.id, (int)r);
            return r;
        }
        e->watching = true;
    }
    return EIF_OK;
}

/** @brief 試験専用。購読表を空にする。 */
void eif_watcher_reset(void)
{
    memset(s_subs, 0, sizeof(s_subs));
    s_sub_count = 0U;
    s_built = false;
}
