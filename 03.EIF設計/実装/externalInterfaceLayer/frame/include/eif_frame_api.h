/**
 * @file eif_frame_api.h
 * @brief 機種セットアップ（setup/<MODEL>/）がFrameへAccessorをつなぐ入口。
 */
#ifndef EIF_FRAME_API_H
#define EIF_FRAME_API_H

#include "eif_accessor_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Accessorをつなぎ、Frameを初期化済みにする。
 * @param acc プログラム終了まで有効であること
 * @retval EIF_OK                 初期化済み（多重呼出しを含む）
 * @retval EIF_ERR_INVALID_PARAM  NULL
 * @retval EIF_ERR_INVALID_CONFIG 下位IFの関数が欠けている
 */
EIF_Result EIF_Frame_Initialize(const EIF_AccessorSet *acc);

/** @brief 試験専用。全状態を未初期化へ戻す。 */
void EIF_Frame_ResetForTest(void);

#ifdef __cplusplus
}
#endif

#endif /* EIF_FRAME_API_H */
