/**
 * @file eif_setup.c
 * @brief MiniPrinterの機種セットアップ（詳細設計書 6.5.4）。Frameの各下位IFに、どのAccessorをつなぐかを決める。
 */
#include "eif_frame_api.h"
#include "pcl_accessor.h"
#include "ref_accessor.h"

/**
 * @brief MiniPrinterでEIFを初期化する。
 *
 * 製品制御IFにPCL Accessor、システム情報IF・設定保存IFにREF Accessorをつなぐ。
 * どのAccessorも、MiniPrinterの対応表（models/MiniPrinter/）を使う。
 *
 * @retval EIF_OK                 初期化済み
 * @retval EIF_ERR_INVALID_CONFIG Accessorの関数が欠けている
 *
 * @note 起動処理のスレッド [C1] から1回呼ぶ。
 */
EIF_Result EIF_Setup(void)
{
    static const EIF_AccessorSet acc = {
        .control = &g_pcl_control_if,  /* PCL Accessor（MiniPrinterの対応表） */
        .info = &g_ref_info_if,        /* REF Accessor（MiniPrinterの対応表） */
        .setting = &g_ref_setting_if,  /* REF書込み。PCL経由にする場合はここを差し替える（QA-01） */
    };
    return EIF_Frame_Initialize(&acc);
}
