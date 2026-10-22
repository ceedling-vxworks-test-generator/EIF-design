/**
 * @file eif_port.h
 * @brief FrameがPCL・REFを呼ぶための口（Port）。内部の接続先を差し替えるときと、試験でスタブにするときに使う。
 *
 * Frameが参照してよいのはPCL公開ヘッダーとREFの製品非依存ヘッダーだけである。
 */
#ifndef EIF_PORT_H
#define EIF_PORT_H

#include <stddef.h>
#include <stdint.h>

#include "product_control_api.h" /* PCL公開ヘッダー */
#include "rim_api_common.h"      /* REF製品非依存ヘッダー */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief PCLの呼出し口。 */
typedef struct {
    external_request_result_t (*submit)(product_request_id_t id, external_request_completion_cb_t cb);
    uc_result_t (*cancel)(uint64_t generation_id);
    uc_result_t (*get_result)(uint64_t generation_id, external_request_result_t *out);
} EIF_PclPort;

/** @brief 情報源（REF）の呼出し口。 */
typedef struct {
    RIStatus (*get_binary)(uint32_t data_id, RI_BINARY *out);
    RIStatus (*subscribe)(const RI_SUBSCRIPTION_REQUEST *req, uint64_t *out_id);
} EIF_InfoSourcePort;

/** @brief 設定の書込み先（REF）の呼出し口。 */
typedef struct {
    RIStatus (*set_binary)(uint32_t data_id, const void *data, size_t size);
} EIF_SettingSinkPort;

/** @brief Port一式。 */
typedef struct {
    const EIF_PclPort *pcl;
    const EIF_InfoSourcePort *info;
    const EIF_SettingSinkPort *setting;
} EIF_Ports;

/** @brief PCL・REFの実APIを指す既定のPort（frame/port/eif_port_default.c）。 */
extern const EIF_Ports g_eif_default_ports;

#ifdef __cplusplus
}
#endif

#endif /* EIF_PORT_H */
