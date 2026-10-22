/**
 * @file pcl_request_map.c
 * @brief MiniPrinterの対応表：公開要求ID → PCL製品要求ID。表に無い要求は公開しない。
 *
 * 対応は暫定。正式な対応はPCL担当と確定する。
 */
#include "pcl_accessor.h"

const EIF_PclRequestMap g_pcl_request_map[] = {
    { EIF_REQ_PRINT,             PRODUCT_REQUEST_ID_PRINT },
    { EIF_REQ_HEAD_CLEANING,     PRODUCT_REQUEST_ID_NORMAL_PURGE_ALL },
    { EIF_REQ_WIPER_MAINTENANCE, PRODUCT_REQUEST_ID_WIPE_ALL },
};

const size_t g_pcl_request_map_count = sizeof(g_pcl_request_map) / sizeof(g_pcl_request_map[0]);
