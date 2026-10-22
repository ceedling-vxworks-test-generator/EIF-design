/**
 * @file eif_connector_registry.c
 * @brief Connectorの登録（設計書_ヘキサゴナル版 7.1）。登録は起動時だけで、EIF_Start()後は不変。
 */
#include <stdatomic.h>
#include <string.h>

#include "eif_core_internal.h"
#include "eif_lock.h"

static eif_connector_t s_connectors[EIF_MAX_CONNECTORS];
static size_t s_count = 0U;
static atomic_bool s_closed = false;
static eif_mutex_t s_lock = EIF_MUTEX_INITIALIZER;

EIF_Result EIF_RegisterConnector(const EIF_ConnectorDesc *desc, EIF_ConnectorId *out_id)
{
    if ((desc == NULL) || (out_id == NULL) || (desc->on_request_completed == NULL) ||
        ((desc->event_filter_count > 0U) && (desc->event_filter == NULL))) {
        return EIF_ERR_INVALID_PARAM;
    }

    eif_mutex_lock(&s_lock);
    if (atomic_load(&s_closed) || (s_count >= EIF_MAX_CONNECTORS)) {
        eif_mutex_unlock(&s_lock);
        return EIF_ERR_BUSY;
    }

    eif_connector_t *c = &s_connectors[s_count];
    memset(c, 0, sizeof(*c));
    c->id = (EIF_ConnectorId)(s_count + 1U);
    c->desc = *desc;
    c->desc.event_filter = NULL; /* 呼出元の配列は保持しない。ビット列へ写す */
    c->desc.event_filter_count = 0U;
    for (size_t i = 0U; i < desc->event_filter_count; i++) {
        const eif_event_id_t e = desc->event_filter[i];
        if ((e > EIF_EVT_INVALID) && (e < EIF_EVT_COUNT)) {
            c->event_mask |= (1UL << (unsigned)e);
        }
    }
    s_count++;
    *out_id = c->id;
    eif_mutex_unlock(&s_lock);
    return EIF_OK;
}

void eif_registry_close(void)
{
    eif_mutex_lock(&s_lock);
    atomic_store(&s_closed, true); /* 以降の読出しはロック不要（不変） */
    eif_mutex_unlock(&s_lock);
}

const eif_connector_t *eif_registry_get(EIF_ConnectorId id)
{
    if ((id == 0U) || (id > EIF_MAX_CONNECTORS)) {
        return NULL;
    }
    return eif_registry_at((size_t)id - 1U);
}

const eif_connector_t *eif_registry_at(size_t index)
{
    if (!atomic_load(&s_closed) || (index >= s_count)) {
        return NULL;
    }
    return &s_connectors[index];
}

void eif_registry_reset(void)
{
    eif_mutex_lock(&s_lock);
    memset(s_connectors, 0, sizeof(s_connectors));
    s_count = 0U;
    atomic_store(&s_closed, false);
    eif_mutex_unlock(&s_lock);
}
