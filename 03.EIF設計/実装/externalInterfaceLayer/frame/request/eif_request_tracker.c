/**
 * @file eif_request_tracker.c
 * @brief 要求台帳と、完了の要求元への振り分け（詳細設計書 7.4）。
 *
 * 台帳が持つのは「どの要求の結果を、誰へ返すか」だけで、実行状態は持たない。
 * 下位IFの呼出し中・Connectorのcallback呼出し中は台帳のmutexを保持しない。
 * 台帳のキーはAccessorが払い出したチケットで、Frameは中身（PCLの受付ID等）を解釈しない。
 */
#include <stdatomic.h>
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"
#include "eif_log.h"

/** @brief 台帳の容量＝製品制御の未完了要求上限（32）＋先着完了用の余裕（8）。 */
#define EIF_TRACKER_CAPACITY (40U)

/** @brief 台帳エントリの状態。 */
typedef enum {
    SLOT_FREE = 0,
    SLOT_ACTIVE,          /**< 受付済み、終端待ち */
    SLOT_EARLY_COMPLETED  /**< 台帳登録より先に完了が届いた */
} slot_state_t;

/** @brief 台帳エントリ。 */
typedef struct {
    slot_state_t state;               /**< 状態 */
    EIF_RequestHandle handle;         /**< EIFが払い出した要求ハンドル */
    EIF_Ticket ticket;                /**< Accessorが払い出したチケット（検索キー） */
    EIF_ConnectorId origin;           /**< 要求元のConnector */
    uint64_t origin_token;            /**< 要求元トークン */
    eif_request_id_t request_id;      /**< 公開要求ID */
    EIF_RequestOutcome early_outcome; /**< 先着完了の終端結果（SLOT_EARLY_COMPLETED のときだけ有効） */
} slot_t;

static slot_t s_slots[EIF_TRACKER_CAPACITY];      /**< 要求台帳（固定長） */
static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER; /**< 台帳の保護 */
static atomic_uint_fast64_t s_next_handle = 1U;   /**< 次に払い出す要求ハンドル */

/**
 * @brief 指定の状態で、チケットが一致するエントリを探す。
 * @param[in] ticket チケット
 * @param[in] state  探す状態
 * @return 見つかったエントリ。無ければNULL
 * @pre s_lock を保持していること。
 */
static slot_t *find_by_ticket(EIF_Ticket ticket, slot_state_t state)
{
    for (size_t i = 0U; i < EIF_TRACKER_CAPACITY; i++) {
        if ((s_slots[i].state == state) && (s_slots[i].ticket == ticket)) {
            return &s_slots[i];
        }
    }
    return NULL;
}

/**
 * @brief 空きエントリを探す。
 * @return 空きエントリ。満杯ならNULL
 * @pre s_lock を保持していること。
 */
static slot_t *find_free(void)
{
    for (size_t i = 0U; i < EIF_TRACKER_CAPACITY; i++) {
        if (s_slots[i].state == SLOT_FREE) {
            return &s_slots[i];
        }
    }
    return NULL;
}

/**
 * @brief 要求ハンドルを採番する（atomic、単調増加、1から）。
 * @return 新しい要求ハンドル
 */
EIF_RequestHandle eif_tracker_next_handle(void)
{
    return (EIF_RequestHandle)atomic_fetch_add(&s_next_handle, 1U);
}

/**
 * @brief チケットを要求台帳へ登録する。同じチケットの先着完了があれば、それを取り出す。
 *
 * @param[in]  ticket       Accessorが払い出したチケット
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  origin_token 要求元トークン
 * @param[in]  request_id   公開要求ID
 * @param[in]  handle       採番済みの要求ハンドル
 * @param[out] out          先着完了の有無と、その終端結果
 *
 * @retval EIF_OK       登録した、または先着完了を取り出した（out->early_completed が true）
 * @retval EIF_ERR_BUSY 台帳が満杯
 *
 * @note Connectorのスレッド [C2] から呼ばれる。完了の通知は呼出し元が（mutexの外で）行う。
 */
EIF_Result eif_tracker_register(EIF_Ticket ticket, EIF_ConnectorId origin, uint64_t origin_token,
                                eif_request_id_t request_id, EIF_RequestHandle handle, eif_track_result_t *out)
{
    out->early_completed = false;
    out->early_outcome = EIF_OUTCOME_PENDING;

    eif_mutex_lock(&s_lock);
    slot_t *early = find_by_ticket(ticket, SLOT_EARLY_COMPLETED);
    if (early != NULL) {
        /* 先着完了：登録せずに解放し、呼出元が要求元へ通知する */
        out->early_completed = true;
        out->early_outcome = early->early_outcome;
        memset(early, 0, sizeof(*early));
        eif_mutex_unlock(&s_lock);
        return EIF_OK;
    }

    slot_t *slot = find_free();
    if (slot == NULL) {
        eif_mutex_unlock(&s_lock);
        return EIF_ERR_BUSY;
    }
    slot->state = SLOT_ACTIVE;
    slot->handle = handle;
    slot->ticket = ticket;
    slot->origin = origin;
    slot->origin_token = origin_token;
    slot->request_id = request_id;
    eif_mutex_unlock(&s_lock);
    return EIF_OK;
}

/**
 * @brief 要求ハンドルから、受付中の要求のチケットと要求元を引く。
 *
 * @param[in]  handle     要求ハンドル
 * @param[out] out_ticket チケット（見つかった場合だけ書く）
 * @param[out] out_origin 要求元のConnector ID（見つかった場合だけ書く）
 * @return 受付中（未終端）の要求なら true
 */
bool eif_tracker_find(EIF_RequestHandle handle, EIF_Ticket *out_ticket, EIF_ConnectorId *out_origin)
{
    bool found = false;

    eif_mutex_lock(&s_lock);
    for (size_t i = 0U; i < EIF_TRACKER_CAPACITY; i++) {
        if ((s_slots[i].state == SLOT_ACTIVE) && (s_slots[i].handle == handle)) {
            *out_ticket = s_slots[i].ticket;
            *out_origin = s_slots[i].origin;
            found = true;
            break;
        }
    }
    eif_mutex_unlock(&s_lock);
    return found;
}

/**
 * @brief 要求元のConnectorの on_request_completed を呼ぶ。
 *
 * @param[in] origin       要求元のConnector ID
 * @param[in] handle       要求ハンドル
 * @param[in] request_id   公開要求ID
 * @param[in] origin_token 要求元トークン
 * @param[in] outcome      終端結果
 *
 * @pre 台帳のmutexを保持していないこと（Connectorのcallbackからの再入でデッドロックしないため）。
 */
void eif_tracker_notify(EIF_ConnectorId origin, EIF_RequestHandle handle, eif_request_id_t request_id,
                        uint64_t origin_token, EIF_RequestOutcome outcome)
{
    const eif_connector_t *c = eif_registry_get(origin);
    if (c == NULL) {
        EIF_LOG_WARN("completion for unknown connector %u", (unsigned)origin);
        return;
    }
    c->desc.on_request_completed(handle, request_id, origin_token, outcome, c->desc.user_context);
}

/**
 * @brief 完了トランポリン。製品要求の完了を受け取り、要求元のConnectorへだけ通知する。
 *
 * 手順：台帳をチケットで検索 → エントリを局所変数へ写して解放 → mutexを放す → 要求元へ通知。
 * 台帳に無い場合は、登録より先に完了が届いた（先着完了）とみなして記録し、登録時に通知する。
 *
 * @param[in] ticket  完了した要求のチケット
 * @param[in] outcome 終端結果
 *
 * @note 製品制御IFの set_done_handler() で渡し、PCLの要求処理context [C3] から呼ばれる。
 */
void eif_tracker_on_done(EIF_Ticket ticket, EIF_RequestOutcome outcome)
{
    slot_t copy;

    eif_mutex_lock(&s_lock);
    slot_t *slot = find_by_ticket(ticket, SLOT_ACTIVE);
    if (slot == NULL) {
        /* 台帳登録より先に届いた完了。登録時に通知する */
        slot_t *free_slot = find_free();
        if (free_slot != NULL) {
            free_slot->state = SLOT_EARLY_COMPLETED;
            free_slot->ticket = ticket;
            free_slot->early_outcome = outcome;
        } else {
            EIF_LOG_ERROR("tracker full: completion of ticket %llu dropped", (unsigned long long)ticket);
        }
        eif_mutex_unlock(&s_lock);
        return;
    }
    copy = *slot;                 /* 通知に必要な情報を写してから */
    memset(slot, 0, sizeof(*slot)); /* エントリを解放する（完了は1回だけ） */
    eif_mutex_unlock(&s_lock);

    /* mutexの外でConnectorを呼ぶ */
    eif_tracker_notify(copy.origin, copy.handle, copy.request_id, copy.origin_token, outcome);
}

/** @brief 試験専用。要求台帳を空にし、採番を1へ戻す。 */
void eif_tracker_reset(void)
{
    eif_mutex_lock(&s_lock);
    memset(s_slots, 0, sizeof(s_slots));
    atomic_store(&s_next_handle, 1U);
    eif_mutex_unlock(&s_lock);
}
