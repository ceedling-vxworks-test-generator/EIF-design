/**
 * @file eif_state_cache.c
 * @brief 差分導出のための前回値（公開モデル）。EIFでデータを保持するのはここだけ。
 */
#include <string.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"

#define CACHE_SLOTS (8U)

typedef struct {
    bool allocated;
    bool has_value;
    eif_info_id_t id;
    size_t offset;
    size_t size;
    eif_mutex_t lock;
} slot_t;

static _Alignas(max_align_t) unsigned char s_pool[EIF_STATE_CACHE_BYTES];
static size_t s_used = 0U;
static slot_t s_slots[CACHE_SLOTS];

static slot_t *slot_of(eif_info_id_t id)
{
    for (size_t i = 0U; i < CACHE_SLOTS; i++) {
        if (s_slots[i].allocated && (s_slots[i].id == id)) {
            return &s_slots[i];
        }
    }
    return NULL;
}

bool eif_cache_allocate(eif_info_id_t id, size_t size)
{
    const size_t aligned = (size + 15U) & ~(size_t)15U;
    if (slot_of(id) != NULL) {
        return true;
    }
    if ((size == 0U) || (aligned > (EIF_STATE_CACHE_BYTES - s_used))) {
        return false;
    }
    for (size_t i = 0U; i < CACHE_SLOTS; i++) {
        if (!s_slots[i].allocated) {
            s_slots[i].allocated = true;
            s_slots[i].has_value = false;
            s_slots[i].id = id;
            s_slots[i].offset = s_used;
            s_slots[i].size = size;
            (void)pthread_mutex_init(&s_slots[i].lock, NULL);
            s_used += aligned;
            return true;
        }
    }
    return false;
}

void eif_cache_lock(eif_info_id_t id, const void **out_prev, bool *out_has_prev)
{
    slot_t *s = slot_of(id);
    *out_prev = NULL;
    *out_has_prev = false;
    if (s == NULL) {
        return;
    }
    eif_mutex_lock(&s->lock);
    *out_prev = &s_pool[s->offset];
    *out_has_prev = s->has_value;
}

void eif_cache_store_and_unlock(eif_info_id_t id, const void *data, size_t size)
{
    slot_t *s = slot_of(id);
    if (s == NULL) {
        return;
    }
    if ((data != NULL) && (size == s->size)) {
        memcpy(&s_pool[s->offset], data, size);
        s->has_value = true;
    }
    eif_mutex_unlock(&s->lock);
}

void eif_cache_reset(void)
{
    for (size_t i = 0U; i < CACHE_SLOTS; i++) {
        if (s_slots[i].allocated) {
            (void)pthread_mutex_destroy(&s_slots[i].lock);
        }
    }
    memset(s_slots, 0, sizeof(s_slots));
    memset(s_pool, 0, sizeof(s_pool));
    s_used = 0U;
}
