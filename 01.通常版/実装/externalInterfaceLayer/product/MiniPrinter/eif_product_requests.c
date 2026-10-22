/**
 * @file eif_product_requests.c
 * @brief MiniPrinterの要求の表（公開要求ID → PCL製品要求ID）。表に無い要求は公開しない。
 *
 * 対応は暫定。正式な対応はPCL担当と確定する。
 */
#include "eif_product_internal.h"

const EIF_RequestMapEntry g_mp_requests[] = {
    /* 公開要求ID                 PCL製品要求ID */
    { EIF_REQ_PRINT,             PRODUCT_REQUEST_ID_PRINT },
    { EIF_REQ_HEAD_CLEANING,     PRODUCT_REQUEST_ID_NORMAL_PURGE_ALL },
    { EIF_REQ_WIPER_MAINTENANCE, PRODUCT_REQUEST_ID_WIPE_ALL },
};

const size_t g_mp_request_count = sizeof(g_mp_requests) / sizeof(g_mp_requests[0]);
