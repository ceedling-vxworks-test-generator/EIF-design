/**
 * @file eif_core.c
 * @brief コアの初期化と開始。コアはPCL・REF・外部コンポーネント・機種を知らない。
 */
#include <stdatomic.h>

#include "eif_core_internal.h"
#include "eif_lock.h"
#include "eif_log.h"

typedef enum {
    EIF_STATE_NONE = 0,
    EIF_STATE_INITIALIZED,
    EIF_STATE_STARTED
} eif_state_t;

static atomic_int s_state = EIF_STATE_NONE;
static const EIF_DrivenPorts *s_ports = NULL;
static eif_mutex_t s_init_lock = EIF_MUTEX_INITIALIZER;

const EIF_DrivenPorts *eif_core_ports(void)
{
    return (atomic_load(&s_state) != EIF_STATE_NONE) ? s_ports : NULL;
}

bool eif_core_is_started(void)
{
    return atomic_load(&s_state) == EIF_STATE_STARTED;
}

static bool ports_valid(const EIF_DrivenPorts *p)
{
    return (p->control != NULL) && (p->info != NULL) && (p->setting != NULL) &&
           (p->control->is_supported != NULL) && (p->control->submit != NULL) && (p->control->cancel != NULL) &&
           (p->control->query != NULL) && (p->control->set_done_handler != NULL) &&
           (p->info->is_supported != NULL) && (p->info->public_size != NULL) && (p->info->read != NULL) &&
           (p->info->watch != NULL) && (p->setting->is_supported != NULL) && (p->setting->store != NULL);
}

EIF_Result EIF_Core_Initialize(const EIF_DrivenPorts *ports)
{
    if ((ports == NULL) || !ports_valid(ports)) {
        return EIF_ERR_INVALID_PARAM;
    }
    eif_mutex_lock(&s_init_lock);
    if (atomic_load(&s_state) == EIF_STATE_NONE) {
        s_ports = ports;
        ports->control->set_done_handler(eif_tracker_on_done); /* 完了の受け口（トランポリン）をつなぐ */
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

void EIF_Core_ResetForTest(void)
{
    eif_mutex_lock(&s_init_lock);
    atomic_store(&s_state, EIF_STATE_NONE);
    s_ports = NULL;
    eif_registry_reset();
    eif_tracker_reset();
    eif_watcher_reset();
    eif_cache_reset();
    eif_mutex_unlock(&s_init_lock);
}
