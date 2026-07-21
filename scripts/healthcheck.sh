#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"
case "$ROLE" in imx6ull|t113) ;; *) echo "Usage: $0 imx6ull|t113" >&2; exit 2 ;; esac
CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/$ROLE.conf}"
[ -f "$CONFIG_FILE" ] || CONFIG_FILE="$BASE_DIR/config/$ROLE.conf"
. "$CONFIG_FILE"
FAILED=0

check_pid()
{
    name=$1
    pid_file=$2
    pid=$(cat "$pid_file" 2>/dev/null || true)
    case "$pid" in
        ''|*[!0-9]*) echo "FAIL $name: missing/invalid pid file $pid_file"; FAILED=1 ;;
        *)
            if kill -0 "$pid" 2>/dev/null; then
                echo "OK   $name: pid=$pid"
            else
                echo "FAIL $name: stale pid=$pid"
                FAILED=1
            fi
            ;;
    esac
}

show_file()
{
    file=$1
    if [ -f "$file" ]; then
        bytes=$(wc -c < "$file" | tr -d ' ')
        echo "INFO file=$file bytes=$bytes"
    else
        echo "INFO file=$file missing"
    fi
}

if [ "$ROLE" = "imx6ull" ]; then
    check_pid supervisor "$SUPERVISOR_PID"
    check_pid can-client "$STM32_CAN_PID"
    check_pid gateway "$GATEWAY_PID"
    show_file "$STM32_CAN_STATE"
    show_file "$STM32_CAN_CSV"
    show_file "$STM32_CAN_LOG"
    show_file "$GATEWAY_LOG"
else
    check_pid supervisor "$SUPERVISOR_PID"
    check_pid t113-receiver "$T113_PID"
    show_file "$T113_STATE"
    show_file "$T113_CSV"
    show_file "$T113_LOG"
fi

df -h /tmp 2>/dev/null || true
free -h 2>/dev/null || free 2>/dev/null || true
exit "$FAILED"
