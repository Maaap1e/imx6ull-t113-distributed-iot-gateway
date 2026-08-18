#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
VERSION=$(tr -d '\r\n' < "$BASE_DIR/VERSION")
RC_ASSET_DIR=${1:-}
DIST_DIR=${DIST_DIR:-"$BASE_DIR/dist"}
RELEASE_DIR="$DIST_DIR/v$VERSION-release"

if [ "$VERSION" != "2.1.0" ]; then
    echo "VERSION must be 2.1.0, got $VERSION" >&2
    exit 1
fi

if [ -z "$RC_ASSET_DIR" ]; then
    echo "Usage: $0 /path/to/v2.1.0-rc.1-assets" >&2
    exit 1
fi

mkdir -p "$RELEASE_DIR"

copy_asset()
{
    source_path=$1
    target_name=${2:-$(basename "$source_path")}

    if [ ! -f "$source_path" ]; then
        echo "missing release asset: $source_path" >&2
        exit 1
    fi
    cp "$source_path" "$RELEASE_DIR/$target_name"
}

copy_asset "$DIST_DIR/iot-gateway-$VERSION-imx6ull.tar.gz"
copy_asset "$DIST_DIR/iot-gateway-$VERSION-imx6ull.tar.gz.sha256"

copy_asset "$RC_ASSET_DIR/stm32_can_ota_ab_secure_bootloader.hex"
copy_asset "$RC_ASSET_DIR/stm32_dht11_can_app_slot_a-v2.1.0.3.bin"
copy_asset "$RC_ASSET_DIR/stm32_dht11_can_app_slot_b-v2.1.0.3.bin"
copy_asset "$RC_ASSET_DIR/stm32-dht11-v2.1.0.3.ota3"
copy_asset "$RC_ASSET_DIR/v2.1.0-rc.1-acceptance-evidence.tar.gz" \
    "v$VERSION-acceptance-evidence.tar.gz"

(
    cd "$RELEASE_DIR"
    sha256sum \
        stm32_can_ota_ab_secure_bootloader.hex \
        stm32_dht11_can_app_slot_a-v2.1.0.3.bin \
        stm32_dht11_can_app_slot_b-v2.1.0.3.bin \
        stm32-dht11-v2.1.0.3.ota3 \
        > "v$VERSION-stm32-assets.sha256"
)

(
    cd "$RELEASE_DIR"
    sha256sum "v$VERSION-acceptance-evidence.tar.gz" \
        > "v$VERSION-acceptance-evidence.tar.gz.sha256"
)

echo "Release assets collected in $RELEASE_DIR"
find "$RELEASE_DIR" -maxdepth 1 -type f -printf '%f\n' | sort
