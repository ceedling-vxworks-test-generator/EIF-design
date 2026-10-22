# externalInterfaceLayer（通常版：Frame／Connector／製品CFG）
#
# 外部ヘッダーの場所（本番リポジトリでは REPO_ROOT から設定する）
#   PCL_INCLUDE_DIR         src/productControlLayer/RequestControl/include
#   RIM_COMMON_INCLUDE_DIR  src/referenceServiceLayer/rim/include/common
#   RIM_MODEL_INCLUDE_DIR   src/referenceServiceLayer/rim/include/<MODEL>
#   API_SERVER_INCLUDE_DIR  libs/api-server/include
#
# 依存規則（詳細設計書 10.4）は include path で強制する：
#   eif_frame には REF製品別ヘッダーと APIサーバーのヘッダーを渡さない。
cmake_minimum_required(VERSION 3.20)
project(external_interface_layer C)

set(MODEL "" CACHE STRING "製品CFG（product/<MODEL>）。未指定なら Skeleton")
set(EIF_MODEL ${MODEL})
if(NOT EIF_MODEL OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/product/${EIF_MODEL})
    set(EIF_MODEL Skeleton)
endif()
message(STATUS "EIF: product CFG = ${EIF_MODEL}")

find_package(Threads REQUIRED)

# ---------------------------------------------------------------- Frame（機種非依存）
file(GLOB_RECURSE EIF_FRAME_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/frame/*.c)
add_library(eif_frame STATIC ${EIF_FRAME_SRCS})
target_include_directories(eif_frame
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${PCL_INCLUDE_DIR}
            ${RIM_COMMON_INCLUDE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/frame
            ${CMAKE_CURRENT_SOURCE_DIR}/frame/include)
target_link_libraries(eif_frame PUBLIC Threads::Threads)
target_compile_features(eif_frame PUBLIC c_std_11)

# 製品CFGだけが使う入口（EIF_Initialize、Port）
add_library(eif_frame_cfg_api INTERFACE)
target_include_directories(eif_frame_cfg_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/frame/include)

# ---------------------------------------------------------------- 製品CFG（機種依存）
file(GLOB EIF_PRODUCT_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/product/${EIF_MODEL}/*.c)
add_library(eif_product STATIC ${EIF_PRODUCT_SRCS})
target_include_directories(eif_product PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/product/${EIF_MODEL}
    ${RIM_MODEL_INCLUDE_DIR})
target_link_libraries(eif_product PUBLIC eif_frame eif_frame_cfg_api)

# ---------------------------------------------------------------- Connector（接続先依存）
add_library(eif_connector_api_server STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/connectors/api_server/api_server_connector.c)
target_include_directories(eif_connector_api_server
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/connectors/api_server/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${API_SERVER_INCLUDE_DIR})
target_link_libraries(eif_connector_api_server PRIVATE Threads::Threads)

# ---------------------------------------------------------------- 一括リンク用
add_library(external_interface_layer INTERFACE)
target_link_libraries(external_interface_layer INTERFACE eif_product eif_frame eif_connector_api_server)

# ---------------------------------------------------------------- 試験
option(EIF_BUILD_TESTS "Frame・製品CFGの単体試験をビルドする" OFF)
if(EIF_BUILD_TESTS)
    file(GLOB_RECURSE EIF_FRAME_SRCS_NO_PORT CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/frame/*.c)
    list(FILTER EIF_FRAME_SRCS_NO_PORT EXCLUDE REGEX ".*/frame/port/.*")
    add_executable(eif_test_frame ${EIF_FRAME_SRCS_NO_PORT}
        test/frame/stub_ports.c test/frame/test_frame.c)
    target_include_directories(eif_test_frame PRIVATE include frame frame/include test test/frame
        ${PCL_INCLUDE_DIR} ${RIM_COMMON_INCLUDE_DIR})
    target_compile_definitions(eif_test_frame PRIVATE EIF_LOG_SILENT)
    target_link_libraries(eif_test_frame PRIVATE Threads::Threads)

    if(EIF_MODEL STREQUAL "MiniPrinter")
        add_executable(eif_test_miniprinter
            product/MiniPrinter/eif_product_requests.c
            product/MiniPrinter/eif_product_network.c
            product/MiniPrinter/eif_product_fault.c
            test/product/test_miniprinter.c)
        target_include_directories(eif_test_miniprinter PRIVATE include frame/include test product/MiniPrinter
            ${PCL_INCLUDE_DIR} ${RIM_COMMON_INCLUDE_DIR} ${RIM_MODEL_INCLUDE_DIR})
    endif()
endif()
