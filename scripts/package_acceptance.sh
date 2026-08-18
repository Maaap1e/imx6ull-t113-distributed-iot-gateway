#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
VERSION="${PACKAGE_VERSION:-$(tr -d '\r\n' < "$BASE_DIR/VERSION")}"
EVIDENCE_VERSION="${EVIDENCE_VERSION:-}"
if [ -z "$EVIDENCE_VERSION" ]; then
    EVIDENCE_POINTER="$BASE_DIR/docs/acceptance/v$VERSION/EVIDENCE_SOURCE_VERSION"
    if [ -f "$EVIDENCE_POINTER" ]; then
        EVIDENCE_VERSION=$(tr -d '\r\n' < "$EVIDENCE_POINTER")
    else
        EVIDENCE_VERSION="$VERSION"
    fi
fi
SOURCE_NAME="v$EVIDENCE_VERSION"
SOURCE_DIR="$BASE_DIR/docs/acceptance/$SOURCE_NAME"
DIST_DIR="${DIST_DIR:-$BASE_DIR/dist}"
ARCHIVE_NAME="v$VERSION-acceptance-evidence.tar.gz"
ARCHIVE="$DIST_DIR/$ARCHIVE_NAME"

if [ ! -f "$SOURCE_DIR/RESULT.md" ] || \
   [ ! -f "$SOURCE_DIR/EVIDENCE_INDEX.md" ] || \
   [ ! -f "$SOURCE_DIR/EVIDENCE_SHA256SUMS.txt" ]; then
    echo "Incomplete acceptance directory: $SOURCE_DIR" >&2
    exit 1
fi

if ! command -v sha256sum >/dev/null 2>&1; then
    echo "sha256sum is required to verify acceptance evidence" >&2
    exit 1
fi

raw_count=$(find "$SOURCE_DIR/raw" -type f | wc -l | tr -d ' ')
manifest_count=$(awk '$2 ~ /^raw\// { count++ } END { print count + 0 }' \
    "$SOURCE_DIR/EVIDENCE_SHA256SUMS.txt")
if [ "$raw_count" -ne "$manifest_count" ]; then
    echo "Evidence manifest does not cover every raw file" >&2
    echo "raw files: $raw_count, manifest entries: $manifest_count" >&2
    exit 1
fi

(cd "$SOURCE_DIR" && tr -d '\r' < EVIDENCE_SHA256SUMS.txt | sha256sum -c -)

mkdir -p "$DIST_DIR"
if [ -e "$ARCHIVE" ] && [ "${FORCE:-0}" != "1" ]; then
    echo "Acceptance package already exists: $ARCHIVE" >&2
    echo "Remove it or rerun with FORCE=1." >&2
    exit 1
fi

tar -czf "$ARCHIVE" -C "$BASE_DIR/docs/acceptance" "$SOURCE_NAME"
(cd "$DIST_DIR" && sha256sum "$ARCHIVE_NAME" > "$ARCHIVE_NAME.sha256")

echo "Created $ARCHIVE"
echo "Created $ARCHIVE.sha256"
if [ "$EVIDENCE_VERSION" != "$VERSION" ]; then
    echo "Evidence source: v$EVIDENCE_VERSION (promoted as v$VERSION)"
fi
