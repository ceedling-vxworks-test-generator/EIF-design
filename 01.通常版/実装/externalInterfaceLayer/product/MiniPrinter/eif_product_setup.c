/**
 * @file eif_product_setup.c
 * @brief MiniPrinterの EIF_Setup()。表を組み立てて Frame へ渡す。
 */
#include "eif_frame_api.h"
#include "eif_product_internal.h"

EIF_Result EIF_Setup(void)
{
    static EIF_ProductProfile profile;

    profile.product_name = "MiniPrinter";
    profile.requests = g_mp_requests;
    profile.request_count = g_mp_request_count;
    profile.infos = g_mp_infos;
    profile.info_count = g_mp_info_count;
    profile.settings = g_mp_settings;
    profile.setting_count = g_mp_setting_count;
    profile.subscriptions = g_mp_subscriptions;
    profile.subscription_count = g_mp_subscription_count;
    return EIF_Initialize(&profile, &g_eif_default_ports);
}
