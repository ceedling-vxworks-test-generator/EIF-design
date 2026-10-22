# externalInterfaceLayer（Frame／Connector／Accessor／機種セットアップ）
#
# 外部ヘッダーの場所（本番リポジトリでは REPO_ROOT から設定する）
#   PCL_INCLUDE_DIR / RIM_COMMON_INCLUDE_DIR / RIM_MODEL_INCLUDE_DIR / API_SERVER_INCLUDE_DIR
#
# 依存規則（詳細設計書 6.4）：
#   eif_frame には外部のinclude pathを1つも渡さない。依存は必ず「Connector／Accessor → Frame」の向き。
cmake_minimum_required(VERSION 3.20)
project(external_interface_layer C)

set(MODEL "" CACHE STRING "機種（setup/<MODEL>、accessors/*/models/<MODEL>）。未指定なら Skeleton")
set(EIF_MODEL ${MODEL})
if(NOT EIF_MODEL OR NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/setup/${EIF_MODEL})
    set(EIF_MODEL Skeleton)
endif()
message(STATUS "EIF: model = ${EIF_MODEL}")

find_package(Threads REQUIRED)

# ---------------------------------------------------------------- Frame（PCL・REF・外部コンポーネント・機種を知らない）
file(GLOB_RECURSE EIF_FRAME_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/frame/*.c)
add_library(eif_frame STATIC ${EIF_FRAME_SRCS})
target_include_directories(eif_frame
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include          # 公開API
            ${CMAKE_CURRENT_SOURCE_DIR}/frame/include     # 下位IF・機種セットアップ向けIF
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/frame)
target_link_libraries(eif_frame PUBLIC Threads::Threads)
target_compile_features(eif_frame PUBLIC c_std_11)

# ---------------------------------------------------------------- Accessor（PCL・REF接続部）
set(EIF_ACCESSOR_SRCS)
if(NOT EIF_MODEL STREQUAL "Skeleton")
    file(GLOB EIF_ACCESSOR_SRCS CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/accessors/pcl/pcl_accessor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/accessors/pcl/models/${EIF_MODEL}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/accessors/ref/ref_accessor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/accessors/ref/models/${EIF_MODEL}/*.c)
endif()
add_library(eif_accessors STATIC ${EIF_ACCESSOR_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/setup/${EIF_MODEL}/eif_setup.c)   # 機種セットアップ
target_include_directories(eif_accessors PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/accessors/pcl
    ${CMAKE_CURRENT_SOURCE_DIR}/accessors/ref
    ${PCL_INCLUDE_DIR} ${RIM_COMMON_INCLUDE_DIR} ${RIM_MODEL_INCLUDE_DIR})
target_link_libraries(eif_accessors PUBLIC eif_frame)

# ---------------------------------------------------------------- Connector（外部コンポーネント接続部）
add_library(eif_connector_api_server STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/connectors/api_server/api_server_connector.c)
target_include_directories(eif_connector_api_server
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/connectors/api_server/include
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${API_SERVER_INCLUDE_DIR})
target_link_libraries(eif_connector_api_server PRIVATE Threads::Threads)

add_library(external_interface_layer INTERFACE)
target_link_libraries(external_interface_layer INTERFACE eif_accessors eif_frame eif_connector_api_server)

# ---------------------------------------------------------------- 試験
option(EIF_BUILD_TESTS "Frameの単体試験と結合試験をビルドする" OFF)
if(EIF_BUILD_TESTS)
    add_executable(eif_test_frame ${EIF_FRAME_SRCS} test/stubs/stub_accessors.c test/frame/test_frame.c)
    target_include_directories(eif_test_frame PRIVATE include frame frame/include test test/stubs)
    target_compile_definitions(eif_test_frame PRIVATE EIF_LOG_SILENT)
    target_link_libraries(eif_test_frame PRIVATE Threads::Threads)
endif()
