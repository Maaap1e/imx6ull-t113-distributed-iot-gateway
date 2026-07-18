#!/bin/sh
set -eu

PID="${PID:-/tmp/stm32_can_client.pid}"

if [ -f "$PID" ]; then
    kill "$(cat "$PID")" 2>/dev/null || true
    sleep 1
fi

killall stm32_can_sensor_client 2>/dev/null || true
rm -f "$PID"
echo "stm32_can_sensor_client stopped"
