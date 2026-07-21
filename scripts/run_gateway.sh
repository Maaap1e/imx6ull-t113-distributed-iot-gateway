#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/imx6ull.conf"
. "$CONFIG_FILE"

mkdir -p "$(dirname "$GATEWAY_PID")"
echo "$$" > "$GATEWAY_PID"
exec "$BASE_DIR/linux/imx6ull_gateway/imx6ull_gateway_app" \
    -a "$T113_IP" -p "$TCP_PORT" -i "$SAMPLE_INTERVAL_MS" \
    -A "$AP3216C_SYSFS_DIR" -D "$AP3216C_DEVICE" \
    -I "$ICM20608_DEVICE" -M "$STM32_CAN_STATE"
