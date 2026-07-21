#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$APP_DIR/imx6ull_gateway_app"
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$APP_DIR/../../config/imx6ull.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"

T113_IP="${T113_IP:-192.168.3.32}"
TCP_PORT="${PORT:-${TCP_PORT:-5000}}"
SAMPLE_INTERVAL_MS="${INTERVAL_MS:-${SAMPLE_INTERVAL_MS:-500}}"
STM32_CAN_STATE="${STM32_CAN_STATE:-/tmp/stm32_can_state.json}"
AP3216C_SYSFS_DIR="${AP3216C_SYSFS_DIR:-/sys/class/misc/ap3216c}"
AP3216C_DEVICE="${AP3216C_DEVICE:-/dev/ap3216c}"
ICM20608_DEVICE="${ICM20608_DEVICE:-/dev/icm20608}"
LOG="${GATEWAY_LOG:-/tmp/imx6ull_gateway.log}"
PID="${GATEWAY_PID:-/tmp/imx6ull_gateway_app.pid}"

mkdir -p "$(dirname "$LOG")" "$(dirname "$PID")"

if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "imx6ull_gateway_app is already running, pid=$(cat "$PID")"
    exit 0
fi

chmod 755 "$APP"
if command -v nohup >/dev/null 2>&1; then
    nohup "$APP" -a "$T113_IP" -p "$TCP_PORT" -i "$SAMPLE_INTERVAL_MS" \
        -A "$AP3216C_SYSFS_DIR" -D "$AP3216C_DEVICE" -I "$ICM20608_DEVICE" \
        -M "$STM32_CAN_STATE" >> "$LOG" 2>&1 &
else
    "$APP" -a "$T113_IP" -p "$TCP_PORT" -i "$SAMPLE_INTERVAL_MS" \
        -A "$AP3216C_SYSFS_DIR" -D "$AP3216C_DEVICE" -I "$ICM20608_DEVICE" \
        -M "$STM32_CAN_STATE" >> "$LOG" 2>&1 &
fi
echo "$!" > "$PID"

sleep 1
cat "$LOG"
echo "log: $LOG"
echo "pid: $PID"
echo "T113_IP: $T113_IP"
echo "TCP_PORT: $TCP_PORT"
echo "STM32_CAN_STATE: $STM32_CAN_STATE"
echo "watch log: tail -f $LOG"
