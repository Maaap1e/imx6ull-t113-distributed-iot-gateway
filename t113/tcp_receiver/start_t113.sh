#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$APP_DIR/t113_display_app"
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/t113.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$APP_DIR/../../config/t113.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"

LOG="${T113_LOG:-/tmp/t113_tcp.log}"
PID="${T113_PID:-/tmp/t113_display_app.pid}"
STATE="${T113_STATE:-/tmp/t113_sensor_state.json}"
CSV="${T113_CSV:-/tmp/t113_sensor_data.csv}"
TCP_PORT="${PORT:-${TCP_PORT:-5000}}"
BIND_IP="${BIND_IP:-0.0.0.0}"

mkdir -p "$(dirname "$LOG")" "$(dirname "$PID")" "$(dirname "$STATE")" "$(dirname "$CSV")"

if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
    echo "t113_display_app is already running, pid=$(cat "$PID")"
    exit 0
fi

rm -f "$STATE"
chmod 755 "$APP"
"$APP" -a "$BIND_IP" -p "$TCP_PORT" -d -l "$LOG" -P "$PID" -S "$STATE" -C "$CSV"

sleep 1
cat "$LOG"
if [ -f "$STATE" ]; then
    echo "initial state:"
    cat "$STATE"
fi
echo "log: $LOG"
echo "pid: $PID"
echo "latest json: $STATE"
echo "csv data: $CSV"
echo "watch log: tail -f $LOG"
echo "watch csv: tail -f $CSV"
