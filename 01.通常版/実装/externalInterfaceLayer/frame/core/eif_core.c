/**
 * @file eif_core.c
 * @brief 初期化・表の検証・開始（詳細設計書 7.1）。
 */
#include <stdatomic.h>

#include "eif_internal.h"
#include "util/eif_lock.h"
#include "util/eif_log.h"

typedef enum {
    EIF_STATE_NONE = 0,
    EIF_STATE_INITIALIZED,
    EIF_STATE_STARTED
} eif_state_t;

static atomic_int s_state = EIF_STATE_NONE;
static const EIF_ProductProfile *s_profile = NULL;
static const EIF_Ports *s_ports = NULL;
static eif_mutex_t s_init_lock = EIF_MUTEX_INITIALIZER;

const EIF_ProductProfile *eif_core_profile(void)
{
    return (atomic_load(&s_state) != EIF_STATE_NONE) ? s_profile : NULL;
}

const EIF_Ports *eif_core_ports(void)
{
    return (atomic_load(&s_state) != EIF_STATE_NONE) ? s_ports : NULL;
}

bool eif_core_is_started(void)
{
    return atomic_load(&s_state) == EIF_STATE_STARTED;
}

/* ---------------------------------------------------------------- 表の検証（詳細設計書 6.5.5） */

static bool sources_valid(const uint32_t *ids, const size_t *sizes, size_t count)
{
    (void)ids;
    if ((count == 0U) || (count > EIF_MAX_SOURCES)) {
        return false;
    }
    for (size_t i = 0U; i < count; i++) {
        if (sizes[i] == 0U) {
            return false;
        }
    }
    return true;
}

static bool validate_requests(const EIF_ProductProfile *p)
{
    for (size_t i = 0U; i < p->request_count; i++) {
        const EIF_RequestMapEntry *e = &p->requests[i];
        if ((e->public_id <= EIF_REQ_INVALID) || (e->public_id >= EIF_REQ_COUNT)) {
            EIF_LOG_ERROR("request map[%zu]: public id out of range", i);
            return false;
        }
        for (size_t j = i + 1U; j < p->request_count; j++) {
            if (p->requests[j].public_id == e->public_id) {
                EIF_LOG_ERROR("request map[%zu]: duplicated public id %d", j, (int)e->public_id);
                return false;
            }
        }
    }
    return true;
}

static bool validate_infos(const EIF_ProductProfile *p)
{
    for (size_t i = 0U; i < p->info_count; i++) {
        const EIF_InfoMapEntry *e = &p->infos[i];
        if ((e->public_id <= EIF_INFO_INVALID) || (e->public_id >= EIF_INFO_COUNT) || (e->convert == NULL) ||
            (e->public_size == 0U) || !sources_valid(e->source_ids, e->source_sizes, e->source_count)) {
            EIF_LOG_ERROR("info map[%zu]: invalid entry", i);
            return false;
        }
        for (size_t j = i + 1U; j < p->info_count; j++) {
            if (p->infos[j].public_id == e->public_id) {
                EIF_LOG_ERROR("info map[%zu]: duplicated public id %d", j, (int)e->public_id);
                return false;
            }
        }
    }
    return true;
}

static bool validate_settings(const EIF_ProductProfile *p)
{
    for (size_t i = 0U; i < p->setting_count; i++) {
        const EIF_SettingMapEntry *e = &p->settings[i];
        if ((e->public_id <= EIF_SETTING_INVALID) || (e->public_id >= EIF_SETTING_COUNT) || (e->merge == NULL) ||
            (e->ref_size == 0U) || (e->ref_size > EIF_SETTING_WORK_BYTES) || (e->patch_size == 0U)) {
            EIF_LOG_ERROR("setting map[%zu]: invalid entry", i);
            return false;
        }
        if (e->sink == EIF_SINK_PCL_REQUEST) {
            /* PCLが要求パラメータを受け付けるまでは使えない（QA-02） */
            EIF_LOG_ERROR("setting map[%zu]: sink PCL_REQUEST is not available yet (QA-02)", i);
            return false;
        }
        if (e->sink != EIF_SINK_REF_DIRECT) {
            return false;
        }
        for (size_t j = i + 1U; j < p->setting_count; j++) {
            if (p->settings[j].public_id == e->public_id) {
                EIF_LOG_ERROR("setting map[%zu]: duplicated public id %d", j, (int)e->public_id);
                return false;
            }
        }
    }
    return true;
}

static bool validate_subscriptions(const EIF_ProductProfile *p)
{
    if (p->subscription_count > EIF_MAX_SUBSCRIPTIONS) {
        EIF_LOG_ERROR("too many subscriptions: %zu", p->subscription_count);
        return false;
    }
    for (size_t i = 0U; i < p->subscription_count; i++) {
        const EIF_SubscriptionEntry *e = &p->subscriptions[i];
        if ((e->build_events == NULL) || !sources_valid(e->source_ids, e->source_sizes, e->source_count)) {
            EIF_LOG_ERROR("subscription[%zu]: invalid entry", i);
            return false;
        }
    }
    return true;
}

static bool allocate_cache(const EIF_ProductProfile *p)
{
    for (size_t i = 0U; i < p->subscription_count; i++) {
        const EIF_SubscriptionEntry *e = &p->subscriptions[i];
        if (e->keep_previous && !eif_cache_allocate(i, e->source_sizes[0])) {
            EIF_LOG_ERROR("subscription[%zu]: state cache capacity exceeded", i);
            return false;
        }
    }
    return true;
}

static bool ports_valid(const EIF_Ports *ports)
{
    return (ports->pcl != NULL) && (ports->info != NULL) && (ports->setting != NULL) &&
           (ports->pcl->submit != NULL) && (ports->pcl->cancel != NULL) && (ports->pcl->get_result != NULL) &&
           (ports->info->get_binary != NULL) && (ports->info->subscribe != NULL) &&
           (ports->setting->set_binary != NULL);
}

/* ---------------------------------------------------------------- 公開・製品CFG向けIF */

EIF_Result EIF_Initialize(const EIF_ProductProfile *profile, const EIF_Ports *ports)
{
    EIF_Result result = EIF_OK;

    if ((profile == NULL) || (ports == NULL)) {
        return EIF_ERR_INVALID_PARAM;
    }

    eif_mutex_lock(&s_init_lock);
    if (atomic_load(&s_state) != EIF_STATE_NONE) {
        eif_mutex_unlock(&s_init_lock); /* 多重呼出しは初期化済みを維持する */
        return EIF_OK;
    }

    if (!ports_valid(ports) || !validate_requests(profile) || !validate_infos(profile) ||
        !validate_settings(profile) || !validate_subscriptions(profile)) {
        result = EIF_ERR_INVALID_CONFIG;
    } else if (!allocate_cache(profile)) {
        eif_cache_reset();
        result = EIF_ERR_INVALID_CONFIG;
    } else {
        s_profile = profile;
        s_ports = ports;
        atomic_store(&s_state, EIF_STATE_INITIALIZED);
        EIF_LOG_INFO("initialized: product=%s", (profile->product_name != NULL) ? profile->product_name : "-");
    }
    eif_mutex_unlock(&s_init_lock);
    return result;
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
    result = eif_subscription_start();
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
    s_profile = NULL;
    s_ports = NULL;
    eif_registry_reset();
    eif_tracker_reset();
    eif_subscription_reset();
    eif_cache_reset();
    eif_mutex_unlock(&s_init_lock);
}
