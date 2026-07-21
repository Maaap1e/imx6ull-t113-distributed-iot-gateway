#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/imx6ull.conf"
. "$CONFIG_FILE"

mkdir -p "$(dirname "$STM32_CAN_STATE")" "$(dirname "$STM32_CAN_CSV")" \
    "$(dirname "$STM32_CAN_PID")"
"$BASE_DIR/linux/can_sensor_client/setup_can.sh" "$CAN_IFACE" "$CAN_BITRATE"
echo "$$" > "$STM32_CAN_PID"
exec "$BASE_DIR/linux/can_sensor_client/stm32_can_sensor_client" \
    -i "$CAN_IFACE" -s "$STM32_CAN_STATE" -c "$STM32_CAN_CSV"
