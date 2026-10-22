/**
 * @file stub_accessors.h
 * @brief 試験用のスタブAccessor（下位IFのスタブ）。EIFの型だけで作れるので、PCL・REFのヘッダーは要らない。
 */
#ifndef STUB_ACCESSORS_H
#define STUB_ACCESSORS_H

#include <stdbool.h>

#include "eif_accessor_if.h"

extern const EIF_AccessorSet g_stub_accessors;

typedef struct {
    /* 製品制御 */
    eif_request_id_t supported_request;   /**< これだけを公開する */
    EIF_Result submit_result;
    bool complete_inline;                 /**< submit の中で完了を返す（先着完了の再現） */
    EIF_Ticket next_ticket;
    EIF_Ticket last_cancel;
    EIF_ControlDoneFn done;
    /* システム情報 */
    eif_network_settings_t network;
    EIF_Result read_result;
    EIF_InfoChangedFn watch_fn;
    void *watch_ctx;
    int watch_calls;
    /* 設定保存 */
    EIF_Result store_result;              /**< EIF_OK＝REF直接、EIF_ACCEPTED＝PCL経由の再現 */
    eif_network_settings_t stored;
    int store_calls;
} stub_t;
extern stub_t g_stub;

void stub_reset(void);

#endif /* STUB_ACCESSORS_H */
