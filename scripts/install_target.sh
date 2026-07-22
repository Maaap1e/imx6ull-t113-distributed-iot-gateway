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

register_rc_local()
{
    rc_local="$DESTDIR/etc/rc.local"
    boot_command="/etc/init.d/S90iot-$ROLE start"

    if [ ! -f "$rc_local" ]; then
        return
    fi
    if grep -F "$boot_command" "$rc_local" >/dev/null 2>&1; then
        echo "rc.local already starts S90iot-$ROLE"
        return
    fi

    rc_tmp="$rc_local.iot.$$"
    awk -v command="$boot_command" '
        BEGIN { inserted = 0 }
        !inserted && $0 ~ /^[[:space:]]*exit[[:space:]]+0[[:space:]]*$/ {
            print command
            inserted = 1
        }
        { print }
        END {
            if (!inserted) print command
        }
    ' "$rc_local" > "$rc_tmp"
    mv -f "$rc_tmp" "$rc_local"
    chmod 755 "$rc_local"
    echo "Registered $boot_command in /etc/rc.local"
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
elif [ "$ROLE" = "t113" ] && [ -x "$DESTDIR/etc/rc.common" ] && \
     [ -d "$DESTDIR/etc/rc.d" ]; then
    copy_exec "$BASE_DIR/deploy/openwrt/iot-t113" "$DESTDIR/etc/init.d/iot-t113"
    if [ -z "$DESTDIR" ]; then
        /etc/init.d/iot-t113 enable
    else
        ln -sf ../init.d/iot-t113 "$DESTDIR/etc/rc.d/S90iot-t113"
    fi
    load_script_conf="$DESTDIR/etc/init.d/load_script.conf"
    if [ -f "$load_script_conf" ] && \
       ! grep -q '^iot-t113$' "$load_script_conf" 2>/dev/null; then
        echo 'iot-t113' >> "$load_script_conf"
        echo "Registered iot-t113 in /etc/init.d/load_script.conf"
    fi
    echo "Installed and enabled OpenWrt/Tina service: /etc/init.d/iot-t113"
elif [ -d "$DESTDIR/etc/init.d" ]; then
    copy_exec "$BASE_DIR/deploy/busybox/S90iot-$ROLE" "$DESTDIR/etc/init.d/S90iot-$ROLE"
    echo "Installed BusyBox init script: /etc/init.d/S90iot-$ROLE"
    register_rc_local
else
    echo "No systemd/init.d directory found; use $APP_DIR/scripts/start_stack.sh $ROLE"
fi

echo "Installed $ROLE runtime under /opt/iot-gateway"
