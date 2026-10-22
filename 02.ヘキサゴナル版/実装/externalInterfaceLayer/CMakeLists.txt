# externalInterfaceLayer（ヘキサゴナル版：コア／ポート／アダプタ／組立て）
#
# 外部ヘッダーの場所（本番リポジトリでは REPO_ROOT から設定する）
#   PCL_INCLUDE_DIR / RIM_COMMON_INCLUDE_DIR / RIM_MODEL_INCLUDE_DIR / API_SERVER_INCLUDE_DIR
#
# 依存規則（設計書_ヘキサゴナル版 12.2）：
#   eif_core には外部のinclude pathを1つも渡さない。依存は必ず「アダプタ → コア」の向き。
cmake_minimum_required(VERSION 3.20)
project(external_interface_layer_hex C)

set(MODEL "" CACHE STRING "機種（composition/<MODEL>、adapters/driven/*/models/<MODEL>）。未指定なら Skeleton")
set(EIF_MODEL ${MODEL})
if(NOT EIF_MODEL OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/composition/${EIF_MODEL})
    set(EIF_MODEL Skeleton)
endif()
message(STATUS "EIF(hex): model = ${EIF_MODEL}")

find_package(Threads REQUIRED)

# ---------------------------------------------------------------- コア（外を知らない）
file(GLOB_RECURSE EIF_CORE_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/core/*.c)
add_library(eif_core STATIC ${EIF_CORE_SRCS})
target_include_directories(eif_core
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include          # 公開ポート
            ${CMAKE_CURRENT_SOURCE_DIR}/core/include     # 出口ポート・組立て用の入口
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core)
target_link_libraries(eif_core PUBLIC Threads::Threads)
target_compile_features(eif_core PUBLIC c_std_11)

# ---------------------------------------------------------------- 使われる側のアダプタ
set(EIF_DRIVEN_SRCS)
if(NOT EIF_MODEL STREQUAL "Skeleton")
    file(GLOB EIF_DRIVEN_SRCS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driven/pcl/pcl_adapter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driven/pcl/models/${EIF_MODEL}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driven/ref/ref_adapter.c
        ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driven/ref/models/${EIF_MODEL}/*.c)
endif()
add_library(eif_driven_adapters STATIC ${EIF_DRIVEN_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/composition/${EIF_MODEL}/eif_compose.c)   # 組立て
target_include_directories(eif_driven_adapters PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driven/pcl
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driven/ref
    ${PCL_INCLUDE_DIR} ${RIM_COMMON_INCLUDE_DIR} ${RIM_MODEL_INCLUDE_DIR})
target_link_libraries(eif_driven_adapters PUBLIC eif_core)

# ---------------------------------------------------------------- 使う側のアダプタ
add_library(eif_adapter_api_server STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driving/api_server/api_server_connector.c)
target_include_directories(eif_adapter_api_server
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/adapters/driving/api_server/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${API_SERVER_INCLUDE_DIR})
target_link_libraries(eif_adapter_api_server PRIVATE Threads::Threads)

add_library(external_interface_layer INTERFACE)
target_link_libraries(external_interface_layer INTERFACE eif_driven_adapters eif_core eif_adapter_api_server)

# ---------------------------------------------------------------- 試験
option(EIF_BUILD_TESTS "コアの単体試験と通し試験をビルドする" OFF)
if(EIF_BUILD_TESTS)
    add_executable(eif_test_core ${EIF_CORE_SRCS} test/fakes/fake_ports.c test/core/test_core.c)
    target_include_directories(eif_test_core PRIVATE include core core/include test test/fakes)
    target_compile_definitions(eif_test_core PRIVATE EIF_LOG_SILENT)
    target_link_libraries(eif_test_core PRIVATE Threads::Threads)
endif()
