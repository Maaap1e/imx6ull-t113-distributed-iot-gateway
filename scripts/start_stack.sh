#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"

case "$ROLE" in imx6ull|t113) ;; *) echo "Usage: $0 imx6ull|t113" >&2; exit 2 ;; esac
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/$ROLE.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/$ROLE.conf"
. "$CONFIG_FILE"

if [ -f "$SUPERVISOR_PID" ] && kill -0 "$(cat "$SUPERVISOR_PID")" 2>/dev/null; then
    echo "$ROLE supervisor already running, pid=$(cat "$SUPERVISOR_PID")"
    exit 0
fi

mkdir -p "$(dirname "$SUPERVISOR_PID")"
mkdir -p "$(dirname "$SUPERVISOR_LOG")"
if command -v nohup >/dev/null 2>&1; then
    nohup env IOT_GATEWAY_CONFIG="$CONFIG_FILE" \
        "$SCRIPT_DIR/runtime_supervisor.sh" "$ROLE" >> "$SUPERVISOR_LOG" 2>&1 &
else
    env IOT_GATEWAY_CONFIG="$CONFIG_FILE" \
        "$SCRIPT_DIR/runtime_supervisor.sh" "$ROLE" >> "$SUPERVISOR_LOG" 2>&1 &
fi
echo "$!" > "$SUPERVISOR_PID"
echo "$ROLE stack started, supervisor pid=$(cat "$SUPERVISOR_PID")"
echo "supervisor log: $SUPERVISOR_LOG"
