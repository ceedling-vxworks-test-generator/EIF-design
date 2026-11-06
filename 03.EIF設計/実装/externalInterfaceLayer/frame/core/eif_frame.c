/**
 * @file eif_frame.c
 * @brief Frameの初期化と開始（詳細設計書 7.1）。FrameはPCL・REF・外部コンポーネント・機種を知らない。
 *
 * 状態は NONE → INITIALIZED（EIF_Frame_Initialize）→ STARTED（EIF_Start）の一方向に進む。
 * 状態の読出しは atomic、遷移は初期化用mutexで直列化する。
 */
#include <stdatomic.h>

#include "eif_frame_internal.h"
#include "eif_lock.h"
#include "eif_log.h"

/** @brief Frameの状態。 */
typedef enum {
    EIF_STATE_NONE = 0,     /**< 未初期化 */
    EIF_STATE_INITIALIZED,  /**< Accessorをつないだ。Connectorの登録を受け付ける */
    EIF_STATE_STARTED       /**< 受付中。Connectorの登録は締め切った */
} eif_state_t;

static atomic_int s_state = EIF_STATE_NONE;            /**< 現在の状態（全APIが参照する） */
static const EIF_AccessorSet *s_accessors = NULL;      /**< 機種セットアップから渡された下位IF一式 */
static eif_mutex_t s_init_lock = EIF_MUTEX_INITIALIZER; /**< 状態遷移の直列化 */

/**
 * @brief 初期化時に保持した下位IF一式を返す。
 * @return 下位IF一式。未初期化ならNULL
 */
const EIF_AccessorSet *eif_frame_accessors(void)
{
    return (atomic_load(&s_state) != EIF_STATE_NONE) ? s_accessors : NULL;
}

/**
 * @brief 受付を開始済みか。
 * @return EIF_Start() 済みなら true
 */
bool eif_frame_is_started(void)
{
    return atomic_load(&s_state) == EIF_STATE_STARTED;
}

/**
 * @brief 下位IF一式の関数がすべてそろっているかを検証する。
 * @param[in] p 下位IF一式（NULLでないこと）
 * @return すべてそろっていれば true
 */
static bool accessors_valid(const EIF_AccessorSet *p)
{
    return (p->control != NULL) && (p->info != NULL) && (p->setting != NULL) &&
           (p->control->is_supported != NULL) && (p->control->submit != NULL) && (p->control->cancel != NULL) &&
           (p->control->query != NULL) && (p->control->set_done_handler != NULL) &&
           (p->info->is_supported != NULL) && (p->info->public_size != NULL) && (p->info->read != NULL) &&
           (p->info->is_event_supported != NULL) && (p->info->watch_event != NULL) &&
           (p->setting->is_supported != NULL) && (p->setting->store != NULL);
}

/**
 * @brief Accessorをつなぎ、Frameを初期化済みにする。
 *
 * @param[in] acc 下位IF一式（プログラム終了まで有効であること）
 *
 * @retval EIF_OK                 初期化済み（多重呼出しを含む）
 * @retval EIF_ERR_INVALID_PARAM  acc がNULL
 * @retval EIF_ERR_INVALID_CONFIG 下位IFの関数が欠けている
 *
 * @note 起動処理のスレッド [C1] から、機種セットアップの EIF_Setup() 経由で呼ばれる。
 */
EIF_Result EIF_Frame_Initialize(const EIF_AccessorSet *acc)
{
    if (acc == NULL) {
        return EIF_ERR_INVALID_PARAM;
    }
    if (!accessors_valid(acc)) {
        return EIF_ERR_INVALID_CONFIG;
    }
    eif_mutex_lock(&s_init_lock);
    if (atomic_load(&s_state) == EIF_STATE_NONE) {
        s_accessors = acc;
        /* 完了の受け口（トランポリン）をPCL Accessorへ渡す。以降、PCLの完了は eif_tracker_on_done() に届く */
        acc->control->set_done_handler(eif_tracker_on_done);
        atomic_store(&s_state, EIF_STATE_INITIALIZED);
    }
    /* 初期化済みなら何もしない（多重呼出しは EIF_OK） */
    eif_mutex_unlock(&s_init_lock);
    return EIF_OK;
}

/**
 * @brief 購読要求をまとめてREFへ購読し、要求の受付を開始する（詳細設計書 4.2.8）。
 *
 * 手順：Connectorの登録を締め切る → 購読表を作り、組合せごとに購読を依頼する → STARTED にする。
 *
 * @retval EIF_OK              開始した（開始済みを含む）
 * @retval EIF_ERR_UNAVAILABLE 未初期化
 * @retval EIF_ERR_BUSY        購読表の行数が上限を超える
 * @retval その他              購読の依頼に失敗（eif_watcher_start() の結果）
 *
 * @note 起動処理のスレッド [C1] から1回呼ぶ。
 */
EIF_Result EIF_Start(void)
{
    EIF_Result result;

    eif_mutex_lock(&s_init_lock);
    if (atomic_load(&s_state) != EIF_STATE_INITIALIZED) {
        eif_mutex_unlock(&s_init_lock);
        return (atomic_load(&s_state) == EIF_STATE_STARTED) ? EIF_OK : EIF_ERR_UNAVAILABLE;
    }
    /* 登録表を不変にしてから受付を開始する（以降、登録表はロックなしで読める） */
    eif_registry_close();
    result = eif_watcher_start();
    if (result == EIF_OK) {
        atomic_store(&s_state, EIF_STATE_STARTED);
    }
    /* 失敗時はINITIALIZEDのまま。登録済みの購読はREFに解除APIが無いため残る */
    eif_mutex_unlock(&s_init_lock);
    return result;
}

/**
 * @brief 試験専用。Frameの全状態を未初期化へ戻す。
 * @warning 製品コードから呼ばないこと。Accessor側の状態（REFの購読等）は戻らない。
 */
void EIF_Frame_ResetForTest(void)
{
    eif_mutex_lock(&s_init_lock);
    atomic_store(&s_state, EIF_STATE_NONE);
    s_accessors = NULL;
    eif_registry_reset();
    eif_tracker_reset();
    eif_watcher_reset();
    eif_mutex_unlock(&s_init_lock);
}
