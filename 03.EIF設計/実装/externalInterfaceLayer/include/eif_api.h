/**
 * @file eif_api.h
 * @brief EIFの公開API。外部コンポーネント側の部品（Connector）と起動処理がincludeする唯一のヘッダー。
 *
 * 仕様は「仕様設計書」10章を正とする。
 *
 * 呼出し順序（起動処理のスレッドから）：
 * -# EIF_Setup()             … 機種セットアップがFrameへAccessorをつなぐ
 * -# EIF_RegisterConnector() … 各Connectorが自分のcallbackと購読要求を登録する
 * -# EIF_Start()             … 購読要求をまとめてREFへ購読し、要求の受付を開始する
 *
 * 以降、要求系API（EIF_SubmitRequest() 等）は任意のスレッドから同時に呼んでよい。
 * EIFは自前のタスクを持たず、すべて呼出し元のスレッドのままPCL・REFまで処理する。
 */
#ifndef EIF_API_H
#define EIF_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "eif_model.h"
#include "eif_public_id.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 登録できるConnectorの最大数。 */
#define EIF_MAX_CONNECTORS (4U)

/** @brief 結果コード。 */
typedef enum {
    EIF_OK = 0,             /**< 完了 */
    EIF_ACCEPTED,           /**< 受付完了（結果は完了callback） */
    EIF_ERR_INVALID_PARAM,  /**< 引数・パラメータ不正 */
    EIF_ERR_NOT_EXPOSED,    /**< 公開対象外（この機種の対応表に無い） */
    EIF_ERR_NOT_FOUND,      /**< 要求ハンドル・情報が無い */
    EIF_ERR_BUSY,           /**< PCLが受け付けない、容量不足、登録不可 */
    EIF_ERR_UNAVAILABLE,    /**< 未初期化、未開始 */
    EIF_ERR_INVALID_CONFIG, /**< 機種セットアップの構成が不正（初期化時のみ） */
    EIF_ERR_INTERNAL        /**< その他内部エラー */
} EIF_Result;

/** @brief 要求の終端結果。 */
typedef enum {
    EIF_OUTCOME_COMPLETED = 0,     /**< 正常完了 */
    EIF_OUTCOME_INVALID = 1,       /**< 要求が不正 */
    EIF_OUTCOME_NOT_SUPPORTED = 2, /**< 下位機能が未対応 */
    EIF_OUTCOME_FAILED = 3,        /**< 実行失敗 */
    EIF_OUTCOME_CANCELED = 4,      /**< 取消で終端 */
    EIF_OUTCOME_PENDING = 5        /**< 未確定（照会時のみ。完了callbackには渡さない） */
} EIF_RequestOutcome;

/** @brief Connectorの識別子。EIF_RegisterConnector() が払い出す。0は無効。 */
typedef uint32_t EIF_ConnectorId;

/** @brief 要求ハンドル。受付成功時にEIFが払い出す。0は無効。稼働中は再利用しない。 */
typedef uint64_t EIF_RequestHandle;

/** @brief 通知イベント。callbackの呼出し中だけ有効（必要ならConnectorがコピーする）。 */
typedef struct {
    eif_event_id_t event_id; /**< 公開イベントID。payloadのどのメンバが有効かを決める */
    union {
        eif_alarm_t alarm; /**< EIF_EVT_ERROR_* / EIF_EVT_WARNING_* */
    } payload;
} EIF_Event;

/**
 * @brief 完了callback。要求を出したConnectorにだけ、終端時に1回呼ばれる。
 *
 * @param[in] handle       EIF_SubmitRequest()（またはEIF_PatchSetting()）が払い出した要求ハンドル
 * @param[in] request_id   公開要求ID（PCL経由の設定変更では EIF_REQ_INVALID）
 * @param[in] origin_token 要求時にConnectorが渡した値をそのまま返す
 * @param[in] outcome      終端結果（EIF_OUTCOME_PENDING は渡さない）
 * @param[in] user_context 登録時の EIF_ConnectorDesc::user_context
 *
 * @note PCLの要求処理context [C3] から呼ばれる。EIF_SubmitRequest() の戻りより先に呼ばれることがある（先着完了）。
 *       長時間ブロックしないこと。中からEIFの要求系APIを呼ばないこと。
 */
typedef void (*EIF_RequestCompletedFn)(EIF_RequestHandle handle,
                                       eif_request_id_t request_id,
                                       uint64_t origin_token,
                                       EIF_RequestOutcome outcome,
                                       void *user_context);

/**
 * @brief イベントcallback。購読要求（EIF_ConnectorDesc::subscriptions）で指定した公開イベントだけが届く。
 *
 * @param[in] event        公開イベント（呼出し中だけ有効）
 * @param[in] user_context 登録時の EIF_ConnectorDesc::user_context
 *
 * @note REFの通知ワーカー [C4] から呼ばれる。長時間ブロックしないこと。
 */
typedef void (*EIF_EventFn)(const EIF_Event *event, void *user_context);

/** @brief 購読要求の通知方法。値は末尾への追加だけを行う（ABI）。 */
typedef enum {
    EIF_SUBSCRIBE_ON_CHANGE = 0,  /**< 変化したときに通知 */
    EIF_SUBSCRIBE_PERIODIC = 1    /**< 一定周期で通知（値が変わらなくても通知。周期の計時はREF） */
} EIF_SubscriptionType;

/**
 * @brief 購読要求。受け取りたい公開イベント1つと、その通知方法（仕様設計書 9.2.4、9.3.3）。
 *
 * EIF_ConnectorDesc::subscriptions に配列で渡す。EIF_Start() が全Connectorの購読要求をまとめ、
 * 「公開イベント・通知方法・周期」の組合せごとにREFの購読を1回だけ登録する。
 */
typedef struct {
    eif_event_id_t id;          /**< 受け取りたい公開イベントID。1つのConnectorの中で重複しないこと */
    EIF_SubscriptionType type;  /**< 通知方法（その公開イベントで指定できるもの） */
    uint32_t interval_ms;       /**< 定期通知の周期[ms]。PERIODIC では1以上、ON_CHANGE では0 */
} EIF_SubscriptionRequest;

/** @brief Connectorの登録内容。EIFが内容をコピーする（関数とuser_contextの指す先はプログラム終了まで有効であること）。 */
typedef struct {
    const char *name;                              /**< ログ用 */
    EIF_RequestCompletedFn on_request_completed;   /**< 必須 */
    EIF_EventFn on_event;                          /**< 不要ならNULL */
    const EIF_SubscriptionRequest *subscriptions;  /**< 購読要求の配列（登録後に解放してよい） */
    size_t subscription_count;                     /**< subscriptions の要素数。0なら何も受け取らない */
    void *user_context;                            /**< callbackへそのまま渡す */
} EIF_ConnectorDesc;

/**
 * @brief 選択された機種でEIFを初期化する。
 *
 * 機種セットアップ（setup/<MODEL>/eif_setup.c）が実装し、FrameへAccessorをつなぐ。
 * 起動処理から1回呼ぶ。多重呼出しは初期化済みのまま EIF_OK を返す。
 *
 * @retval EIF_OK                 初期化済み
 * @retval EIF_ERR_INVALID_CONFIG Accessorの関数が欠けている
 */
EIF_Result EIF_Setup(void);

/**
 * @brief Connectorを登録する。EIF_Start() より前に、起動処理のスレッドから呼ぶ。
 *
 * @param[in]  desc   登録内容（EIFがコピーする）
 * @param[out] out_id 払い出したConnector ID
 *
 * @retval EIF_OK                登録した
 * @retval EIF_ERR_INVALID_PARAM desc・out_id がNULL、on_request_completed がNULL、
 *                               subscription_count が1以上なのに subscriptions がNULL、購読要求が不正
 *                               （範囲外の公開イベントID、type と interval_ms の組合せ、
 *                                その公開イベントで指定できない通知方法、同じ公開イベントの重複）
 * @retval EIF_ERR_BUSY          登録数の上限（EIF_MAX_CONNECTORS）、または EIF_Start() 後
 *
 * @note 機種がその公開イベントを通知しない場合はエラーにしない（通知が届かないだけ）。
 */
EIF_Result EIF_RegisterConnector(const EIF_ConnectorDesc *desc, EIF_ConnectorId *out_id);

/**
 * @brief 全Connectorの購読要求をまとめてREFへ購読し、要求の受付を開始する。
 *
 * 「公開イベント・通知方法・周期」の組合せごとにREFの購読を1回だけ登録する。
 * 機種が通知しない公開イベントは購読しない（エラーにしない）。
 * 以降、Connectorの登録はできない。起動処理のスレッドから1回呼ぶ。
 *
 * @retval EIF_OK              開始した（開始済みを含む）
 * @retval EIF_ERR_UNAVAILABLE EIF_Setup() が未実行
 * @retval EIF_ERR_BUSY        購読要求の組合せが上限（8）を超える（REFへの購読はしない）
 * @retval その他              購読の登録に失敗（登録済みの購読は残る）
 */
EIF_Result EIF_Start(void);

/**
 * @brief 公開要求を受け付け、PCLへ転送する。
 *
 * EIF_ACCEPTED は受付完了であり、実行完了ではない。終端結果は origin の完了callbackで届く。
 * 失敗時は要求を登録せず、後から完了callbackは呼ばない。
 *
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  request_id   公開要求ID
 * @param[in]  param        要求パラメータ（現時点ではNULLのみ。QA-02）
 * @param[in]  param_size   param のサイズ（現時点では0のみ）
 * @param[in]  origin_token 完了callbackでそのまま返す値（Connectorが自分の要求と結果を対応付けるため）
 * @param[out] out_handle   受付成功時に要求ハンドル、失敗時は0
 *
 * @retval EIF_ACCEPTED          受付完了
 * @retval EIF_ERR_INVALID_PARAM 引数不正、未登録のorigin
 * @retval EIF_ERR_NOT_EXPOSED   この機種で公開していない要求
 * @retval EIF_ERR_BUSY          PCLが受け付けない、要求台帳が満杯
 * @retval EIF_ERR_UNAVAILABLE   未開始
 */
EIF_Result EIF_SubmitRequest(EIF_ConnectorId origin,
                             eif_request_id_t request_id,
                             const void *param,
                             size_t param_size,
                             uint64_t origin_token,
                             EIF_RequestHandle *out_handle);

/**
 * @brief 取消を要求する。最終結果は元の要求の完了callbackで届く。
 *
 * @param[in] origin 要求元のConnector ID（要求を出したConnector以外は取消できない）
 * @param[in] handle 取り消す要求の要求ハンドル
 *
 * @retval EIF_OK                取消を受け付けた（取消の完了ではない）
 * @retval EIF_ERR_INVALID_PARAM handle が0
 * @retval EIF_ERR_NOT_FOUND     未登録、終端済み、または origin が要求元と異なる
 * @retval EIF_ERR_BUSY          PCLが取消を受け付けない
 * @retval EIF_ERR_UNAVAILABLE   未開始
 */
EIF_Result EIF_CancelRequest(EIF_ConnectorId origin, EIF_RequestHandle handle);

/**
 * @brief 要求の結果を照会する（PCLへ委譲。EIFは結果を保持しない）。
 *
 * @param[in]  handle      要求ハンドル
 * @param[out] out_outcome 未完了なら EIF_OUTCOME_PENDING、終端済みなら終端結果
 *
 * @retval EIF_OK                照会できた
 * @retval EIF_ERR_INVALID_PARAM handle が0、out_outcome がNULL
 * @retval EIF_ERR_NOT_FOUND     要求台帳に無い（終端済み・未登録）
 * @retval EIF_ERR_UNAVAILABLE   未開始
 */
EIF_Result EIF_GetRequestResult(EIF_RequestHandle handle, EIF_RequestOutcome *out_outcome);

/**
 * @brief 公開情報を取得する。毎回REFから最新値を取得し、公開モデルへ変換して返す（同期）。
 *
 * @param[in]  info_id  公開情報ID
 * @param[out] out      公開モデルの書込み先（例：EIF_INFO_NETWORK_SETTINGS なら eif_network_settings_t）
 * @param[in]  out_size out のサイズ（公開モデルのsizeofと一致すること）
 *
 * @retval EIF_OK                取得した
 * @retval EIF_ERR_INVALID_PARAM out がNULL、サイズ不一致
 * @retval EIF_ERR_NOT_EXPOSED   この機種で公開していない情報
 * @retval EIF_ERR_NOT_FOUND     REFに値が無い
 * @retval EIF_ERR_UNAVAILABLE   未開始、REF未初期化
 * @retval EIF_ERR_INTERNAL      変換失敗、REFの型の不一致
 */
EIF_Result EIF_GetInfo(eif_info_id_t info_id, void *out, size_t out_size);

/**
 * @brief 公開設定を部分更新する（パッチで指定しなかった項目は現在値のまま）。
 *
 * 書込み先（REFへ直接／PCL経由）は機種セットアップで決まる。Connectorは戻り値で扱いを分ける。
 *
 * @param[in]  origin       要求元のConnector ID
 * @param[in]  setting_id   公開設定ID
 * @param[in]  patch        パッチモデル（例：EIF_SETTING_NETWORK なら eif_network_settings_patch_t）
 * @param[in]  patch_size   patch のサイズ（パッチモデルのsizeofと一致すること）
 * @param[in]  origin_token EIF_ACCEPTED の場合に完了callbackでそのまま返す値
 * @param[out] out_handle   EIF_ACCEPTED の場合に要求ハンドル、それ以外は0
 *
 * @retval EIF_OK                反映を完了した（REFへ直接書き込む設定）
 * @retval EIF_ACCEPTED          設定変更要求として受け付けた（PCL経由。完了callbackを待つ）
 * @retval EIF_ERR_INVALID_PARAM 引数不正、未登録のorigin、変更できない項目の変更
 * @retval EIF_ERR_NOT_EXPOSED   この機種で公開していない設定
 * @retval EIF_ERR_BUSY          PCLが受け付けない（PCL経由の場合）
 * @retval EIF_ERR_UNAVAILABLE   未開始
 * @retval EIF_ERR_INTERNAL      その他
 */
EIF_Result EIF_PatchSetting(EIF_ConnectorId origin,
                            eif_setting_id_t setting_id,
                            const void *patch,
                            size_t patch_size,
                            uint64_t origin_token,
                            EIF_RequestHandle *out_handle);

#ifdef __cplusplus
}
#endif

#endif /* EIF_API_H */
