/**
 * @file eif_compose.c
 * @brief MiniPrinterの組立て（設計書_ヘキサゴナル版 10章）。コアのどの出口ポートに、どのアダプタをつなぐかを決める。
 */
#include "eif_core_api.h"
#include "pcl_adapter.h"
#include "ref_adapter.h"

EIF_Result EIF_Setup(void)
{
    static const EIF_DrivenPorts ports = {
        .control = &g_pcl_control_port,  /* PCLアダプタ（MiniPrinterの対応表） */
        .info = &g_ref_info_port,        /* REFアダプタ（MiniPrinterの対応表） */
        .setting = &g_ref_setting_port,  /* REF書込み。PCL経由にする場合はここを差し替える（QA-01） */
    };
    return EIF_Core_Initialize(&ports);
}
