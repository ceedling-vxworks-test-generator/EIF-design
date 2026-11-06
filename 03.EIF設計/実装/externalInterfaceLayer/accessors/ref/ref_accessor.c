/**
 * @file ref_accessor.c
 * @brief REF Accessorの共通処理。RIMのAPIを呼び、対応表の変換関数でREFの型と公開モデルを相互に変換する。
 *
 * 機種ごとの違い（REFのID・型・変換）は models/<MODEL>/ の対応表だけに置く。
 * REFから取得した値はコピーせず、ポインタ（RefView）のまま変換関数へ渡す。
 */
#include <stdalign.h>
#include <string.h>

#include "ref_accessor.h"

/** @brief 公開モデル・REFの型を一時的に置く領域の大きさ。 */
#define REF_WORK_BYTES (2048U)
/** @brief 監視の登録表の行数（Frameの購読表の上限と同じ）。 */
#define REF_MAX_WATCH (8U)

/** @brief 監視登録表の1行（詳細設計書 3.3.5 WatchEntry。REFの購読IDと、Frameの受け口の対応）。 */
typedef struct {
    bool used;                   /**< 使用中 */
    uint64_t subscription_id;    /**< REFが払い出した購読ID */
    const RefEventMapping *map;  /**< イベントの対応表の行 */
    EIF_InfoEventFn fn;          /**< Frameの受け口（on_info_event） */
    void *ctx;                   /**< Frameの購読表の行 */
} WatchEntry;

static WatchEntry s_watch[REF_MAX_WATCH]; /**< 監視の登録表（EIF_Start() 後は不変） */

/**
 * @brief REFの結果コード → EIF_Result（詳細設計書 9.3）。
 * @param[in] s REFの結果コード
 * @return 変換後の結果コード
 */
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

/**
 * @brief 情報の対応表から公開情報IDの行を探す。
 * @param[in] id 公開情報ID
 * @return 対応表の行。無ければNULL（＝この機種では公開しない）
 */
static const RefInfoMapping *find_info(eif_info_id_t id)
{
    for (size_t i = 0U; i < g_ref_info_map_count; i++) {
        if (g_ref_info_map[i].public_info_id == id) {
            return &g_ref_info_map[i];
        }
    }
    return NULL;
}

/**
 * @brief イベントの対応表から公開イベントIDの行を探す。
 * @param[in] id 公開イベントID
 * @return 対応表の行。無ければNULL（＝この機種では通知しない）
 */
static const RefEventMapping *find_event(eif_event_id_t id)
{
    for (size_t i = 0U; i < g_ref_event_map_count; i++) {
        if (g_ref_event_map[i].public_event_id == id) {
            return &g_ref_event_map[i];
        }
    }
    return NULL;
}

/**
 * @brief 設定の対応表から公開設定IDの行を探す。
 * @param[in] id 公開設定ID
 * @return 対応表の行。無ければNULL（＝この機種では公開しない）
 */
static const RefSettingMapping *find_setting(eif_setting_id_t id)
{
    for (size_t i = 0U; i < g_ref_setting_map_count; i++) {
        if (g_ref_setting_map[i].public_setting_id == id) {
            return &g_ref_setting_map[i];
        }
    }
    return NULL;
}

/**
 * @brief REFから値を取得し、サイズを照合する（コピーしない）。
 *
 * @param[in]  ids   REFのData IDの配列
 * @param[in]  sizes 期待するREFの型のsizeofの配列
 * @param[in]  count 取得する数
 * @param[out] out   取得した値（ポインタとサイズ）の配列
 *
 * @retval EIF_OK           すべて取得した
 * @retval EIF_ERR_INTERNAL サイズが一致しない（型の不一致）、dataがNULL
 * @retval その他           RIM_GetBinary() の結果の変換結果
 */
static EIF_Result read_sources(const uint32_t *ids, const size_t *sizes, size_t count, RefView *out)
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

/**
 * @brief 対応表の1行に従ってREFから取得し、公開モデルへ変換する。
 *
 * @param[in]  m    情報の対応表の行
 * @param[out] out  公開モデルの書込み先
 * @param[in]  size out のサイズ
 *
 * @retval EIF_OK                取得・変換した
 * @retval EIF_ERR_INVALID_PARAM サイズが公開モデルと一致しない
 * @retval EIF_ERR_INTERNAL      変換関数が失敗、型の不一致
 * @retval その他                REFの取得失敗
 */
static EIF_Result read_public(const RefInfoMapping *m, void *out, size_t size)
{
    RefView v[EIF_REF_MAX_SOURCES];
    if (size != m->public_size) {
        return EIF_ERR_INVALID_PARAM;
    }
    const EIF_Result r = read_sources(m->ref_ids, m->ref_sizes, m->ref_count, v);
    if (r != EIF_OK) {
        return r;
    }
    return m->convert_func(v, m->ref_count, out, size) ? EIF_OK : EIF_ERR_INTERNAL;
}

/* ---------------------------------------------------------------- システム情報IF（EIF_SystemInfoIf）の実装 */

/**
 * @brief この機種で公開している情報か。
 * @param[in] id 公開情報ID
 * @return 情報の対応表に行があれば true
 */
static bool info_is_supported(eif_info_id_t id)
{
    return find_info(id) != NULL;
}

/**
 * @brief 公開モデルのサイズ。
 * @param[in] id 公開情報ID
 * @return 公開モデルのsizeof。公開していなければ0
 */
static size_t info_public_size(eif_info_id_t id)
{
    const RefInfoMapping *m = find_info(id);
    return (m != NULL) ? m->public_size : 0U;
}

/**
 * @brief 最新値を公開モデルで取得する（同期）。
 * @param[in]  id   公開情報ID
 * @param[out] out  公開モデルの書込み先
 * @param[in]  size out のサイズ
 * @retval EIF_OK              取得した
 * @retval EIF_ERR_NOT_EXPOSED 対応表に無い
 * @retval その他              read_public() の結果
 * @note Connectorのスレッド [C2] から、Frame経由で呼ばれる。
 */
static EIF_Result info_read(eif_info_id_t id, void *out, size_t size)
{
    const RefInfoMapping *m = find_info(id);
    return (m != NULL) ? read_public(m, out, size) : EIF_ERR_NOT_EXPOSED;
}

/**
 * @brief 対応表の行で、その通知方法を指定できるか。
 * @param[in] m    イベントの対応表の行
 * @param[in] type 通知方法
 * @return 指定できれば true
 */
static bool event_allows(const RefEventMapping *m, EIF_SubscriptionType type)
{
    return ((type == EIF_SUBSCRIBE_ON_CHANGE) && m->allow_on_change) ||
           ((type == EIF_SUBSCRIBE_PERIODIC) && m->allow_periodic);
}

/**
 * @brief その購読要求を、この機種が通知できるか（詳細設計書 4.3.7）。
 * @param[in] req 購読要求
 * @return イベントの対応表に行があり、指定の通知方法が許されていれば true
 */
static bool info_is_event_supported(const EIF_SubscriptionRequest *req)
{
    const RefEventMapping *m = (req != NULL) ? find_event(req->id) : NULL;
    return (m != NULL) && event_allows(m, req->type);
}

/**
 * @brief RIMへ登録する共通の通知トランポリン。購読IDから監視を引き、通知内容を公開イベントにしてFrameへ渡す。
 *
 * @param[in] subscription_id REFが払い出した購読ID
 * @param[in] message         REFの通知内容（変換関数へそのまま渡す）
 *
 * @note REFの通知ワーカー [C4] から呼ばれる。通知1件につき公開イベント1件。
 *       EIFは前回値を持たず、最新値の取り直しもしない。変換できない通知はFrameへ渡さない。
 *       監視の登録表は EIF_Start() 後は不変のため、ロックしない。
 */
static void on_ref_notified(uint64_t subscription_id, const void *message)
{
    for (size_t i = 0U; i < REF_MAX_WATCH; i++) {
        const WatchEntry *w = &s_watch[i];
        if (w->used && (w->subscription_id == subscription_id)) {
            EIF_Event ev;
            memset(&ev, 0, sizeof(ev));
            if ((message != NULL) && w->map->convert_func(w->map->public_event_id, message, &ev)) {
                w->fn(&ev, w->ctx); /* REFの通知contextのままFrameへ */
            }
            return;
        }
    }
    /* 未登録の購読IDは捨てる */
}

/**
 * @brief 購読要求1件をREFの購読要求へ変換して購読する（詳細設計書 4.3.6）。
 *
 * - id          → targetType＝データ、targetId＝対応表の ref_source_id
 * - type        → trigger（ON_CHANGE → RI_TRIGGER_ON_CHANGE、PERIODIC → RI_TRIGGER_PERIODIC）
 * - interval_ms → intervalMs
 * - method＝callback、callback＝on_ref_notified
 *
 * @param[in] req 購読要求
 * @param[in] fn  通知時に呼ぶFrameの関数
 * @param[in] ctx fn へそのまま渡す値（Frameの購読表の行）
 *
 * @retval EIF_OK              購読した
 * @retval EIF_ERR_NOT_EXPOSED 対応表に無い、指定できない通知方法、fn がNULL
 * @retval EIF_ERR_BUSY        監視の登録表が満杯
 * @retval その他              RIM_Subscribe() の結果の変換結果
 *
 * @note EIF_Start() から、起動処理のスレッド [C1] で呼ばれる。定期通知の周期の計時はREF。
 */
static EIF_Result info_watch_event(const EIF_SubscriptionRequest *req, EIF_InfoEventFn fn, void *ctx)
{
    const RefEventMapping *m = (req != NULL) ? find_event(req->id) : NULL;
    RI_SUBSCRIPTION_REQUEST rreq;
    uint64_t sub_id = 0U;

    if ((m == NULL) || (fn == NULL) || !event_allows(m, req->type)) {
        return EIF_ERR_NOT_EXPOSED;
    }
    for (size_t i = 0U; i < REF_MAX_WATCH; i++) {
        if (!s_watch[i].used) {
            memset(&rreq, 0, sizeof(rreq));
            rreq.targetType = RI_TARGET_DATA;
            rreq.targetId = m->ref_source_id;
            rreq.trigger = (req->type == EIF_SUBSCRIBE_PERIODIC) ? RI_TRIGGER_PERIODIC : RI_TRIGGER_ON_CHANGE;
            rreq.method = RI_METHOD_CALLBACK;
            rreq.intervalMs = req->interval_ms;
            rreq.callback = on_ref_notified; /* 通知時にREFから呼んでもらう関数（購読ハンドラ） */
            const RIStatus s = RIM_Subscribe(&rreq, &sub_id);
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
    .is_event_supported = info_is_event_supported,
    .watch_event = info_watch_event,
};

/* ---------------------------------------------------------------- 設定保存IF（EIF_SettingStoreIf）の実装：REFへ直接書き込む */

/**
 * @brief この機種で公開している設定か。
 * @param[in] id 公開設定ID
 * @return 設定の対応表に行があれば true
 */
static bool setting_is_supported(eif_setting_id_t id)
{
    return find_setting(id) != NULL;
}

/**
 * @brief マージ済みの公開モデルをREFの型へ変換し、REFへ書き込む。
 *
 * 手順：現在値（公開モデル）を取得 → 書込み先の現在値（REFの型）を取得してコピー
 *       → 変換関数で合成結果を反映 → RIM_SetBinary()。
 * 公開モデルに無いREFの項目は、現在値のコピーを土台にするため失われない。
 *
 * @param[in]  id         公開設定ID
 * @param[in]  merged     マージ済みの公開モデル
 * @param[in]  size       merged のサイズ
 * @param[out] out_ticket 使わない（REFへ直接書くため常に同期で完了する）
 *
 * @retval EIF_OK                書き込んだ
 * @retval EIF_ERR_NOT_EXPOSED   対応表に無い
 * @retval EIF_ERR_INVALID_PARAM 変換関数が失敗（値が不正、変更できない項目の変更）
 * @retval EIF_ERR_INTERNAL      対応表の不整合、作業領域不足
 * @retval その他                REFの取得・書込みの失敗
 *
 * @note Frameの SettingMerger から、そのmutexの中で呼ばれる（作業領域の利用は直列になる）。
 */
static EIF_Result setting_store(eif_setting_id_t id, const void *merged, size_t size, EIF_Ticket *out_ticket)
{
    static alignas(max_align_t) unsigned char current_public[REF_WORK_BYTES];
    static alignas(max_align_t) unsigned char out_ref[REF_WORK_BYTES];
    const RefSettingMapping *s = find_setting(id);
    RefView cur;

    (void)out_ticket;
    if (s == NULL) {
        return EIF_ERR_NOT_EXPOSED;
    }
    const RefInfoMapping *m = find_info(s->current_info_id);
    if ((m == NULL) || (size != m->public_size) || (size > REF_WORK_BYTES) || (s->ref_size > REF_WORK_BYTES)) {
        return EIF_ERR_INTERNAL;
    }
    /* Frameの設定マージは排他区間内で呼ぶため、ここでの静的領域の利用は直列になる */
    EIF_Result r = read_public(m, current_public, size);
    if (r == EIF_OK) {
        r = read_sources(&s->ref_id, &s->ref_size, 1U, &cur);
    }
    if (r != EIF_OK) {
        return r;
    }
    memcpy(out_ref, cur.data, s->ref_size); /* 公開モデルに無いREFの項目を保つ */
    if (!s->convert_func(merged, current_public, cur.data, out_ref, s->ref_size)) {
        return EIF_ERR_INVALID_PARAM;
    }
    return map_ri(RIM_SetBinary(s->ref_id, out_ref, s->ref_size));
}

const EIF_SettingStoreIf g_ref_setting_if = {
    .is_supported = setting_is_supported,
    .store = setting_store,
};

/** @brief 試験専用。監視の登録表を空にする（REF側の購読は解除しない）。 */
void ref_accessor_reset_for_test(void)
{
    memset(s_watch, 0, sizeof(s_watch));
}
