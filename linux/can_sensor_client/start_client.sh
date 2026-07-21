#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$APP_DIR/stm32_can_sensor_client"
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$APP_DIR/../../config/imx6ull.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"

IFACE="${IFACE:-${CAN_IFACE:-can0}}"
BITRATE="${BITRATE:-${CAN_BITRATE:-500000}}"
STATE="${STATE:-${STM32_CAN_STATE:-/tmp/stm32_can_state.json}}"
CSV="${CSV:-${STM32_CAN_CSV:-/tmp/stm32_can_data.csv}}"
LOG="${LOG:-${STM32_CAN_LOG:-/tmp/stm32_can_client.log}}"
PID="${PID:-${STM32_CAN_PID:-/tmp/stm32_can_client.pid}}"

mkdir -p "$(dirname "$STATE")" "$(dirname "$CSV")" "$(dirname "$LOG")" "$(dirname "$PID")"

if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "stm32_can_sensor_client is already running, pid=$(cat "$PID")"
    exit 0
fi

"$APP_DIR/setup_can.sh" "$IFACE" "$BITRATE"
rm -f "$STATE"

if command -v nohup >/dev/null 2>&1; then
    nohup "$APP" -i "$IFACE" -s "$STATE" -c "$CSV" >> "$LOG" 2>&1 &
else
    "$APP" -i "$IFACE" -s "$STATE" -c "$CSV" >> "$LOG" 2>&1 &
fi
echo "$!" > "$PID"

sleep 1
cat "$LOG"
echo "log: $LOG"
echo "pid: $PID"
echo "latest json: $STATE"
echo "csv data: $CSV"
