#!/bin/sh
set -eu

PID="/tmp/t113_display_app.pid"

if [ -f "$PID" ]; then
    kill "$(cat "$PID")" 2>/dev/null || true
    sleep 1
fi

killall t113_display_app 2>/dev/null || true
rm -f "$PID"
echo "t113_display_app stopped"
