/**
 * @file eif_request_tracker.c
 * @brief 要求台帳と、PCL完了の要求元への振り分け（詳細設計書 6.3.1、7.4）。
 *
 * 台帳が持つのは「どの要求の結果を、誰へ返すか」だけで、実行状態は持たない。
 * PCLのAPI呼出し中・Connectorのcallback呼出し中は台帳のmutexを保持しない。
 */
#include <stdatomic.h>
#include <string.h>

#include "eif_internal.h"
#include "util/eif_lock.h"
#include "util/eif_log.h"

/** @brief 台帳の容量＝PCLの未完了要求上限（32）＋先着完了用の余裕（8）。 */
#define EIF_TRACKER_CAPACITY (40U)

typedef enum {
    SLOT_FREE = 0,
    SLOT_ACTIVE,          /**< 受付済み、終端待ち */
    SLOT_EARLY_COMPLETED  /**< 台帳登録より先に完了が届いた */
} slot_state_t;

typedef struct {
    slot_state_t state;
    EIF_RequestHandle handle;
    uint64_t generation_id;
    EIF_ConnectorId origin;
    uint64_t origin_token;
    eif_request_id_t request_id;
    EIF_RequestOutcome early_outcome;
} slot_t;

static slot_t s_slots[EIF_TRACKER_CAPACITY];
static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER;
static atomic_uint_fast64_t s_next_handle = 1U;

static slot_t *find_by_generation(uint64_t generation_id, slot_state_t state)
{
    for (size_t i = 0U; i < EIF_TRACKER_CAPACITY; i++) {
        if ((s_slots[i].state == state) && (s_slots[i].generation_id == generation_id)) {
            return &s_slots[i];
        }
    }
    return NULL;
}

static slot_t *find_free(void)
{
    for (size_t i = 0U; i < EIF_TRACKER_CAPACITY; i++) {
        if (s_slots[i].state == SLOT_FREE) {
            return &s_slots[i];
        }
    }
    return NULL;
}

EIF_RequestHandle eif_tracker_next_handle(void)
{
    return (EIF_RequestHandle)atomic_fetch_add(&s_next_handle, 1U);
}

EIF_Result eif_tracker_register(uint64_t generation_id, EIF_ConnectorId origin, uint64_t origin_token,
                                eif_request_id_t request_id, EIF_RequestHandle handle, eif_track_result_t *out)
{
    out->early_completed = false;
    out->early_outcome = EIF_OUTCOME_PENDING;

    eif_mutex_lock(&s_lock);
    slot_t *early = find_by_generation(generation_id, SLOT_EARLY_COMPLETED);
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
    slot->generation_id = generation_id;
    slot->origin = origin;
    slot->origin_token = origin_token;
    slot->request_id = request_id;
    eif_mutex_unlock(&s_lock);
    return EIF_OK;
}

bool eif_tracker_find(EIF_RequestHandle handle, uint64_t *out_generation_id, EIF_ConnectorId *out_origin)
{
    bool found = false;

    eif_mutex_lock(&s_lock);
    for (size_t i = 0U; i < EIF_TRACKER_CAPACITY; i++) {
        if ((s_slots[i].state == SLOT_ACTIVE) && (s_slots[i].handle == handle)) {
            *out_generation_id = s_slots[i].generation_id;
            *out_origin = s_slots[i].origin;
            found = true;
            break;
        }
    }
    eif_mutex_unlock(&s_lock);
    return found;
}

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

void eif_tracker_on_pcl_completed(product_request_id_t product_request_id, uint64_t generation_id,
                                  external_request_execution_response_state_t result)
{
    slot_t copy;
    (void)product_request_id;

    eif_mutex_lock(&s_lock);
    slot_t *slot = find_by_generation(generation_id, SLOT_ACTIVE);
    if (slot == NULL) {
        /* 台帳登録より先に届いた完了。登録時に通知する */
        slot_t *free_slot = find_free();
        if (free_slot != NULL) {
            free_slot->state = SLOT_EARLY_COMPLETED;
            free_slot->generation_id = generation_id;
            free_slot->early_outcome = eif_map_outcome(result);
        } else {
            EIF_LOG_ERROR("tracker full: completion of generation %llu dropped", (unsigned long long)generation_id);
        }
        eif_mutex_unlock(&s_lock);
        return;
    }
    copy = *slot;
    memset(slot, 0, sizeof(*slot));
    eif_mutex_unlock(&s_lock);

    eif_tracker_notify(copy.origin, copy.handle, copy.request_id, copy.origin_token, eif_map_outcome(result));
}

void eif_tracker_reset(void)
{
    eif_mutex_lock(&s_lock);
    memset(s_slots, 0, sizeof(s_slots));
    atomic_store(&s_next_handle, 1U);
    eif_mutex_unlock(&s_lock);
}
