/**
 * @file eif_core_api.h
 * @brief 組立て（composition）がコアへ出口ポートをつなぐ入口。
 */
#ifndef EIF_CORE_API_H
#define EIF_CORE_API_H

#include "eif_driven_ports.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 出口ポートをつなぎ、コアを初期化済みにする。
 * @param ports プログラム終了まで有効であること
 * @retval EIF_OK                 初期化済み（多重呼出しを含む）
 * @retval EIF_ERR_INVALID_PARAM  NULL、ポートの関数が欠けている
 */
EIF_Result EIF_Core_Initialize(const EIF_DrivenPorts *ports);

/** @brief 試験専用。全状態を未初期化へ戻す。 */
void EIF_Core_ResetForTest(void);

#ifdef __cplusplus
}
#endif

#endif /* EIF_CORE_API_H */
