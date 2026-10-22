/**
 * @file eif_frame.c
 * @brief Frameの初期化と開始。FrameはPCL・REF・外部コンポーネント・機種を知らない。
 */
#include <stdatomic.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"
#include "eif_log.h"

typedef enum {
    EIF_STATE_NONE = 0,
    EIF_STATE_INITIALIZED,
    EIF_STATE_STARTED
} eif_state_t;

static atomic_int s_state = EIF_STATE_NONE;
static const EIF_AccessorSet *s_accessors = NULL;
static eif_mutex_t s_init_lock = EIF_MUTEX_INITIALIZER;

const EIF_AccessorSet *eif_frame_accessors(void)
{
    return (atomic_load(&s_state) != EIF_STATE_NONE) ? s_accessors : NULL;
}

bool eif_frame_is_started(void)
{
    return atomic_load(&s_state) == EIF_STATE_STARTED;
}

static bool accessors_valid(const EIF_AccessorSet *p)
{
    return (p->control != NULL) && (p->info != NULL) && (p->setting != NULL) &&
           (p->control->is_supported != NULL) && (p->control->submit != NULL) && (p->control->cancel != NULL) &&
           (p->control->query != NULL) && (p->control->set_done_handler != NULL) &&
           (p->info->is_supported != NULL) && (p->info->public_size != NULL) && (p->info->read != NULL) &&
           (p->info->watch != NULL) && (p->setting->is_supported != NULL) && (p->setting->store != NULL);
}

EIF_Result EIF_Frame_Initialize(const EIF_AccessorSet *acc)
{
    if (acc == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!accessors_valid(acc)) {
        return EIF_ERR_INVALID_CONFIG;
    }
    eif_mutex_lock(&s_init_lock);
    if (atomic_load(&s_state) == EIF_STATE_NONE) {
        s_accessors = acc;
        acc->control->set_done_handler(eif_tracker_on_done); /* 完了の受け口（トランポリン）をつなぐ */
        atomic_store(&s_state, EIF_STATE_INITIALIZED);
    }
    eif_mutex_unlock(&s_init_lock);
    return EIF_OK;
}

EIF_Result EIF_Start(void)
{
    EIF_Result result;

    eif_mutex_lock(&s_init_lock);
    if (atomic_load(&s_state) != EIF_STATE_INITIALIZED) {
        eif_mutex_unlock(&s_init_lock);
        return (atomic_load(&s_state) == EIF_STATE_STARTED) ? EIF_OK : EIF_ERR_UNAVAILABLE;
    }
    eif_registry_close();
    result = eif_watcher_start();
    if (result == EIF_OK) {
        atomic_store(&s_state, EIF_STATE_STARTED);
    }
    eif_mutex_unlock(&s_init_lock);
    return result;
}

void EIF_Frame_ResetForTest(void)
{
    eif_mutex_lock(&s_init_lock);
    atomic_store(&s_state, EIF_STATE_NONE);
    s_accessors = NULL;
    eif_registry_reset();
    eif_tracker_reset();
    eif_watcher_reset();
    eif_cache_reset();
    eif_mutex_unlock(&s_init_lock);
}
