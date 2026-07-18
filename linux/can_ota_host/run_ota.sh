#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
APP="$APP_DIR/stm32_can_ota_host"
IFACE="${IFACE:-can0}"
BITRATE="${BITRATE:-500000}"
PACING_US="${PACING_US:-20000}"
TIMEOUT_MS="${TIMEOUT_MS:-8000}"
CLIENT_PID="${CAN_CLIENT_PID:-/tmp/stm32_can_client.pid}"
CLIENT_WAS_RUNNING=0
CLIENT_STOP_SCRIPT=""
CLIENT_START_SCRIPT=""

if [ $# -lt 1 ]; then
    echo "Usage: $0 stm32_app.bin"
    exit 1
fi

FIRMWARE="$1"

for script in \
    "$APP_DIR/stop_client.sh" \
    "$APP_DIR/../imx6ull_stm32_can_client/stop_client.sh"
do
    if [ -f "$script" ]; then
        CLIENT_STOP_SCRIPT="$script"
        break
    fi
done

for script in \
    "$APP_DIR/start_client.sh" \
    "$APP_DIR/../imx6ull_stm32_can_client/start_client.sh"
do
    if [ -f "$script" ]; then
        CLIENT_START_SCRIPT="$script"
        break
    fi
done

restart_can_client()
{
    if [ "$CLIENT_WAS_RUNNING" -eq 1 ] && [ -n "$CLIENT_START_SCRIPT" ]; then
        echo "Restarting STM32 CAN sensor client..."
        if ! sh "$CLIENT_START_SCRIPT"; then
            echo "warning: failed to restart STM32 CAN sensor client" >&2
        fi
    fi
    return 0
}

trap restart_can_client EXIT

if [ -f "$CLIENT_PID" ] && kill -0 "$(cat "$CLIENT_PID")" 2>/dev/null; then
    CLIENT_WAS_RUNNING=1
    if [ -n "$CLIENT_STOP_SCRIPT" ]; then
        echo "Stopping STM32 CAN sensor client before OTA..."
        sh "$CLIENT_STOP_SCRIPT"
    else
        echo "Stopping STM32 CAN sensor client pid=$(cat "$CLIENT_PID") before OTA..."
        kill "$(cat "$CLIENT_PID")" 2>/dev/null || true
        sleep 1
    fi
fi

"$APP_DIR/setup_can.sh" "$IFACE" "$BITRATE"
"$APP" -i "$IFACE" -f "$FIRMWARE" -p "$PACING_US" -t "$TIMEOUT_MS"
