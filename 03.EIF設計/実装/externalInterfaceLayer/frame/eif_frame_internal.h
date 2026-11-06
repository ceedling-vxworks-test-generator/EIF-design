/**
 * @file eif_frame_internal.h
 * @brief Frame内部の部品間IF（Frameの外からは使わない。CMakeでPRIVATE include）。
 */
#ifndef EIF_FRAME_INTERNAL_H
#define EIF_FRAME_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_frame_api.h"

/** @brief 1つのConnectorが登録できる購読要求の数（公開イベントごとに1件まで）。 */
#define EIF_MAX_SUBSCRIPTIONS_PER_CONNECTOR ((size_t)EIF_EVT_COUNT)
/** @brief 購読表の行数の上限（REF Accessorの監視登録表の上限と同じ）。 */
#define EIF_MAX_SUBSCRIPTIONS (8U)
/** @brief 公開モデルの最大サイズ（作業用バッファ）。 */
#define EIF_PUBLIC_MODEL_MAX (2048U)

/* ================================================================ frame/core（eif_frame.c） */

/**
 * @brief 初期化時に保持した下位IF一式を返す。
 * @return 下位IF一式。未初期化ならNULL
 */
const EIF_AccessorSet *eif_frame_accessors(void);

/**
 * @brief 受付を開始済みか（EIF_Start() 済みか）。
 * @return 開始済みなら true
 */
bool eif_frame_is_started(void);

/* ================================================================ connector（eif_connector_registry.c） */

/** @brief Connector登録表の1行（詳細設計書 3.2.1 ConnectorEntry）。 */
typedef struct {
    EIF_ConnectorId id;                           /**< Connector ID（1〜EIF_MAX_CONNECTORS） */
    const char *name;                             /**< ログ用 */
    EIF_RequestCompletedFn on_request_completed;  /**< 完了callback */
    EIF_EventFn on_event;                         /**< イベントcallback（NULL可） */
    void *user_context;                           /**< callbackへそのまま渡す値 */
    EIF_SubscriptionRequest subscriptions[EIF_MAX_SUBSCRIPTIONS_PER_CONNECTOR]; /**< 登録時に写した購読要求 */
    size_t subscription_count;                    /**< subscriptions の件数 */
} ConnectorEntry;

/**
 * @brief その公開イベントで、その通知方法を指定できるか（公開イベントの定義。機種によらない）。
 * @param[in] id   公開イベントID
 * @param[in] type 通知方法
 * @return 指定できれば true
 */
bool eif_event_allows(eif_event_id_t id, EIF_SubscriptionType type);

/** @brief 登録を締め切る。以降の EIF_RegisterConnector() は EIF_ERR_BUSY。EIF_Start() から呼ぶ。 */
void eif_registry_close(void);

/**
 * @brief Connector IDから登録内容を引く。
 * @param[in] id Connector ID
 * @return 登録内容。未登録ならNULL
 * @note 登録は起動時だけで EIF_Start() 後は不変のため、ロックせずに読む。
 */
const ConnectorEntry *eif_registry_get(EIF_ConnectorId id);

/**
 * @brief 登録順の index 番目の登録内容を返す（配信で全Connectorを順に見るため）。
 * @param[in] index 0から
 * @return 登録内容。範囲外ならNULL
 */
const ConnectorEntry *eif_registry_at(size_t index);

/** @brief 試験専用。登録表を空にし、締切りを解除する。 */
void eif_registry_reset(void);

/* ================================================================ notify（eif_event_dispatcher.c） */

/**
 * @brief 公開イベントを、connector_mask に含まれるConnectorへ登録順に配信する。
 * @param[in] event          公開イベント（呼出し中だけ有効）
 * @param[in] connector_mask 配信先（bit (ID-1) ＝ Connector ID）。購読表の行の値
 * @note 呼出し元のcontext（REFの通知ワーカー [C4]）のまま、各Connectorの on_event を直接呼ぶ。
 */
void eif_dispatch_event(const EIF_Event *event, uint32_t connector_mask);

/* ================================================================ request（eif_request_tracker.c／eif_request_gateway.c） */

/**
 * @brief 要求ハンドルを採番する（atomic、単調増加、1から）。
 * @return 新しい要求ハンドル
 */
EIF_RequestHandle eif_tracker_next_handle(void);

/** @brief eif_tracker_register() の結果。 */
typedef struct {
    bool early_completed;              /**< 登録より先に完了が届いていた（先着完了） */
    EIF_RequestOutcome early_outcome;  /**< 先着完了の終端結果 */
} eif_track_result_t;

/**
 * @brief チケットを要求台帳へ登録する。
 *
 * 同じチケットの先着完了があれば、登録せずにそのエントリを解放し、out で知らせる。
 *
 * @param[in]  ticket       Accessorが払い出したチケット
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  origin_token 要求元トークン
 * @param[in]  request_id   公開要求ID
 * @param[in]  handle       採番済みの要求ハンドル
 * @param[out] out          先着完了の有無と、その終端結果
 *
 * @retval EIF_OK       登録した（または先着完了を取り出した）
 * @retval EIF_ERR_BUSY 台帳が満杯
 */
EIF_Result eif_tracker_register(EIF_Ticket ticket, EIF_ConnectorId origin, uint64_t origin_token,
                                eif_request_id_t request_id, EIF_RequestHandle handle, eif_track_result_t *out);

/**
 * @brief 要求ハンドルから、チケットと要求元を引く。
 * @param[in]  handle     要求ハンドル
 * @param[out] out_ticket チケット
 * @param[out] out_origin 要求元のConnector ID
 * @return 受付中（未終端）の要求なら true
 */
bool eif_tracker_find(EIF_RequestHandle handle, EIF_Ticket *out_ticket, EIF_ConnectorId *out_origin);

/**
 * @brief 完了トランポリン。Accessorから製品要求の完了を受け取り、要求元へ通知する。
 * @param[in] ticket  完了した要求のチケット
 * @param[in] outcome 終端結果
 * @note 製品制御IFの set_done_handler() で渡す。PCLの要求処理context [C3] から呼ばれる。
 */
void eif_tracker_on_done(EIF_Ticket ticket, EIF_RequestOutcome outcome);

/**
 * @brief 要求元のConnectorの on_request_completed を呼ぶ。
 * @param[in] origin       要求元のConnector ID
 * @param[in] handle       要求ハンドル
 * @param[in] request_id   公開要求ID
 * @param[in] origin_token 要求元トークン
 * @param[in] outcome      終端結果
 * @note 台帳のmutexを保持していない状態で呼ぶこと。
 */
void eif_tracker_notify(EIF_ConnectorId origin, EIF_RequestHandle handle, eif_request_id_t request_id,
                        uint64_t origin_token, EIF_RequestOutcome outcome);

/** @brief 試験専用。要求台帳を空にし、採番を1へ戻す。 */
void eif_tracker_reset(void);

/**
 * @brief チケットを台帳へ登録し、先着完了なら要求元へ通知する（要求受付とPCL経由の設定で共用）。
 *
 * @param[in]  ticket       Accessorが払い出したチケット
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  request_id   公開要求ID（設定変更では EIF_REQ_INVALID）
 * @param[in]  origin_token 要求元トークン
 * @param[out] out_handle   払い出した要求ハンドル
 *
 * @retval EIF_ACCEPTED 登録した（先着完了なら通知済み）
 * @retval EIF_ERR_BUSY 台帳が満杯（製品制御IFの cancel() で取消を依頼済み）
 */
EIF_Result eif_gateway_track(EIF_Ticket ticket, EIF_ConnectorId origin, eif_request_id_t request_id,
                             uint64_t origin_token, EIF_RequestHandle *out_handle);

/* ================================================================ notify（eif_watcher.c：購読表） */

/**
 * @brief 全Connectorの購読要求から購読表を作り、組合せごとにシステム情報IFへ購読を依頼する。EIF_Start() から呼ぶ。
 * @retval EIF_OK       依頼した（この機種で通知しない購読要求は飛ばす）
 * @retval EIF_ERR_BUSY 購読表の行数が上限（EIF_MAX_SUBSCRIPTIONS）を超える
 * @retval その他       watch_event() の失敗
 */
EIF_Result eif_watcher_start(void);

/** @brief 試験専用。購読表を空にする。 */
void eif_watcher_reset(void);

#endif /* EIF_FRAME_INTERNAL_H */
