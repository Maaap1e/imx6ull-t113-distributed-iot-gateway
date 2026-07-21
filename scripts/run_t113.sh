#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/t113.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/t113.conf"
. "$CONFIG_FILE"

mkdir -p "$(dirname "$T113_PID")" "$(dirname "$T113_STATE")" "$(dirname "$T113_CSV")"
exec "$BASE_DIR/t113/tcp_receiver/t113_display_app" \
    -a "$BIND_IP" -p "$TCP_PORT" -P "$T113_PID" \
    -S "$T113_STATE" -C "$T113_CSV"
