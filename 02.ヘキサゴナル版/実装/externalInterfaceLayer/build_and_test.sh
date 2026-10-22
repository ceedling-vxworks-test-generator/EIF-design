#!/bin/sh
# EIF（ヘキサゴナル版）のビルド確認と単体試験。CMakeが無い環境でも gcc だけで確認できるようにする。
#
# 外部ヘッダーの場所は環境変数で指定できる（既定は本リポジトリの 参考資料/ 配下）。
#   PCL_INC / RIM_COMMON_INC / RIM_MODEL_INC / APISV_INC
set -eu

ROOT=$(cd "$(dirname "$0")" && pwd)
REF_ROOT=${EIF_REF_ROOT:-$ROOT/../../../参考資料}
PCL_INC=${PCL_INC:-$REF_ROOT/PAD_Firm_Module-develop/src/productControlLayer/RequestControl/include}
RIM_COMMON_INC=${RIM_COMMON_INC:-$REF_ROOT/PAD_Firm_RIM-develop/include/common}
RIM_MODEL_INC=${RIM_MODEL_INC:-$REF_ROOT/PAD_Firm_RIM-develop/include/MiniPrinter}
APISV_INC=${APISV_INC:-$REF_ROOT/firmware-server-develop/include}
CC=${CC:-gcc}

CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L -DEIF_LOG_SILENT"
OUT=$ROOT/build
mkdir -p "$OUT"
cd "$ROOT"

CORE_SRCS=$(find core -name '*.c' | sort)
CORE_INC="-Iinclude -Icore/include -Icore"

echo "== 1. コア：PCL・REF・APIサーバーのヘッダーを1つも渡さずにビルドできること（依存規則）"
$CC $CFLAGS $CORE_INC -c $CORE_SRCS
rm -f ./*.o
echo "   OK"

echo "== 2. コアの単体試験（偽物アダプタ）"
$CC $CFLAGS $CORE_INC -Itest -Itest/fakes $CORE_SRCS test/fakes/fake_ports.c test/core/test_core.c \
    -o "$OUT/test_core" -lpthread
"$OUT/test_core"

echo "== 3. コア＋MiniPrinterの組立て（PCL・REFアダプタ）の通し試験（PCL・RIMの関数は偽物）"
$CC $CFLAGS $CORE_INC -Itest -Iadapters/driven/pcl -Iadapters/driven/ref \
    -isystem "$PCL_INC" -isystem "$RIM_COMMON_INC" -isystem "$RIM_MODEL_INC" \
    $CORE_SRCS adapters/driven/pcl/pcl_adapter.c adapters/driven/pcl/models/MiniPrinter/pcl_request_map.c \
    adapters/driven/ref/ref_adapter.c adapters/driven/ref/models/MiniPrinter/ref_maps.c \
    composition/MiniPrinter/eif_compose.c test/adapters/test_e2e_miniprinter.c -o "$OUT/test_e2e" -lpthread
"$OUT/test_e2e"

echo "== 4. コンパイル確認（リンクしない）：Skeletonの組立て、APIサーバー用アダプタ"
$CC $CFLAGS $CORE_INC -c composition/Skeleton/eif_compose.c
$CC $CFLAGS -Iinclude -Iadapters/driving/api_server/include -isystem "$APISV_INC" -isystem "$APISV_INC/rest" \
    -c adapters/driving/api_server/api_server_connector.c
rm -f ./*.o
echo "   OK"
echo "== すべて成功"
