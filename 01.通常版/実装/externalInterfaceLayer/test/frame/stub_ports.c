/**
 * @file stub_ports.c
 * @brief 試験用のPort（PCL・REFの代わり）。
 */
#include <string.h>

#include "stub_ports.h"

stub_pcl_t g_stub_pcl;
stub_ref_t g_stub_ref;

typedef struct {
    bool used;
    uint32_t id;
    unsigned char buf[512];
    size_t size;
} ref_data_t;

static ref_data_t s_data[STUB_REF_MAX_DATA];

/* ---------------------------------------------------------------- PCL */

static external_request_result_t pcl_submit(product_request_id_t id, external_request_completion_cb_t cb)
{
    external_request_result_t r;
    memset(&r, 0, sizeof(r));
    r.api_result = g_stub_pcl.submit_result;
    r.execution_result = EXTERNAL_REQUEST_EXECUTION_RESPONSE_PENDING;
    g_stub_pcl.last_product_request = id;
    g_stub_pcl.last_cb = cb;
    if (r.api_result == UC_OK) {
        r.generation_id = ++g_stub_pcl.next_generation;
        if (g_stub_pcl.complete_inline) {
            cb(id, r.generation_id, EXTERNAL_REQUEST_EXECUTION_RESPONSE_ACCEPTED);
        }
    }
    return r;
}

static uc_result_t pcl_cancel(uint64_t generation_id)
{
    g_stub_pcl.last_cancel_generation = generation_id;
    g_stub_pcl.cancel_calls++;
    return UC_OK;
}

static uc_result_t pcl_get_result(uint64_t generation_id, external_request_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->api_result = UC_OK;
    out->generation_id = generation_id;
    out->execution_result = g_stub_pcl.get_result_state;
    return UC_OK;
}

void stub_pcl_complete(uint64_t generation_id, external_request_execution_response_state_t state)
{
    g_stub_pcl.last_cb(g_stub_pcl.last_product_request, generation_id, state);
}

/* ---------------------------------------------------------------- REF */

static ref_data_t *find_data(uint32_t id)
{
    for (size_t i = 0U; i < STUB_REF_MAX_DATA; i++) {
        if (s_data[i].used && (s_data[i].id == id)) {
            return &s_data[i];
        }
    }
    return NULL;
}

void stub_ref_put(uint32_t data_id, const void *data, size_t size)
{
    ref_data_t *d = find_data(data_id);
    for (size_t i = 0U; (d == NULL) && (i < STUB_REF_MAX_DATA); i++) {
        if (!s_data[i].used) {
            d = &s_data[i];
            d->used = true;
            d->id = data_id;
        }
    }
    memcpy(d->buf, data, size);
    d->size = size;
}

const void *stub_ref_peek(uint32_t data_id, size_t *out_size)
{
    ref_data_t *d = find_data(data_id);
    *out_size = (d != NULL) ? d->size : 0U;
    return (d != NULL) ? d->buf : NULL;
}

static RIStatus ref_get_binary(uint32_t data_id, RI_BINARY *out)
{
    if (g_stub_ref.get_status != RI_SUCCESS) {
        return g_stub_ref.get_status;
    }
    ref_data_t *d = find_data(data_id);
    if (d == NULL) {
        return RI_NO_DATA;
    }
    out->data = d->buf;
    out->size = (uint32_t)d->size;
    return RI_SUCCESS;
}

static RIStatus ref_subscribe(const RI_SUBSCRIPTION_REQUEST *req, uint64_t *out_id)
{
    if (g_stub_ref.subscribe_status != RI_SUCCESS) {
        return g_stub_ref.subscribe_status;
    }
    g_stub_ref.subs[g_stub_ref.subscribe_calls] = *req;
    *out_id = 100U + (uint64_t)g_stub_ref.subscribe_calls;
    g_stub_ref.subscribe_calls++;
    return RI_SUCCESS;
}

static RIStatus ref_set_binary(uint32_t data_id, const void *data, size_t size)
{
    g_stub_ref.set_calls++;
    stub_ref_put(data_id, data, size);
    return RI_SUCCESS;
}

void stub_ref_notify(size_t subscription_index)
{
    g_stub_ref.subs[subscription_index].callback(100U + (uint64_t)subscription_index, NULL);
}

/* ---------------------------------------------------------------- 一式 */

static const EIF_PclPort s_pcl = { pcl_submit, pcl_cancel, pcl_get_result };
static const EIF_InfoSourcePort s_info = { ref_get_binary, ref_subscribe };
static const EIF_SettingSinkPort s_setting = { ref_set_binary };
const EIF_Ports g_stub_ports = { &s_pcl, &s_info, &s_setting };

void stub_ports_reset(void)
{
    memset(&g_stub_pcl, 0, sizeof(g_stub_pcl));
    memset(&g_stub_ref, 0, sizeof(g_stub_ref));
    memset(s_data, 0, sizeof(s_data));
    g_stub_pcl.submit_result = UC_OK;
    g_stub_pcl.get_result_state = EXTERNAL_REQUEST_EXECUTION_RESPONSE_PENDING;
    g_stub_ref.get_status = RI_SUCCESS;
    g_stub_ref.subscribe_status = RI_SUCCESS;
}
