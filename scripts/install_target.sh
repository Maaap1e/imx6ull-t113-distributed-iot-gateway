#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"
DESTDIR="${DESTDIR:-}"
APP_DIR="$DESTDIR/opt/iot-gateway"
ETC_DIR="$DESTDIR/etc/iot-gateway"

case "$ROLE" in imx6ull|t113) ;; *) echo "Usage: $0 imx6ull|t113" >&2; exit 2 ;; esac

require_binary()
{
    if [ ! -f "$1" ]; then
        echo "Missing binary: $1 (build it before installing)" >&2
        exit 1
    fi
}

copy_exec()
{
    src=$1
    dst=$2
    mkdir -p "$(dirname "$dst")"
    cp "$src" "$dst"
    chmod 755 "$dst"
}

mkdir -p "$APP_DIR/scripts" "$APP_DIR/config" "$ETC_DIR"
for script in "$BASE_DIR"/scripts/*.sh; do
    copy_exec "$script" "$APP_DIR/scripts/$(basename "$script")"
done

if [ "$ROLE" = "imx6ull" ]; then
    require_binary "$BASE_DIR/linux/imx6ull_gateway/imx6ull_gateway_app"
    require_binary "$BASE_DIR/linux/can_sensor_client/stm32_can_sensor_client"
    require_binary "$BASE_DIR/linux/can_ota_host/stm32_can_ota_host"
    mkdir -p "$APP_DIR/linux/imx6ull_gateway" "$APP_DIR/linux/can_sensor_client" \
        "$APP_DIR/linux/can_ota_host"
    copy_exec "$BASE_DIR/linux/imx6ull_gateway/imx6ull_gateway_app" \
        "$APP_DIR/linux/imx6ull_gateway/imx6ull_gateway_app"
    copy_exec "$BASE_DIR/linux/can_sensor_client/stm32_can_sensor_client" \
        "$APP_DIR/linux/can_sensor_client/stm32_can_sensor_client"
    copy_exec "$BASE_DIR/linux/can_sensor_client/setup_can.sh" \
        "$APP_DIR/linux/can_sensor_client/setup_can.sh"
    copy_exec "$BASE_DIR/linux/can_sensor_client/start_client.sh" \
        "$APP_DIR/linux/can_sensor_client/start_client.sh"
    copy_exec "$BASE_DIR/linux/can_sensor_client/stop_client.sh" \
        "$APP_DIR/linux/can_sensor_client/stop_client.sh"
    copy_exec "$BASE_DIR/linux/imx6ull_gateway/start_gateway.sh" \
        "$APP_DIR/linux/imx6ull_gateway/start_gateway.sh"
    copy_exec "$BASE_DIR/linux/imx6ull_gateway/stop_gateway.sh" \
        "$APP_DIR/linux/imx6ull_gateway/stop_gateway.sh"
    copy_exec "$BASE_DIR/linux/can_ota_host/stm32_can_ota_host" \
        "$APP_DIR/linux/can_ota_host/stm32_can_ota_host"
    copy_exec "$BASE_DIR/linux/can_ota_host/setup_can.sh" \
        "$APP_DIR/linux/can_ota_host/setup_can.sh"
    copy_exec "$BASE_DIR/linux/can_ota_host/run_ota.sh" \
        "$APP_DIR/linux/can_ota_host/run_ota.sh"
else
    require_binary "$BASE_DIR/t113/tcp_receiver/t113_display_app"
    mkdir -p "$APP_DIR/t113/tcp_receiver"
    copy_exec "$BASE_DIR/t113/tcp_receiver/t113_display_app" \
        "$APP_DIR/t113/tcp_receiver/t113_display_app"
    copy_exec "$BASE_DIR/t113/tcp_receiver/start_t113.sh" \
        "$APP_DIR/t113/tcp_receiver/start_t113.sh"
    copy_exec "$BASE_DIR/t113/tcp_receiver/stop_t113.sh" \
        "$APP_DIR/t113/tcp_receiver/stop_t113.sh"
fi

cp "$BASE_DIR/config/$ROLE.conf" "$APP_DIR/config/$ROLE.conf"
if [ ! -f "$ETC_DIR/$ROLE.conf" ]; then
    cp "$BASE_DIR/config/$ROLE.conf" "$ETC_DIR/$ROLE.conf"
else
    cp "$BASE_DIR/config/$ROLE.conf" "$ETC_DIR/$ROLE.conf.new"
    echo "Kept existing $ETC_DIR/$ROLE.conf; review $ROLE.conf.new"
fi

if [ -d "$DESTDIR/etc/systemd/system" ]; then
    cp "$BASE_DIR/deploy/systemd/iot-runtime-maintenance@.service" "$DESTDIR/etc/systemd/system/"
    if [ "$ROLE" = "imx6ull" ]; then
        cp "$BASE_DIR/deploy/systemd/iot-can-sensor.service" "$DESTDIR/etc/systemd/system/"
        cp "$BASE_DIR/deploy/systemd/iot-imx6ull-gateway.service" "$DESTDIR/etc/systemd/system/"
    else
        cp "$BASE_DIR/deploy/systemd/iot-t113-receiver.service" "$DESTDIR/etc/systemd/system/"
    fi
    echo "Installed systemd units; enable them as described in docs/RUNTIME_MANAGEMENT.md"
elif [ -d "$DESTDIR/etc/init.d" ]; then
    copy_exec "$BASE_DIR/deploy/busybox/S90iot-$ROLE" "$DESTDIR/etc/init.d/S90iot-$ROLE"
    echo "Installed BusyBox init script: /etc/init.d/S90iot-$ROLE"
else
    echo "No systemd/init.d directory found; use $APP_DIR/scripts/start_stack.sh $ROLE"
fi

echo "Installed $ROLE runtime under /opt/iot-gateway"
