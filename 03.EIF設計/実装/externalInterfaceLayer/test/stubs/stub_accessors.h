/**
 * @file stub_accessors.h
 * @brief 試験用のスタブAccessor（下位IFのスタブ）。EIFの型だけで作れるので、PCL・REFのヘッダーは要らない。
 *
 * 試験は g_stub の値で振る舞いを決め、呼ばれた内容を g_stub から確認する。
 * 完了・変化は、試験が g_stub.done／g_stub.watch_fn を直接呼んで起こす。
 */
#ifndef STUB_ACCESSORS_H
#define STUB_ACCESSORS_H

#include <stdbool.h>

#include "eif_accessor_if.h"

/** @brief スタブの下位IF一式（EIF_Frame_Initialize() へ渡す）。 */
extern const EIF_AccessorSet g_stub_accessors;

/** @brief スタブの設定値と記録。 */
typedef struct {
    /* 製品制御 */
    eif_request_id_t supported_request;   /**< これだけを公開する */
    EIF_Result submit_result;             /**< submit() が返す値 */
    bool complete_inline;                 /**< submit の中で完了を返す（先着完了の再現） */
    EIF_Ticket next_ticket;               /**< 最後に払い出したチケット */
    EIF_Ticket last_cancel;               /**< 最後に cancel() されたチケット */
    EIF_ControlDoneFn done;               /**< Frameから渡された完了トランポリン */
    /* システム情報 */
    eif_network_settings_t network;       /**< read() が返すネットワーク設定 */
    EIF_Result read_result;               /**< read() が返す値 */
    EIF_InfoChangedFn watch_fn;           /**< Frameから渡された変化の受け口 */
    void *watch_ctx;                      /**< watch() で渡された値 */
    int watch_calls;                      /**< watch() が呼ばれた回数 */
    /* 設定保存 */
    EIF_Result store_result;              /**< EIF_OK＝REF直接、EIF_ACCEPTED＝PCL経由の再現 */
    eif_network_settings_t stored;        /**< 最後に store() されたマージ結果 */
    int store_calls;                      /**< store() が呼ばれた回数 */
} stub_t;

/** @brief スタブの状態。 */
extern stub_t g_stub;

/** @brief スタブを既定値に戻す（公開要求は EIF_REQ_PRINT だけ、取得・保存は成功）。 */
void stub_reset(void);

#endif /* STUB_ACCESSORS_H */
