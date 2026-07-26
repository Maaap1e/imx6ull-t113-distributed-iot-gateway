#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$APP_DIR/../../config/imx6ull.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"
MQTT_PID="${MQTT_PID:-/tmp/mqtt_bridge.pid}"

if [ -f "$MQTT_PID" ]; then
    pid=$(cat "$MQTT_PID" 2>/dev/null || true)
    case "$pid" in
        ''|*[!0-9]*) ;;
        *) kill "$pid" 2>/dev/null || true ;;
    esac
fi
rm -f "$MQTT_PID"
echo "mqtt_bridge stopped"
