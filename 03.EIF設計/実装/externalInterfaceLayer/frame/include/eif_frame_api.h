/**
 * @file eif_frame_api.h
 * @brief 機種セットアップ（setup/<MODEL>/）がFrameへAccessorをつなぐ入口。
 *
 * 機種セットアップと試験だけがincludeする。Connectorは使わない（eif_api.h だけを使う）。
 */
#ifndef EIF_FRAME_API_H
#define EIF_FRAME_API_H

#include "eif_accessor_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Accessorをつなぎ、Frameを初期化済みにする。
 *
 * 下位IFの関数がすべてそろっているかを検証し、参照を保持する。
 * 製品制御IFへ完了トランポリン（eif_tracker_on_done）を渡す。
 * 機種セットアップの EIF_Setup() から、起動処理のスレッド [C1] で呼ぶ。
 *
 * @param[in] acc 下位IF一式（プログラム終了まで有効であること）
 *
 * @retval EIF_OK                 初期化済み（多重呼出しを含む）
 * @retval EIF_ERR_INVALID_PARAM  acc がNULL
 * @retval EIF_ERR_INVALID_CONFIG 下位IFの関数が欠けている（未初期化のまま）
 */
EIF_Result EIF_Frame_Initialize(const EIF_AccessorSet *acc);

/**
 * @brief 試験専用。Frameの全状態（初期化状態・登録表・要求台帳・前回値）を未初期化へ戻す。
 * @warning 製品コードから呼ばないこと。
 */
void EIF_Frame_ResetForTest(void);

#ifdef __cplusplus
}
#endif

#endif /* EIF_FRAME_API_H */
