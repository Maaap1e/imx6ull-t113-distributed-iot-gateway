#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$APP_DIR/stm32_can_sensor_client"
IFACE="${IFACE:-can0}"
BITRATE="${BITRATE:-500000}"
STATE="${STATE:-/tmp/stm32_can_state.json}"
CSV="${CSV:-/tmp/stm32_can_data.csv}"
LOG="${LOG:-/tmp/stm32_can_client.log}"
PID="${PID:-/tmp/stm32_can_client.pid}"

if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "stm32_can_sensor_client is already running, pid=$(cat "$PID")"
    exit 0
fi

"$APP_DIR/setup_can.sh" "$IFACE" "$BITRATE"
rm -f "$LOG" "$STATE" "$CSV"

nohup "$APP" -i "$IFACE" -s "$STATE" -c "$CSV" >> "$LOG" 2>&1 &
echo "$!" > "$PID"

sleep 1
cat "$LOG"
echo "log: $LOG"
echo "pid: $PID"
echo "latest json: $STATE"
echo "csv data: $CSV"
