/**
 * @file eif_watcher.c
 * @brief 情報変化の監視と差分（詳細設計書 6.3.4、7.8）。
 *
 * 通知定義表（機種によらない）に従って、システム情報IFへ監視を依頼する。
 * 差分は公開モデル（アラーム一覧）に対して行うため、機種ごとの差分関数は要らない。
 * EIFはタイマーを持たない。定期通知が必要な情報は、REF Accessorの対応表で定期通知を指定する。
 */
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_log.h"

/** @brief 差分の種類。 */
typedef enum {
    WATCH_LIST_DIFF = 0 /**< 一覧の差分（キー＝alarm_id）→ 発生／解消 */
} watch_kind_t;

/** @brief 通知定義表の1行。 */
typedef struct {
    eif_info_id_t info_id;    /**< 監視する公開情報 */
    watch_kind_t kind;        /**< 差分の種類 */
    eif_event_id_t raised;    /**< 新しく現れた要素に対して出すイベント */
    eif_event_id_t resolved;  /**< 消えた要素に対して出すイベント */
} watch_def_t;

/** @brief 通知定義表。公開モデルが増えたときだけ追加する。 */
static const watch_def_t k_watch[] = {
    { EIF_INFO_ALARM_LIST, WATCH_LIST_DIFF, EIF_EVT_ERROR_RAISED, EIF_EVT_ERROR_RESOLVED },
};
/** @brief 通知定義表の行数。 */
#define WATCH_COUNT (sizeof(k_watch) / sizeof(k_watch[0]))

/**
 * @brief アラーム一覧に、同じ alarm_id のアラームが含まれるか。
 * @param[in] l 探す一覧
 * @param[in] a 探すアラーム
 * @return 含まれていれば true
 */
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

/**
 * @brief from にあって other に無いアラームについて、イベントを out へ追加する。
 *
 * 発生：from＝今回、other＝前回（前回が無ければ other＝NULL で全件）。
 * 解消：from＝前回、other＝今回。
 *
 * @param[in]     from  元の一覧
 * @param[in]     other 比べる一覧（NULLなら from の全件を出す）
 * @param[in]     id    出すイベントID
 * @param[in,out] out   イベントの配列（EIF_MAX_EVENTS_PER_NOTIFY 件）
 * @param[in]     pos   out の書込み開始位置
 * @return 書込み後の位置（EIF_MAX_EVENTS_PER_NOTIFY を超えない。超える分は出さない）
 */
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

/**
 * @brief システム情報IFから呼ばれる受け口。前回値と比べて発生／解消イベントを作り、配信する。
 *
 * 手順：前回値をロックして取得 → 発生（今回にあって前回に無い）→ 解消（前回にあって今回に無い）
 *       → 今回値を保存してロックを放す → 各イベントを配信。
 * 起動直後（前回値なし）は、今回の全件を発生として出す。
 *
 * @param[in] id           変化した公開情報ID
 * @param[in] public_model 最新値（eif_alarm_list_t。呼出し中だけ有効）
 * @param[in] size         public_model のサイズ
 * @param[in] ctx          watch() で渡した通知定義表の行（watch_def_t*）
 *
 * @note REF Accessorから、REFの通知ワーカーのcontext [C4] のまま呼ばれる。
 *       イベントはスタック上に作り、配信が終われば破棄する（キューを持たない）。
 */
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
    /* 発生：今回にあって前回に無い（前回が無ければ全件） */
    n = emit((const eif_alarm_list_t *)public_model, has_prev ? (const eif_alarm_list_t *)prev : NULL,
             d->raised, events, 0U);
    if (has_prev) {
        /* 解消：前回にあって今回に無い */
        n = emit((const eif_alarm_list_t *)prev, (const eif_alarm_list_t *)public_model, d->resolved, events, n);
    }
    eif_cache_store_and_unlock(id, public_model, size);

    /* 前回値のロックを放してから配信する（Connectorのcallback中にロックを持たない） */
    for (size_t i = 0U; i < n; i++) {
        eif_dispatch_event(&events[i]);
    }
}

/**
 * @brief 通知定義表に従って、システム情報IFへ監視を依頼する。
 *
 * この機種で公開していない情報（is_supported() が false）は飛ばす。
 * 監視する情報ごとに、StateCacheへ前回値の領域を割り当てる。
 *
 * @retval EIF_OK           依頼した
 * @retval EIF_ERR_INTERNAL StateCacheの容量不足
 * @retval その他           watch() の失敗（REFへの購読登録の失敗等）
 *
 * @note EIF_Start() から、起動処理のスレッド [C1] で呼ばれる。
 */
EIF_Result eif_watcher_start(void)
{
    const EIF_SystemInfoIf *info = eif_frame_accessors()->info;

    for (size_t i = 0U; i < WATCH_COUNT; i++) {
        const watch_def_t *d = &k_watch[i];
        if (!info->is_supported(d->info_id)) {
            continue; /* この機種では公開しない */
        }
        if (!eif_cache_allocate(d->info_id, sizeof(eif_alarm_list_t))) {
            EIF_LOG_ERROR("state cache capacity exceeded: info=%d", (int)d->info_id);
            return EIF_ERR_INTERNAL;
        }
        /* 変化時に on_info_changed を呼んでもらう。ctx に定義表の行を渡し、受け口で使う */
        const EIF_Result r = info->watch(d->info_id, on_info_changed, (void *)d);
        if (r != EIF_OK) {
            EIF_LOG_ERROR("watch failed: info=%d result=%d", (int)d->info_id, (int)r);
            return r;
        }
    }
    return EIF_OK;
}

/** @brief 試験専用。監視の状態を戻す（Watcherは状態を持たないため何もしない）。 */
void eif_watcher_reset(void)
{
}
