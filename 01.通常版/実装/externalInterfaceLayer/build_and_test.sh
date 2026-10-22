#!/bin/sh
# EIF（通常版）のビルド確認と単体試験。CMakeが無い環境でも gcc だけで確認できるようにする。
#
# 外部ヘッダーの場所は環境変数で指定できる（既定は本リポジトリの 参考資料/ 配下）。
#   PCL_INC         PCL公開ヘッダー（product_control_api.h 等）
#   RIM_COMMON_INC  REF製品非依存ヘッダー（rim_api_common.h 等）
#   RIM_MODEL_INC   REF製品別ヘッダー（rim_api.h：MiniPrinter）
#   APISV_INC       APIサーバーのヘッダー（server_api.h 等）
set -eu

ROOT=$(cd "$(dirname "$0")" && pwd)
REF_ROOT=${EIF_REF_ROOT:-$ROOT/../../../参考資料}
PCL_INC=${PCL_INC:-$REF_ROOT/PAD_Firm_Module-develop/src/productControlLayer/RequestControl/include}
RIM_COMMON_INC=${RIM_COMMON_INC:-$REF_ROOT/PAD_Firm_RIM-develop/include/common}
RIM_MODEL_INC=${RIM_MODEL_INC:-$REF_ROOT/PAD_Firm_RIM-develop/include/MiniPrinter}
APISV_INC=${APISV_INC:-$REF_ROOT/firmware-server-develop/include}
CC=${CC:-gcc}

# 自分のコードは警告をエラーにする。外部ヘッダーは -isystem で警告対象から外す。
CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L -DEIF_LOG_SILENT"
OUT=$ROOT/build
mkdir -p "$OUT"
cd "$ROOT"

FRAME_SRCS=$(find frame -name '*.c' ! -path 'frame/port/*' | sort)

echo "== 1. Frame: REF製品別ヘッダー・APIサーバーのヘッダー無しでビルドできること（依存規則）"
$CC $CFLAGS -Iinclude -Iframe/include -Iframe -isystem "$PCL_INC" -isystem "$RIM_COMMON_INC" \
    -c $FRAME_SRCS frame/port/eif_port_default.c
rm -f ./*.o
echo "   OK"

echo "== 2. Frameの単体試験"
$CC $CFLAGS -Iinclude -Iframe/include -Iframe -Itest -Itest/frame -isystem "$PCL_INC" -isystem "$RIM_COMMON_INC" \
    $FRAME_SRCS test/frame/*.c -o "$OUT/test_frame" -lpthread
"$OUT/test_frame"

echo "== 3. 製品CFG（MiniPrinter）の単体試験"
$CC $CFLAGS -Iinclude -Iframe/include -Itest -Iproduct/MiniPrinter \
    -isystem "$PCL_INC" -isystem "$RIM_COMMON_INC" -isystem "$RIM_MODEL_INC" \
    product/MiniPrinter/eif_product_requests.c product/MiniPrinter/eif_product_network.c \
    product/MiniPrinter/eif_product_fault.c test/product/test_miniprinter.c -o "$OUT/test_miniprinter"
"$OUT/test_miniprinter"

echo "== 4. コンパイル確認（リンクしない）：製品CFGのSetup、Skeleton、APIサーバー用Connector"
$CC $CFLAGS -Iinclude -Iframe/include -Iproduct/MiniPrinter \
    -isystem "$PCL_INC" -isystem "$RIM_COMMON_INC" -isystem "$RIM_MODEL_INC" \
    -c product/MiniPrinter/eif_product_setup.c product/Skeleton/eif_product_setup.c
$CC $CFLAGS -Iinclude -Iconnectors/api_server/include -isystem "$APISV_INC" -isystem "$APISV_INC/rest" \
    -c connectors/api_server/api_server_connector.c
rm -f ./*.o
echo "   OK"
echo "== すべて成功"
