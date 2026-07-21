#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"

case "$ROLE" in imx6ull|t113) ;; *) echo "Usage: $0 imx6ull|t113" >&2; exit 2 ;; esac
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/$ROLE.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/$ROLE.conf"
. "$CONFIG_FILE"

if [ -f "$SUPERVISOR_PID" ]; then
    pid=$(cat "$SUPERVISOR_PID" 2>/dev/null || true)
    case "$pid" in ''|*[!0-9]*) ;; *) kill "$pid" 2>/dev/null || true ;; esac
    rm -f "$SUPERVISOR_PID"
fi

if [ "$ROLE" = "imx6ull" ]; then
    IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/linux/imx6ull_gateway/stop_gateway.sh"
    IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/linux/can_sensor_client/stop_client.sh"
else
    IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/t113/tcp_receiver/stop_t113.sh"
fi
echo "$ROLE stack stopped"
