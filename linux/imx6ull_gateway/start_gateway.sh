#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$APP_DIR/imx6ull_gateway_app"
LOG="/tmp/imx6ull_gateway.log"
PID="/tmp/imx6ull_gateway_app.pid"

T113_IP="${T113_IP:-192.168.10.20}"
PORT="${PORT:-5000}"
INTERVAL_MS="${INTERVAL_MS:-500}"
STM32_CAN_STATE="${STM32_CAN_STATE:-/tmp/stm32_can_state.json}"

if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "imx6ull_gateway_app is already running, pid=$(cat "$PID")"
    exit 0
fi

rm -f "$LOG"
chmod 755 "$APP"
if command -v nohup >/dev/null 2>&1; then
    nohup "$APP" -a "$T113_IP" -p "$PORT" -i "$INTERVAL_MS" -M "$STM32_CAN_STATE" >> "$LOG" 2>&1 &
else
    "$APP" -a "$T113_IP" -p "$PORT" -i "$INTERVAL_MS" -M "$STM32_CAN_STATE" >> "$LOG" 2>&1 &
fi
echo "$!" > "$PID"

sleep 1
cat "$LOG"
echo "log: $LOG"
echo "pid: $PID"
echo "T113_IP: $T113_IP"
echo "STM32_CAN_STATE: $STM32_CAN_STATE"
echo "watch log: tail -f $LOG"
