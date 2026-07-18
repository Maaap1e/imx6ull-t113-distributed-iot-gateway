#!/bin/sh
set -eu

IFACE="${1:-can0}"
BITRATE="${2:-500000}"

ip link set "$IFACE" down 2>/dev/null || true
ip link set "$IFACE" type can bitrate "$BITRATE" restart-ms 100
ip link set "$IFACE" up

echo "$IFACE is up, bitrate=$BITRATE"
ip -details link show "$IFACE"
