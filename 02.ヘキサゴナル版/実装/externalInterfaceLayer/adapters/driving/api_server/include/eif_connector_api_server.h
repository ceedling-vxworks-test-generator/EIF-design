/**
 * @file eif_connector_api_server.h
 * @brief APIサーバー用Connectorの入口。起動処理が EIF_Setup() の後、server_start() の前に1回呼ぶ。
 */
#ifndef EIF_CONNECTOR_API_SERVER_H
#define EIF_CONNECTOR_API_SERVER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief EIFへ自身を登録し、APIサーバーへRESTのcallbackを登録する。
 * @retval true  成功
 * @retval false EIFへの登録、またはAPIサーバーへの登録に失敗
 */
bool EifApiServerConnector_Setup(void);

#ifdef __cplusplus
}
#endif

#endif /* EIF_CONNECTOR_API_SERVER_H */
