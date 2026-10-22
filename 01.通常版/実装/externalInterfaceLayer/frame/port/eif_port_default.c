/**
 * @file eif_port_default.c
 * @brief PCL・REFの実APIを指す既定のPort（詳細設計書 6.7）。
 *
 * 試験ではこのファイルをリンクせず、スタブのPortを EIF_Initialize() へ渡す。
 */
#include "eif_port.h"

static const EIF_PclPort s_pcl = {
    .submit = ExternalRequestIngressSubmit,
    .cancel = ExternalRequestIngressCancelRequestedByOwnerGeneration,
    .get_result = ExternalRequestIngressGetResult,
};

static const EIF_InfoSourcePort s_info = {
    .get_binary = RIM_GetBinary,
    .subscribe = RIM_Subscribe,
};

static const EIF_SettingSinkPort s_setting = {
    .set_binary = RIM_SetBinary,
};

const EIF_Ports g_eif_default_ports = {
    .pcl = &s_pcl,
    .info = &s_info,
    .setting = &s_setting,
};
