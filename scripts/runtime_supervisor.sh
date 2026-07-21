#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"
RUNNING=1

case "$ROLE" in
    imx6ull|t113) ;;
    *) echo "Usage: $0 imx6ull|t113" >&2; exit 2 ;;
esac

CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/$ROLE.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/$ROLE.conf"
. "$CONFIG_FILE"

case "$SUPERVISOR_INTERVAL_SECONDS" in
    ''|*[!0-9]*|0) echo "Invalid SUPERVISOR_INTERVAL_SECONDS" >&2; exit 1 ;;
esac

is_alive()
{
    pid_file=$1
    [ -f "$pid_file" ] || return 1
    pid=$(cat "$pid_file" 2>/dev/null || true)
    case "$pid" in ''|*[!0-9]*) return 1 ;; esac
    kill -0 "$pid" 2>/dev/null
}

ota_in_progress()
{
    [ -f "$OTA_LOCK" ] || return 1
    ota_pid=$(cat "$OTA_LOCK" 2>/dev/null || true)
    case "$ota_pid" in
        ''|*[!0-9]*) rm -f "$OTA_LOCK"; return 1 ;;
    esac
    if kill -0 "$ota_pid" 2>/dev/null; then
        return 0
    fi
    echo "supervisor: removing stale OTA lock"
    rm -f "$OTA_LOCK"
    return 1
}

start_missing()
{
    if [ "$ROLE" = "imx6ull" ]; then
        if ! ota_in_progress && ! is_alive "$STM32_CAN_PID"; then
            echo "supervisor: restarting CAN client"
            IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/linux/can_sensor_client/start_client.sh"
        fi
        if ! is_alive "$GATEWAY_PID"; then
            echo "supervisor: restarting gateway"
            IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/linux/imx6ull_gateway/start_gateway.sh"
        fi
    elif ! is_alive "$T113_PID"; then
        echo "supervisor: restarting T113 receiver"
        IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/t113/tcp_receiver/start_t113.sh"
    fi
}

trap 'RUNNING=0' INT TERM
while [ "$RUNNING" -eq 1 ]; do
    start_missing || true
    IOT_GATEWAY_CONFIG="$CONFIG_FILE" "$BASE_DIR/scripts/runtime_maintenance.sh" "$ROLE" --once || true
    sleep "$SUPERVISOR_INTERVAL_SECONDS" &
    sleeper=$!
    wait "$sleeper" 2>/dev/null || true
done
