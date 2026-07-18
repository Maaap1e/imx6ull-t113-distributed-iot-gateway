#!/bin/sh
set -eu

PID="/tmp/imx6ull_gateway_app.pid"

if [ -f "$PID" ]; then
    kill "$(cat "$PID")" 2>/dev/null || true
    sleep 1
fi

killall imx6ull_gateway_app 2>/dev/null || true
rm -f "$PID"
echo "imx6ull_gateway_app stopped"
