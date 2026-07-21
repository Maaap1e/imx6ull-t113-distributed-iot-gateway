#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-all}"
IMX_CC="${IMX_CC:-arm-linux-gnueabihf-gcc}"
T113_CC="${T113_CC:-arm-openwrt-linux-gcc}"

case "$ROLE" in
    imx6ull)
        command -v "$IMX_CC" >/dev/null 2>&1 || {
            echo "i.MX6ULL compiler not found: $IMX_CC" >&2
            exit 1
        }
        make -C "$BASE_DIR/linux/can_sensor_client" CC="$IMX_CC"
        make -C "$BASE_DIR/linux/can_ota_host" CC="$IMX_CC"
        make -C "$BASE_DIR/linux/imx6ull_gateway" CC="$IMX_CC"
        ;;
    t113)
        command -v "$T113_CC" >/dev/null 2>&1 || {
            echo "T113 compiler not found: $T113_CC" >&2
            exit 1
        }
        make -C "$BASE_DIR/t113/tcp_receiver" CC="$T113_CC"
        ;;
    all)
        IMX_CC="$IMX_CC" T113_CC="$T113_CC" sh "$0" imx6ull
        IMX_CC="$IMX_CC" T113_CC="$T113_CC" sh "$0" t113
        ;;
    *)
        echo "Usage: $0 [imx6ull|t113|all]" >&2
        exit 2
        ;;
esac

if [ "${RUN_TESTS:-0}" = "1" ]; then
    make -C "$BASE_DIR/tests" test CC="${HOST_CC:-gcc}"
fi
