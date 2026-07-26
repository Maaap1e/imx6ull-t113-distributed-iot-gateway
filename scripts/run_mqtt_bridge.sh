#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/imx6ull.conf"
. "$CONFIG_FILE"

MQTT_ENABLE="${MQTT_ENABLE:-0}"
MQTT_BROKER_URI="${MQTT_BROKER_URI:-tcp://127.0.0.1:1883}"
MQTT_CLIENT_ID="${MQTT_CLIENT_ID:-imx6ull-gateway-01}"
MQTT_TOPIC_PREFIX="${MQTT_TOPIC_PREFIX:-iot-gateway/imx6ull-01}"
MQTT_STATE_FILE="${MQTT_STATE_FILE:-/tmp/imx6ull_gateway_state.json}"
MQTT_PUBLISH_INTERVAL_SECONDS="${MQTT_PUBLISH_INTERVAL_SECONDS:-5}"
MQTT_LED_DIR="${MQTT_LED_DIR:-/sys/class/leds/sys-led}"
MQTT_USERNAME="${MQTT_USERNAME:-}"
MQTT_PASSWORD="${MQTT_PASSWORD:-}"
MQTT_PID="${MQTT_PID:-/tmp/mqtt_bridge.pid}"

if [ "$MQTT_ENABLE" != "1" ]; then
    echo "mqtt_bridge is disabled (set MQTT_ENABLE=1)"
    exit 0
fi

mkdir -p "$(dirname "$MQTT_PID")"
echo "$$" > "$MQTT_PID"
export MQTT_USERNAME MQTT_PASSWORD
exec env LD_LIBRARY_PATH="/opt/iot-gateway/lib:${LD_LIBRARY_PATH:-}" \
    "$BASE_DIR/linux/mqtt_bridge/mqtt_bridge" \
    -b "$MQTT_BROKER_URI" -c "$MQTT_CLIENT_ID" \
    -t "$MQTT_TOPIC_PREFIX" -f "$MQTT_STATE_FILE" \
    -i "$MQTT_PUBLISH_INTERVAL_SECONDS" -L "$MQTT_LED_DIR"
