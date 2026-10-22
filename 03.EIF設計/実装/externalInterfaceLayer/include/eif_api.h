/**
 * @file eif_api.h
 * @brief EIFの公開API。外部コンポーネント側の部品（Connector）と起動処理がincludeする唯一のヘッダー。
 *
 * 仕様は「仕様設計書」10章を正とする。
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

/** @brief Connectorの識別子。0は無効。 */
typedef uint32_t EIF_ConnectorId;

/** @brief 要求ハンドル。0は無効。稼働中は再利用しない。 */
typedef uint64_t EIF_RequestHandle;

/** @brief 通知イベント。呼出し中だけ有効。 */
typedef struct {
    eif_event_id_t event_id;
    union {
        eif_alarm_t alarm; /**< EIF_EVT_ERROR_* / EIF_EVT_WARNING_* */
    } payload;
} EIF_Event;

/**
 * @brief 完了callback。要求を出したConnectorにだけ、終端時に1回呼ばれる。
 * @note PCLの要求処理contextから呼ばれる（EIF_SubmitRequest()の戻りより先に呼ばれることがある）。
 *       長時間ブロックしないこと。中からEIFの要求系APIを呼ばないこと。
 */
typedef void (*EIF_RequestCompletedFn)(EIF_RequestHandle handle,
                                       eif_request_id_t request_id,
                                       uint64_t origin_token,
                                       EIF_RequestOutcome outcome,
                                       void *user_context);

/**
 * @brief イベントcallback。event_filterに含めた公開イベントだけが届く。
 * @note REFの通知ワーカーから呼ばれる。長時間ブロックしないこと。
 */
typedef void (*EIF_EventFn)(const EIF_Event *event, void *user_context);

/** @brief Connectorの登録内容。EIFが内容をコピーする。 */
typedef struct {
    const char *name;                            /**< ログ用 */
    EIF_RequestCompletedFn on_request_completed; /**< 必須 */
    EIF_EventFn on_event;                        /**< 不要ならNULL */
    const eif_event_id_t *event_filter;          /**< 受け取る公開イベントID */
    size_t event_filter_count;
    void *user_context;                          /**< callbackへそのまま渡す */
} EIF_ConnectorDesc;

/** @brief 選択された機種でEIFを初期化する（機種セットアップ setup/<MODEL>/ が実装する）。 */
EIF_Result EIF_Setup(void);

/** @brief Connectorを登録する。EIF_Start()より前に呼ぶ。 */
EIF_Result EIF_RegisterConnector(const EIF_ConnectorDesc *desc, EIF_ConnectorId *out_id);

/** @brief 購読を登録し、受付を開始する。 */
EIF_Result EIF_Start(void);

/** @brief 公開要求を受け付ける。EIF_ACCEPTEDは受付完了であり、実行完了ではない。 */
EIF_Result EIF_SubmitRequest(EIF_ConnectorId origin,
                             eif_request_id_t request_id,
                             const void *param,
                             size_t param_size,
                             uint64_t origin_token,
                             EIF_RequestHandle *out_handle);

/** @brief 取消を要求する。最終結果は元の要求の完了callbackで届く。 */
EIF_Result EIF_CancelRequest(EIF_ConnectorId origin, EIF_RequestHandle handle);

/** @brief 結果を照会する（PCLへ委譲）。 */
EIF_Result EIF_GetRequestResult(EIF_RequestHandle handle, EIF_RequestOutcome *out_outcome);

/** @brief 公開情報を取得する。 */
EIF_Result EIF_GetInfo(eif_info_id_t info_id, void *out, size_t out_size);

/**
 * @brief 公開設定を部分更新する。
 * @retval EIF_OK       反映を完了した（out_handleは0）
 * @retval EIF_ACCEPTED 設定変更要求として受け付けた（完了callbackを待つ）
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
