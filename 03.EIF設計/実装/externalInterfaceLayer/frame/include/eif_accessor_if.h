/**
 * @file eif_accessor_if.h
 * @brief 下位IF（FrameがPCL・REFを使うときの呼出し口）。詳細設計書 6.2.2。
 *
 * Frameが定義し、型はEIF自身の型（公開ID・公開モデル・EIF_Result）だけで書く。
 * PCL・REFの型はこのヘッダーに出てこない。Accessor（accessors/）がこのIFを実装する。
 */
#ifndef EIF_ACCESSOR_IF_H
#define EIF_ACCESSOR_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Accessorが払い出す要求の識別子。0は無効。Frameは中身を解釈しない。 */
typedef uint64_t EIF_Ticket;

/* ---------------------------------------------------------------- 製品制御IF */

/** @brief 製品要求の完了を受け取るFrameの関数（トランポリン）。 */
typedef void (*EIF_ControlDoneFn)(EIF_Ticket ticket, EIF_RequestOutcome outcome);

typedef struct {
    bool (*is_supported)(eif_request_id_t id);                    /**< この機種で公開しているか */
    EIF_Result (*submit)(eif_request_id_t id, EIF_Ticket *out);   /**< EIF_ACCEPTED／EIF_ERR_BUSY 等 */
    EIF_Result (*cancel)(EIF_Ticket ticket);                      /**< EIF_OK＝取消を受付 */
    EIF_Result (*query)(EIF_Ticket ticket, EIF_RequestOutcome *out);
    void (*set_done_handler)(EIF_ControlDoneFn fn);               /**< 初期化時に1回 */
} EIF_ProductControlIf;

/* ---------------------------------------------------------------- システム情報IF */

/** @brief 情報の変化を受け取るFrameの関数。public_model は呼出し中だけ有効。 */
typedef void (*EIF_InfoChangedFn)(eif_info_id_t id, const void *public_model, size_t size, void *ctx);

typedef struct {
    bool (*is_supported)(eif_info_id_t id);
    size_t (*public_size)(eif_info_id_t id);                                   /**< 公開モデルのsizeof */
    EIF_Result (*read)(eif_info_id_t id, void *out_public, size_t size);
    EIF_Result (*watch)(eif_info_id_t id, EIF_InfoChangedFn fn, void *ctx);    /**< 変化（または定期）の通知を依頼 */
} EIF_SystemInfoIf;

/* ---------------------------------------------------------------- 設定保存IF */

typedef struct {
    bool (*is_supported)(eif_setting_id_t id);
    /**
     * @brief マージ済みの公開モデルを保存する。
     * @retval EIF_OK       保存完了（REFへ直接）
     * @retval EIF_ACCEPTED 受付（PCL経由。結果は製品制御IFの完了と同じ経路で届く。out_ticketを返す）
     */
    EIF_Result (*store)(eif_setting_id_t id, const void *merged_public, size_t size, EIF_Ticket *out_ticket);
} EIF_SettingStoreIf;

/* ---------------------------------------------------------------- 一式 */

typedef struct {
    const EIF_ProductControlIf *control;
    const EIF_SystemInfoIf *info;
    const EIF_SettingStoreIf *setting;
} EIF_AccessorSet;

#ifdef __cplusplus
}
#endif

#endif /* EIF_ACCESSOR_IF_H */
