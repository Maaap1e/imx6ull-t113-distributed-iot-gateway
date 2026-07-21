#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$APP_DIR/../../config/imx6ull.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"
PID="${PID:-${STM32_CAN_PID:-/tmp/stm32_can_client.pid}}"

if [ -f "$PID" ]; then
    pid=$(cat "$PID")
    case "$pid" in
        ''|*[!0-9]*) ;;
        *) kill "$pid" 2>/dev/null || true ;;
    esac
fi
rm -f "$PID"
echo "stm32_can_sensor_client stopped"
