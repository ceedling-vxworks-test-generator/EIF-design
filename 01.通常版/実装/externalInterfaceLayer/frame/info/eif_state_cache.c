/**
 * @file eif_state_cache.c
 * @brief 差分導出のための前回値（詳細設計書 7.9）。EIFでデータを保持するのはここだけ。
 */
#include <stdbool.h>
#include <string.h>

#include "eif_internal.h"
#include "util/eif_lock.h"

typedef struct {
    bool allocated;
    bool has_value;
    size_t entry_index;
    size_t offset;
    size_t size;
    eif_mutex_t lock;
} cache_slot_t;

static unsigned char s_pool[EIF_STATE_CACHE_BYTES];
static size_t s_used = 0U;
static cache_slot_t s_slots[EIF_STATE_CACHE_SLOTS];

static cache_slot_t *slot_of(size_t entry_index)
{
    for (size_t i = 0U; i < EIF_STATE_CACHE_SLOTS; i++) {
        if (s_slots[i].allocated && (s_slots[i].entry_index == entry_index)) {
            return &s_slots[i];
        }
    }
    return NULL;
}

bool eif_cache_allocate(size_t entry_index, size_t size)
{
    if ((size == 0U) || (size > (EIF_STATE_CACHE_BYTES - s_used))) {
        return false;
    }
    for (size_t i = 0U; i < EIF_STATE_CACHE_SLOTS; i++) {
        if (!s_slots[i].allocated) {
            s_slots[i].allocated = true;
            s_slots[i].has_value = false;
            s_slots[i].entry_index = entry_index;
            s_slots[i].offset = s_used;
            s_slots[i].size = size;
            (void)pthread_mutex_init(&s_slots[i].lock, NULL);
            s_used += size;
            return true;
        }
    }
    return false;
}

void eif_cache_lock(size_t entry_index, const void **out_prev, size_t *out_size, bool *out_has_prev)
{
    cache_slot_t *s = slot_of(entry_index);
    *out_prev = NULL;
    *out_size = 0U;
    *out_has_prev = false;
    if (s == NULL) {
        return;
    }
    eif_mutex_lock(&s->lock);
    *out_prev = &s_pool[s->offset];
    *out_size = s->size;
    *out_has_prev = s->has_value;
}

void eif_cache_store_and_unlock(size_t entry_index, const void *data, size_t size)
{
    cache_slot_t *s = slot_of(entry_index);
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
    for (size_t i = 0U; i < EIF_STATE_CACHE_SLOTS; i++) {
        if (s_slots[i].allocated) {
            (void)pthread_mutex_destroy(&s_slots[i].lock);
        }
    }
    memset(s_slots, 0, sizeof(s_slots));
    memset(s_pool, 0, sizeof(s_pool));
    s_used = 0U;
}
