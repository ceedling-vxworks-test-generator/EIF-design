/**
 * @file eif_product_setup.c
 * @brief 製品CFGの雛形。全ての表が空で、何も公開しない。新機種はこのフォルダをコピーして作る。
 */
#include "eif_frame_api.h"

static const EIF_ProductProfile s_profile = {
    .product_name = "Skeleton",
    .requests = NULL,
    .request_count = 0U,
    .infos = NULL,
    .info_count = 0U,
    .settings = NULL,
    .setting_count = 0U,
    .subscriptions = NULL,
    .subscription_count = 0U,
};

EIF_Result EIF_Setup(void)
{
    return EIF_Initialize(&s_profile, &g_eif_default_ports);
}
