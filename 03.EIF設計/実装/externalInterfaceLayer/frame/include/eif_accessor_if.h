/**
 * @file eif_accessor_if.h
 * @brief 下位IF（FrameがPCL・REFを使うときの呼出し口）。詳細設計書 6.2.2。
 *
 * Frameが定義し、型はEIF自身の型（公開ID・公開モデル・EIF_Result）だけで書く。
 * PCL・REFの型はこのヘッダーに出てこない。Accessor（accessors/）がこのIFを実装し、
 * 機種セットアップ（setup/<MODEL>/）が EIF_AccessorSet にまとめて EIF_Frame_Initialize() へ渡す。
 *
 * どの関数も呼出し元のスレッドのまま処理し、タスクを挟まないこと（性能方針「早く軽く」）。
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

/** @brief Accessorが払い出す要求の識別子（PCL AccessorではPCLの受付ID）。0は無効。Frameは中身を解釈しない。 */
typedef uint64_t EIF_Ticket;

/* ---------------------------------------------------------------- 製品制御IF */

/**
 * @brief 製品要求の完了を受け取るFrameの関数（トランポリン。実体は eif_tracker_on_done()）。
 *
 * @param[in] ticket  submit() が返したチケット
 * @param[in] outcome 終端結果
 *
 * @note Accessorは、PCLの完了callbackのcontext [C3] のまま呼ぶ。
 */
typedef void (*EIF_ControlDoneFn)(EIF_Ticket ticket, EIF_RequestOutcome outcome);

/** @brief 製品制御IF。PCLへの製品要求の送信・取消・照会。 */
typedef struct {
    /**
     * @brief この機種で公開している要求か（＝Accessorの対応表に行があるか）。
     * @param[in] id 公開要求ID
     * @return 公開していれば true
     */
    bool (*is_supported)(eif_request_id_t id);
    /**
     * @brief 製品要求を送信する。PCLの受付で即時に戻る（実行完了は待たない）。
     * @param[in]  id  公開要求ID
     * @param[out] out 受付成功時にチケット
     * @retval EIF_ACCEPTED 受付完了（完了は EIF_ControlDoneFn で届く。戻りより先に届くことがある）
     * @retval その他       PCLが拒否した等。完了は届かない
     */
    EIF_Result (*submit)(eif_request_id_t id, EIF_Ticket *out);
    /**
     * @brief 取消を依頼する。
     * @param[in] ticket 取り消す要求のチケット
     * @retval EIF_OK 取消を受け付けた（最終結果は EIF_ControlDoneFn で届く）
     */
    EIF_Result (*cancel)(EIF_Ticket ticket);
    /**
     * @brief 結果を照会する。
     * @param[in]  ticket 照会する要求のチケット
     * @param[out] out    終端結果、または EIF_OUTCOME_PENDING
     * @retval EIF_OK 照会できた
     */
    EIF_Result (*query)(EIF_Ticket ticket, EIF_RequestOutcome *out);
    /**
     * @brief 完了時に呼ぶFrameの関数を受け取る。初期化時に1回だけ呼ばれる。
     * @param[in] fn 完了トランポリン
     */
    void (*set_done_handler)(EIF_ControlDoneFn fn);
} EIF_ProductControlIf;

/* ---------------------------------------------------------------- システム情報IF */

/**
 * @brief 情報の変化を受け取るFrameの関数（実体は Watcher の on_info_changed()）。
 *
 * @param[in] id           変化した公開情報ID
 * @param[in] public_model 最新値（公開モデル。呼出し中だけ有効）
 * @param[in] size         public_model のサイズ
 * @param[in] ctx          watch() で渡した値
 *
 * @note Accessorは、REFの通知ワーカーのcontext [C4] のまま呼ぶ。
 */
typedef void (*EIF_InfoChangedFn)(eif_info_id_t id, const void *public_model, size_t size, void *ctx);

/** @brief システム情報IF。公開情報の取得と、変化の監視。 */
typedef struct {
    /**
     * @brief この機種で公開している情報か（＝Accessorの対応表に行があるか）。
     * @param[in] id 公開情報ID
     * @return 公開していれば true
     */
    bool (*is_supported)(eif_info_id_t id);
    /**
     * @brief 公開モデルのサイズ。
     * @param[in] id 公開情報ID
     * @return 公開モデルのsizeof。公開していなければ0
     */
    size_t (*public_size)(eif_info_id_t id);
    /**
     * @brief 最新値を公開モデルで取得する（同期）。
     * @param[in]  id         公開情報ID
     * @param[out] out_public 公開モデルの書込み先
     * @param[in]  size       out_public のサイズ
     * @retval EIF_OK 取得した
     */
    EIF_Result (*read)(eif_info_id_t id, void *out_public, size_t size);
    /**
     * @brief 変化（または定期）の通知を依頼する。通知契機は Accessor の対応表で決まる。
     * @param[in] id  公開情報ID
     * @param[in] fn  変化時に呼ぶFrameの関数
     * @param[in] ctx fn へそのまま渡す値
     * @retval EIF_OK 登録した
     */
    EIF_Result (*watch)(eif_info_id_t id, EIF_InfoChangedFn fn, void *ctx);
} EIF_SystemInfoIf;

/* ---------------------------------------------------------------- 設定保存IF */

/** @brief 設定保存IF。マージ済みの公開設定の書込み。書込み先（REF直接／PCL経由）はつなぐAccessorで決まる。 */
typedef struct {
    /**
     * @brief この機種で公開している設定か。
     * @param[in] id 公開設定ID
     * @return 公開していれば true
     */
    bool (*is_supported)(eif_setting_id_t id);
    /**
     * @brief マージ済みの公開モデルを保存する。
     * @param[in]  id            公開設定ID
     * @param[in]  merged_public マージ済みの公開モデル（現在値＋パッチ）
     * @param[in]  size          merged_public のサイズ
     * @param[out] out_ticket    EIF_ACCEPTED の場合にチケット
     * @retval EIF_OK       保存完了（REFへ直接）
     * @retval EIF_ACCEPTED 受付（PCL経由。結果は製品制御IFの完了と同じ経路で届く）
     */
    EIF_Result (*store)(eif_setting_id_t id, const void *merged_public, size_t size, EIF_Ticket *out_ticket);
} EIF_SettingStoreIf;

/* ---------------------------------------------------------------- 一式 */

/** @brief 下位IF一式。機種セットアップが static const で用意し、プログラム終了まで有効であること。 */
typedef struct {
    const EIF_ProductControlIf *control; /**< 製品制御IF（PCL Accessor） */
    const EIF_SystemInfoIf *info;        /**< システム情報IF（REF Accessor） */
    const EIF_SettingStoreIf *setting;   /**< 設定保存IF（REF Accessor、またはPCL経由の設定書込みAccessor） */
} EIF_AccessorSet;

#ifdef __cplusplus
}
#endif

#endif /* EIF_ACCESSOR_IF_H */
