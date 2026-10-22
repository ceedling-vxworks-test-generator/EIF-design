#!/bin/sh
# EIFのビルド確認と単体試験。CMakeが無い環境でも gcc だけで確認できるようにする。
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

FRAME_SRCS=$(find frame -name '*.c' | sort)
FRAME_INC="-Iinclude -Iframe/include -Iframe"

echo "== 1. Frame：PCL・REF・APIサーバーのヘッダーを1つも渡さずにビルドできること（依存規則）"
$CC $CFLAGS $FRAME_INC -c $FRAME_SRCS
rm -f ./*.o
echo "   OK"

echo "== 2. Frameの単体試験（スタブAccessor）"
$CC $CFLAGS $FRAME_INC -Itest -Itest/stubs $FRAME_SRCS test/stubs/stub_accessors.c test/frame/test_frame.c \
    -o "$OUT/test_frame" -lpthread
"$OUT/test_frame"

echo "== 3. Frame＋MiniPrinterのセットアップ（PCL・REF Accessor）の結合試験（PCL・RIMの関数はスタブ）"
$CC $CFLAGS $FRAME_INC -Itest -Iaccessors/pcl -Iaccessors/ref \
    -isystem "$PCL_INC" -isystem "$RIM_COMMON_INC" -isystem "$RIM_MODEL_INC" \
    $FRAME_SRCS accessors/pcl/pcl_accessor.c accessors/pcl/models/MiniPrinter/pcl_request_map.c \
    accessors/ref/ref_accessor.c accessors/ref/models/MiniPrinter/ref_maps.c \
    setup/MiniPrinter/eif_setup.c test/integration/test_integration_miniprinter.c -o "$OUT/test_integration" -lpthread
"$OUT/test_integration"

echo "== 4. コンパイル確認（リンクしない）：Skeletonのセットアップ、APIサーバー用Connector"
$CC $CFLAGS $FRAME_INC -c setup/Skeleton/eif_setup.c
$CC $CFLAGS -Iinclude -Iconnectors/api_server/include -isystem "$APISV_INC" -isystem "$APISV_INC/rest" \
    -c connectors/api_server/api_server_connector.c
rm -f ./*.o
echo "   OK"
echo "== すべて成功"
