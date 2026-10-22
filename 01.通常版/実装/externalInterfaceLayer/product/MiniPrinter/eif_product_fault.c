/**
 * @file eif_product_fault.c
 * @brief MiniPrinterの購読の表と、異常情報リストの差分 → 発生／解消イベント（現行 ws/api_server_ws_callbacks.c を移設）。
 */
#include <stdio.h>
#include <string.h>

#include "eif_product_internal.h"

void mp_fault_to_alarm(RIM_FaultCode code, eif_alarm_t *out)
{
    /* 暫定：内部コードをそのまま番号化する。正式な公開アラームID・コードの割当ては別途確定する */
    memset(out, 0, sizeof(*out));
    (void)snprintf(out->alarm_id, sizeof(out->alarm_id), "MP%04u", (unsigned)code);
    (void)snprintf(out->alarm_code, sizeof(out->alarm_code), "E%05u", (unsigned)code);
}

static bool is_active(const RIM_FaultInfoList *list, RIM_FaultCode code)
{
    const uint32_t n = (list->count < RIM_FAULT_INFO_LIST_NUM) ? list->count : RIM_FAULT_INFO_LIST_NUM;
    for (uint32_t i = 0U; i < n; i++) {
        if ((list->items[i].code == code) && (list->items[i].state == RIM_FAULT_STATE_ACTIVE)) {
            return true;
        }
    }
    return false;
}

static size_t emit(const RIM_FaultInfoList *from, const RIM_FaultInfoList *other, bool other_valid,
                   eif_event_id_t event_id, EIF_Event *out, size_t pos, size_t max)
{
    const uint32_t n = (from->count < RIM_FAULT_INFO_LIST_NUM) ? from->count : RIM_FAULT_INFO_LIST_NUM;
    for (uint32_t i = 0U; (i < n) && (pos < max); i++) {
        const RIM_FaultInfo *f = &from->items[i];
        if ((f->state == RIM_FAULT_STATE_ACTIVE) && !(other_valid && is_active(other, f->code))) {
            out[pos].event_id = event_id;
            mp_fault_to_alarm(f->code, &out[pos].payload.alarm);
            pos++;
        }
    }
    return pos;
}

size_t mp_build_fault_events(const EIF_SourceView *now, size_t now_count, const void *prev, size_t prev_size,
                             bool has_prev, EIF_Event *out, size_t max)
{
    if ((now_count != 1U) || (now[0].size != sizeof(RIM_FaultInfoList))) {
        return 0U;
    }
    const RIM_FaultInfoList *cur = (const RIM_FaultInfoList *)now[0].data;
    const RIM_FaultInfoList *old = (has_prev && (prev_size == sizeof(RIM_FaultInfoList)))
                                       ? (const RIM_FaultInfoList *)prev : NULL;

    /* 今回あって前回なかった → 発生。前回あって今回ない → 解消。初回は発生中のものを全て発生とする */
    size_t pos = emit(cur, old, old != NULL, EIF_EVT_ERROR_RAISED, out, 0U, max);
    if (old != NULL) {
        pos = emit(old, cur, true, EIF_EVT_ERROR_RESOLVED, out, pos, max);
    }
    return pos;
}

const EIF_SubscriptionEntry g_mp_subscriptions[] = {
    { RI_TARGET_DATA, RI_DATA_FAULT_INFO_LIST, RI_TRIGGER_ON_CHANGE, 0U,
      { RI_DATA_FAULT_INFO_LIST }, { sizeof(RIM_FaultInfoList) }, 1U,
      true, mp_build_fault_events },
};
const size_t g_mp_subscription_count = sizeof(g_mp_subscriptions) / sizeof(g_mp_subscriptions[0]);
