#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"
MODE="${2:-}"
RUNNING=1

usage()
{
    echo "Usage: $0 imx6ull|t113 [--once]" >&2
    exit 2
}

case "$ROLE" in
    imx6ull|t113) ;;
    *) usage ;;
esac

CONFIG_FILE="${IOT_GATEWAY_CONFIG:-/etc/iot-gateway/$ROLE.conf}"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="$REPO_DIR/config/$ROLE.conf"
fi
if [ ! -f "$CONFIG_FILE" ]; then
    echo "runtime maintenance: missing config for $ROLE" >&2
    exit 1
fi
. "$CONFIG_FILE"

is_uint()
{
    case "$1" in
        ''|*[!0-9]*) return 1 ;;
        *) return 0 ;;
    esac
}

for value in "$LOG_MAX_BYTES" "$LOG_KEEP" "$CSV_MAX_BYTES" "$CSV_KEEP" "$MAINTENANCE_INTERVAL_SECONDS"; do
    if ! is_uint "$value"; then
        echo "runtime maintenance: invalid numeric setting: $value" >&2
        exit 1
    fi
done

shift_archives()
{
    file=$1
    keep=$2

    if [ "$keep" -le 0 ]; then
        return
    fi

    rm -f "$file.$keep"
    index=$((keep - 1))
    while [ "$index" -ge 1 ]; do
        if [ -f "$file.$index" ]; then
            mv -f "$file.$index" "$file.$((index + 1))"
        fi
        index=$((index - 1))
    done
}

file_size()
{
    if [ ! -f "$1" ]; then
        echo 0
        return
    fi
    wc -c < "$1" | tr -d ' '
}

rotate_copytruncate()
{
    file=$1
    max_bytes=$2
    keep=$3
    size=$(file_size "$file")

    if [ "$size" -lt "$max_bytes" ]; then
        return
    fi
    if [ "$keep" -le 0 ]; then
        : > "$file"
        return
    fi

    shift_archives "$file" "$keep"
    tmp="$file.1.tmp.$$"
    if cp "$file" "$tmp" && mv -f "$tmp" "$file.1"; then
        : > "$file"
        echo "rotated log $file ($size bytes)"
    else
        rm -f "$tmp"
        echo "failed to rotate log $file" >&2
    fi
}

rotate_rename()
{
    file=$1
    max_bytes=$2
    keep=$3
    size=$(file_size "$file")

    if [ "$size" -lt "$max_bytes" ]; then
        return
    fi
    if [ "$keep" -le 0 ]; then
        rm -f "$file"
        return
    fi

    shift_archives "$file" "$keep"
    if mv -f "$file" "$file.1"; then
        echo "rotated csv $file ($size bytes)"
    fi
}

maintain_once()
{
    if [ "$ROLE" = "imx6ull" ]; then
        rotate_copytruncate "$SUPERVISOR_LOG" "$LOG_MAX_BYTES" "$LOG_KEEP"
        rotate_copytruncate "$GATEWAY_LOG" "$LOG_MAX_BYTES" "$LOG_KEEP"
        rotate_copytruncate "$STM32_CAN_LOG" "$LOG_MAX_BYTES" "$LOG_KEEP"
        rotate_rename "$STM32_CAN_CSV" "$CSV_MAX_BYTES" "$CSV_KEEP"
    else
        rotate_copytruncate "$SUPERVISOR_LOG" "$LOG_MAX_BYTES" "$LOG_KEEP"
        rotate_copytruncate "$T113_LOG" "$LOG_MAX_BYTES" "$LOG_KEEP"
        rotate_rename "$T113_CSV" "$CSV_MAX_BYTES" "$CSV_KEEP"
    fi
}

if [ "$MODE" = "--once" ]; then
    maintain_once
    exit 0
fi

trap 'RUNNING=0' INT TERM
while [ "$RUNNING" -eq 1 ]; do
    maintain_once
    sleep "$MAINTENANCE_INTERVAL_SECONDS" &
    sleeper=$!
    wait "$sleeper" 2>/dev/null || true
done
