#!/bin/sh
set -eu

APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$APP_DIR/../.." && pwd)
APP="$APP_DIR/stm32_can_ota_host"
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/imx6ull.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$BASE_DIR/config/imx6ull.conf"
fi
[ ! -f "$CONFIG_FILE" ] || . "$CONFIG_FILE"

IFACE="${IFACE:-${CAN_IFACE:-can0}}"
BITRATE="${BITRATE:-${CAN_BITRATE:-500000}}"
PACING_US="${PACING_US:-20000}"
TIMEOUT_MS="${TIMEOUT_MS:-8000}"
CLIENT_PID="${CAN_CLIENT_PID:-${STM32_CAN_PID:-/tmp/stm32_can_client.pid}}"
OTA_LOCK="${OTA_LOCK:-/tmp/stm32_can_ota.lock}"
CLIENT_WAS_RUNNING=0
SYSTEMD_WAS_RUNNING=0
CLIENT_STOP_SCRIPT=""
CLIENT_START_SCRIPT=""

if [ $# -lt 1 ]; then
    echo "Usage: $0 stm32_app.bin"
    exit 1
fi

FIRMWARE="$1"

for script in \
    "$APP_DIR/stop_client.sh" \
    "$APP_DIR/../can_sensor_client/stop_client.sh"
do
    if [ -f "$script" ]; then
        CLIENT_STOP_SCRIPT="$script"
        break
    fi
done

for script in \
    "$APP_DIR/start_client.sh" \
    "$APP_DIR/../can_sensor_client/start_client.sh"
do
    if [ -f "$script" ]; then
        CLIENT_START_SCRIPT="$script"
        break
    fi
done

restart_can_client()
{
    rm -f "$OTA_LOCK"
    if [ "$SYSTEMD_WAS_RUNNING" -eq 1 ]; then
        echo "Restarting iot-can-sensor.service..."
        systemctl start iot-can-sensor.service || \
            echo "warning: failed to restart iot-can-sensor.service" >&2
    elif [ "$CLIENT_WAS_RUNNING" -eq 1 ] && [ -n "$CLIENT_START_SCRIPT" ]; then
        echo "Restarting STM32 CAN sensor client..."
        if ! IOT_GATEWAY_CONFIG="$CONFIG_FILE" sh "$CLIENT_START_SCRIPT"; then
            echo "warning: failed to restart STM32 CAN sensor client" >&2
        fi
    fi
    return 0
}

mkdir -p "$(dirname "$OTA_LOCK")"
if [ -f "$OTA_LOCK" ]; then
    lock_pid=$(cat "$OTA_LOCK" 2>/dev/null || true)
    case "$lock_pid" in
        ''|*[!0-9]*) ;;
        *)
            if kill -0 "$lock_pid" 2>/dev/null; then
                echo "another OTA is already running, pid=$lock_pid" >&2
                exit 1
            fi
            ;;
    esac
fi
echo "$$" > "$OTA_LOCK"
trap restart_can_client EXIT
trap 'exit 130' HUP INT TERM

if command -v systemctl >/dev/null 2>&1 && \
   systemctl is-active --quiet iot-can-sensor.service 2>/dev/null; then
    SYSTEMD_WAS_RUNNING=1
    echo "Stopping iot-can-sensor.service before OTA..."
    systemctl stop iot-can-sensor.service
elif [ -f "$CLIENT_PID" ] && kill -0 "$(cat "$CLIENT_PID")" 2>/dev/null; then
    CLIENT_WAS_RUNNING=1
    if [ -n "$CLIENT_STOP_SCRIPT" ]; then
        echo "Stopping STM32 CAN sensor client before OTA..."
        IOT_GATEWAY_CONFIG="$CONFIG_FILE" sh "$CLIENT_STOP_SCRIPT"
    else
        echo "Stopping STM32 CAN sensor client pid=$(cat "$CLIENT_PID") before OTA..."
        kill "$(cat "$CLIENT_PID")" 2>/dev/null || true
        sleep 1
    fi
fi

"$APP_DIR/setup_can.sh" "$IFACE" "$BITRATE"
"$APP" -i "$IFACE" -f "$FIRMWARE" -p "$PACING_US" -t "$TIMEOUT_MS"
