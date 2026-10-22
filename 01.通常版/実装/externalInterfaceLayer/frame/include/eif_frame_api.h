/**
 * @file eif_frame_api.h
 * @brief 製品CFGがFrameへ表を渡す入口。製品CFGの EIF_Setup() からだけ呼ぶ。
 */
#ifndef EIF_FRAME_API_H
#define EIF_FRAME_API_H

#include "eif_port.h"
#include "eif_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 前回値（StateCache）として保持できる合計バイト数。 */
#define EIF_STATE_CACHE_BYTES (4096U)
/** @brief 前回値を保持できる購読エントリ数。 */
#define EIF_STATE_CACHE_SLOTS (8U)
/** @brief 設定のマージに使う作業用バッファのサイズ（REF製品型の最大サイズ）。 */
#define EIF_SETTING_WORK_BYTES (512U)
/** @brief 購読エントリの最大数。 */
#define EIF_MAX_SUBSCRIPTIONS (16U)
/** @brief 1回の通知で生成できるイベントの最大数。 */
#define EIF_MAX_EVENTS_PER_NOTIFY (8U)

/**
 * @brief 表を検証して保持し、EIFを初期化済みにする。
 * @param profile プログラム終了まで有効であること
 * @param ports   プログラム終了まで有効であること
 * @retval EIF_OK                 初期化済み（多重呼出しを含む）
 * @retval EIF_ERR_INVALID_PARAM  NULL
 * @retval EIF_ERR_INVALID_CONFIG 表が不正
 */
EIF_Result EIF_Initialize(const EIF_ProductProfile *profile, const EIF_Ports *ports);

/** @brief 試験専用。全状態を未初期化へ戻す。製品から呼んではならない。 */
void EIF_Frame_ResetForTest(void);

#ifdef __cplusplus
}
#endif

#endif /* EIF_FRAME_API_H */
