/**
 * @file fake_ports.h
 * @brief 試験用の偽物アダプタ（出口ポートの偽物）。EIFの型だけで作れるので、PCL・REFのヘッダーは要らない。
 */
#ifndef FAKE_PORTS_H
#define FAKE_PORTS_H

#include <stdbool.h>

#include "eif_driven_ports.h"

extern const EIF_DrivenPorts g_fake_ports;

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
} fake_t;
extern fake_t g_fake;

void fake_reset(void);

#endif /* FAKE_PORTS_H */
