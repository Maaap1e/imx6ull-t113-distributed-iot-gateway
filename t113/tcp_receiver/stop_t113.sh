#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/t113.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$APP_DIR/../../config/t113.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"
PID="${T113_PID:-/tmp/t113_display_app.pid}"

if [ -f "$PID" ]; then
    pid=$(cat "$PID")
    case "$pid" in
        ''|*[!0-9]*) ;;
        *) kill "$pid" 2>/dev/null || true ;;
    esac
fi
rm -f "$PID"
echo "t113_display_app stopped"
