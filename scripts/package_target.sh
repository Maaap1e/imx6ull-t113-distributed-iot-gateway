#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
ROLE="${1:-}"
VERSION=$(tr -d '\r\n' < "$BASE_DIR/VERSION")
DIST_DIR="${DIST_DIR:-$BASE_DIR/dist}"

case "$ROLE" in
    imx6ull|t113) ;;
    *) echo "Usage: $0 imx6ull|t113" >&2; exit 2 ;;
esac

require_file()
{
    if [ ! -f "$1" ]; then
        echo "Missing build output: $1" >&2
        echo "Cross-compile the $ROLE target before packaging." >&2
        exit 1
    fi
}

if [ "$ROLE" = "imx6ull" ]; then
    require_file "$BASE_DIR/linux/imx6ull_gateway/imx6ull_gateway_app"
    require_file "$BASE_DIR/linux/can_sensor_client/stm32_can_sensor_client"
    require_file "$BASE_DIR/linux/can_ota_host/stm32_can_ota_host"
else
    require_file "$BASE_DIR/t113/tcp_receiver/t113_display_app"
fi

mkdir -p "$DIST_DIR"
BUNDLE_NAME="iot-gateway-$VERSION-$ROLE"
ARCHIVE="$DIST_DIR/$BUNDLE_NAME.tar.gz"
if [ -e "$ARCHIVE" ] && [ "${FORCE:-0}" != "1" ]; then
    echo "Package already exists: $ARCHIVE" >&2
    echo "Remove it or rerun with FORCE=1." >&2
    exit 1
fi

STAGE_ROOT=$(mktemp -d)
case "$STAGE_ROOT" in /tmp/*|/var/tmp/*) ;; *) echo "Unexpected temporary path" >&2; exit 1 ;; esac
trap 'rm -rf "$STAGE_ROOT"' EXIT HUP INT TERM
BUNDLE_DIR="$STAGE_ROOT/$BUNDLE_NAME"
mkdir -p "$BUNDLE_DIR/scripts" "$BUNDLE_DIR/config" "$BUNDLE_DIR/deploy/busybox" \
    "$BUNDLE_DIR/deploy/openwrt" "$BUNDLE_DIR/deploy/systemd" "$BUNDLE_DIR/docs"

cp "$BASE_DIR/VERSION" "$BASE_DIR/CHANGELOG.md" "$BASE_DIR/README.md" "$BUNDLE_DIR/"
cp "$BASE_DIR/config/$ROLE.conf" "$BUNDLE_DIR/config/"
cp "$BASE_DIR/deploy/busybox/S90iot-$ROLE" "$BUNDLE_DIR/deploy/busybox/"
if [ "$ROLE" = "t113" ]; then
    cp "$BASE_DIR/deploy/openwrt/iot-t113" "$BUNDLE_DIR/deploy/openwrt/"
fi
cp "$BASE_DIR/deploy/systemd/"*.service "$BUNDLE_DIR/deploy/systemd/"
cp "$BASE_DIR/docs/RUNTIME_MANAGEMENT.md" "$BASE_DIR/docs/V1_ACCEPTANCE.md" "$BUNDLE_DIR/docs/"

for name in install_target.sh healthcheck.sh runtime_maintenance.sh \
    runtime_supervisor.sh run_can_client.sh run_gateway.sh run_t113.sh \
    start_stack.sh stop_stack.sh; do
    cp "$BASE_DIR/scripts/$name" "$BUNDLE_DIR/scripts/$name"
    chmod 755 "$BUNDLE_DIR/scripts/$name"
done

if [ "$ROLE" = "imx6ull" ]; then
    mkdir -p "$BUNDLE_DIR/linux/imx6ull_gateway" \
        "$BUNDLE_DIR/linux/can_sensor_client" "$BUNDLE_DIR/linux/can_ota_host"
    cp "$BASE_DIR/linux/imx6ull_gateway/imx6ull_gateway_app" \
        "$BASE_DIR/linux/imx6ull_gateway/start_gateway.sh" \
        "$BASE_DIR/linux/imx6ull_gateway/stop_gateway.sh" \
        "$BUNDLE_DIR/linux/imx6ull_gateway/"
    cp "$BASE_DIR/linux/can_sensor_client/stm32_can_sensor_client" \
        "$BASE_DIR/linux/can_sensor_client/setup_can.sh" \
        "$BASE_DIR/linux/can_sensor_client/start_client.sh" \
        "$BASE_DIR/linux/can_sensor_client/stop_client.sh" \
        "$BUNDLE_DIR/linux/can_sensor_client/"
    cp "$BASE_DIR/linux/can_ota_host/stm32_can_ota_host" \
        "$BASE_DIR/linux/can_ota_host/setup_can.sh" \
        "$BASE_DIR/linux/can_ota_host/run_ota.sh" \
        "$BUNDLE_DIR/linux/can_ota_host/"
else
    mkdir -p "$BUNDLE_DIR/t113/tcp_receiver"
    cp "$BASE_DIR/t113/tcp_receiver/t113_display_app" \
        "$BASE_DIR/t113/tcp_receiver/start_t113.sh" \
        "$BASE_DIR/t113/tcp_receiver/stop_t113.sh" \
        "$BUNDLE_DIR/t113/tcp_receiver/"
fi

find "$BUNDLE_DIR" -type f \( -name '*.sh' -o -name 'S90iot-*' -o -perm -u+x \) \
    -exec chmod 755 {} \;
tar -czf "$ARCHIVE" -C "$STAGE_ROOT" "$BUNDLE_NAME"
if command -v sha256sum >/dev/null 2>&1; then
    (cd "$DIST_DIR" && sha256sum "$BUNDLE_NAME.tar.gz" > "$BUNDLE_NAME.tar.gz.sha256")
fi
echo "Created $ARCHIVE"
echo "Copy this archive to the $ROLE board, extract it, then run:"
echo "  sh scripts/install_target.sh $ROLE"
