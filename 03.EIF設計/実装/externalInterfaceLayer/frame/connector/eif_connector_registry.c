/**
 * @file eif_connector_registry.c
 * @brief Connectorの登録（詳細設計書 3.2.1、4.2.4）。登録は起動時だけで、EIF_Start()後は不変。
 *
 * 登録中はmutexで保護する。EIF_Start() で締め切った後は不変になるため、
 * 読出し（完了通知・イベント配信のたびに呼ばれる）はロックせずに行う。
 */
#include <stdatomic.h>
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"

static ConnectorEntry s_connectors[EIF_MAX_CONNECTORS]; /**< 登録表（登録順。index＋1 ＝ Connector ID） */
static size_t s_count = 0U;                               /**< 登録済みの件数 */
static atomic_bool s_closed = false;                      /**< 締切り済み（EIF_Start() 後）なら true */
static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER;        /**< 登録中の保護 */

/** @brief 公開イベントの定義の1行。その公開イベントで指定できる通知方法（仕様設計書 9.3.2）。 */
typedef struct {
    eif_event_id_t id;    /**< 公開イベントID */
    bool on_change;       /**< 変化時（EIF_SUBSCRIBE_ON_CHANGE）を指定できる */
    bool periodic;        /**< 定期（EIF_SUBSCRIBE_PERIODIC）を指定できる */
} event_def_t;

/**
 * @brief 公開イベントの定義。機種によらない（機種が通知するかは下位IFの is_event_supported() で決まる）。
 * 公開イベントを追加したら、ここにも行を足す。表に無い公開イベントは購読要求で指定できない。
 */
static const event_def_t k_event_defs[] = {
    { EIF_EVT_ERROR_RAISED,      true, false },
    { EIF_EVT_ERROR_RESOLVED,    true, false },
    { EIF_EVT_WARNING_RAISED,    true, false },
    { EIF_EVT_WARNING_RESOLVED,  true, false },
    { EIF_EVT_INK_LEVEL_CHANGED, true, true  }, /* 状態の通知のため定期も指定できる */
    { EIF_EVT_JOB_PROGRESS,      true, false },
};
/** @brief 公開イベントの定義の行数。 */
#define EVENT_DEF_COUNT (sizeof(k_event_defs) / sizeof(k_event_defs[0]))

/**
 * @brief その公開イベントで、その通知方法を指定できるか。
 * @param[in] id   公開イベントID
 * @param[in] type 通知方法
 * @return 指定できれば true（範囲外の公開イベントID・通知方法は false）
 */
bool eif_event_allows(eif_event_id_t id, EIF_SubscriptionType type)
{
    for (size_t i = 0U; i < EVENT_DEF_COUNT; i++) {
        if (k_event_defs[i].id == id) {
            if (type == EIF_SUBSCRIBE_ON_CHANGE) {
                return k_event_defs[i].on_change;
            }
            if (type == EIF_SUBSCRIBE_PERIODIC) {
                return k_event_defs[i].periodic;
            }
            return false;
        }
    }
    return false;
}

/**
 * @brief 購読要求の配列の形を確かめる（機種には依存しない）。
 * @param[in] subs  購読要求の配列
 * @param[in] count 要素数
 * @return 正しければ true
 */
static bool subscriptions_valid(const EIF_SubscriptionRequest *subs, size_t count)
{
    if (count > EIF_MAX_SUBSCRIPTIONS_PER_CONNECTOR) {
        return false; /* 重複が無ければ公開イベントの数を超えない */
    }
    for (size_t i = 0U; i < count; i++) {
        const EIF_SubscriptionRequest *r = &subs[i];
        if ((r->id <= EIF_EVT_INVALID) || (r->id >= EIF_EVT_COUNT)) {
            return false; /* 範囲外の公開イベントID */
        }
        if ((r->type == EIF_SUBSCRIBE_ON_CHANGE) && (r->interval_ms != 0U)) {
            return false; /* 変化時に周期は指定しない */
        }
        if ((r->type == EIF_SUBSCRIBE_PERIODIC) && (r->interval_ms == 0U)) {
            return false; /* 定期には周期が要る */
        }
        if (!eif_event_allows(r->id, r->type)) {
            return false; /* その公開イベントで指定できない通知方法（範囲外の type を含む） */
        }
        for (size_t j = 0U; j < i; j++) {
            if (subs[j].id == r->id) {
                return false; /* 同じ公開イベントの重複 */
            }
        }
    }
    return true;
}

/**
 * @brief Connectorを登録する（詳細設計書 4.2.4）。
 *
 * desc をコピーし、購読要求は登録表へ写して保持する（呼出し元の配列は保持しない）。
 * 機種がその公開イベントを通知するかは、ここでは見ない（EIF_Start() で決まる）。
 *
 * @param[in]  desc   登録内容
 * @param[out] out_id 払い出したConnector ID（1から）
 *
 * @retval EIF_OK                登録した
 * @retval EIF_ERR_INVALID_PARAM desc・out_id がNULL、on_request_completed がNULL、
 *                               subscription_count>0 なのに subscriptions がNULL、購読要求が不正
 * @retval EIF_ERR_BUSY          締切り後、または登録数の上限
 *
 * @note 起動処理のスレッド [C1] から呼ぶ。
 */
EIF_Result EIF_RegisterConnector(const EIF_ConnectorDesc *desc, EIF_ConnectorId *out_id)
{
    if ((desc == NULL) || (out_id == NULL) || (desc->on_request_completed == NULL) ||
        ((desc->subscription_count > 0U) && (desc->subscriptions == NULL))) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!subscriptions_valid(desc->subscriptions, desc->subscription_count)) {
        return EIF_ERR_INVALID_PARAM;
    }

    eif_mutex_lock(&s_lock);
    if (atomic_load(&s_closed) || (s_count >= EIF_MAX_CONNECTORS)) {
        eif_mutex_unlock(&s_lock);
        return EIF_ERR_BUSY;
    }

    ConnectorEntry *c = &s_connectors[s_count];
    memset(c, 0, sizeof(*c));
    c->id = (EIF_ConnectorId)(s_count + 1U);
    c->name = desc->name;
    c->on_request_completed = desc->on_request_completed;
    c->on_event = desc->on_event;
    c->user_context = desc->user_context;
    /* 呼出元の配列は保持しない。登録表へ写す */
    for (size_t i = 0U; i < desc->subscription_count; i++) {
        c->subscriptions[i] = desc->subscriptions[i];
    }
    c->subscription_count = desc->subscription_count;
    s_count++;
    *out_id = c->id;
    eif_mutex_unlock(&s_lock);
    return EIF_OK;
}

/**
 * @brief 登録を締め切る。以降の登録は EIF_ERR_BUSY になり、登録表は不変になる。
 * @note EIF_Start() から呼ばれる。
 */
void eif_registry_close(void)
{
    eif_mutex_lock(&s_lock);
    atomic_store(&s_closed, true); /* 以降の読出しはロック不要（不変） */
    eif_mutex_unlock(&s_lock);
}

/**
 * @brief Connector IDから登録内容を引く。
 * @param[in] id Connector ID
 * @return 登録内容。未登録・締切り前ならNULL
 */
const ConnectorEntry *eif_registry_get(EIF_ConnectorId id)
{
    if ((id == 0U) || (id > EIF_MAX_CONNECTORS)) {
        return NULL;
    }
    return eif_registry_at((size_t)id - 1U);
}

/**
 * @brief 登録順の index 番目の登録内容を返す。
 * @param[in] index 0から
 * @return 登録内容。範囲外・締切り前ならNULL（締切り前は登録中の可能性があるため返さない）
 */
const ConnectorEntry *eif_registry_at(size_t index)
{
    if (!atomic_load(&s_closed) || (index >= s_count)) {
        return NULL;
    }
    return &s_connectors[index];
}

/** @brief 試験専用。登録表を空にし、締切りを解除する。 */
void eif_registry_reset(void)
{
    eif_mutex_lock(&s_lock);
    memset(s_connectors, 0, sizeof(s_connectors));
    s_count = 0U;
    atomic_store(&s_closed, false);
    eif_mutex_unlock(&s_lock);
}
