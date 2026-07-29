#!/usr/bin/env python3
"""Build a validated, versioned STM32 CAN OTA package from an App binary."""

from __future__ import annotations

import argparse
import os
import struct
import sys
import tempfile
import zlib
from pathlib import Path

PACKAGE_MAGIC = 0x41544F43  # "COTA" when stored little endian
HEADER_SIZE = 32
FORMAT_VERSION = 1
HARDWARE_ID_STM32F103 = 0xF103
FLAG_ALLOW_DOWNGRADE = 0x01

APP_ADDRESS = 0x08010000
METADATA_ADDRESS = 0x0807F800
APP_MAX_SIZE = METADATA_ADDRESS - APP_ADDRESS
SRAM_START = 0x20000000
SRAM_END = 0x20010000


def parse_uint(text: str) -> int:
    try:
        return int(text, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid integer: {text}") from exc


def parse_version(text: str) -> tuple[int, str]:
    parts = text.split(".")
    if len(parts) not in (3, 4):
        raise argparse.ArgumentTypeError(
            "version must be major.minor.patch or major.minor.patch.build"
        )
    try:
        values = [int(part, 10) for part in parts]
    except ValueError as exc:
        raise argparse.ArgumentTypeError("version components must be decimal") from exc
    if any(value < 0 or value > 255 for value in values):
        raise argparse.ArgumentTypeError("version components must be in 0..255")
    if len(values) == 3:
        values.append(0)
    encoded = (
        (values[0] << 24)
        | (values[1] << 16)
        | (values[2] << 8)
        | values[3]
    )
    if encoded == 0:
        raise argparse.ArgumentTypeError("version 0.0.0.0 is reserved")
    return encoded, ".".join(str(value) for value in values)


def validate_image(image: bytes) -> None:
    if not image:
        raise ValueError("input image is empty")
    if len(image) > APP_MAX_SIZE:
        raise ValueError(
            f"image is {len(image)} bytes; App slot limit is {APP_MAX_SIZE} bytes"
        )
    if len(image) < 8:
        raise ValueError("image is too small to contain an STM32 vector table")

    initial_sp, reset_handler = struct.unpack_from("<II", image)
    if not SRAM_START <= initial_sp <= SRAM_END:
        raise ValueError(f"invalid initial stack pointer 0x{initial_sp:08X}")
    reset_address = reset_handler & ~1
    if (reset_handler & 1) == 0:
        raise ValueError(f"reset handler is not Thumb code: 0x{reset_handler:08X}")
    if not APP_ADDRESS <= reset_address < APP_ADDRESS + len(image):
        raise ValueError(
            f"reset handler 0x{reset_handler:08X} is outside the image"
        )


def build_package(
    image: bytes,
    hardware_id: int,
    firmware_version: int,
    flags: int,
) -> tuple[bytes, int, int]:
    image_crc = zlib.crc32(image) & 0xFFFFFFFF
    header_without_crc = struct.pack(
        "<IHBBHHIIII",
        PACKAGE_MAGIC,
        HEADER_SIZE,
        FORMAT_VERSION,
        flags,
        hardware_id,
        0,
        firmware_version,
        len(image),
        image_crc,
        0,
    )
    if len(header_without_crc) != 28:
        raise AssertionError("internal OTA header layout error")
    header_crc = zlib.crc32(header_without_crc) & 0xFFFFFFFF
    return header_without_crc + struct.pack("<I", header_crc) + image, image_crc, header_crc


def write_atomic(path: Path, data: bytes, force: bool) -> None:
    if path.exists() and not force:
        raise FileExistsError(f"output already exists: {path} (use --force)")
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
    except BaseException:
        try:
            os.unlink(temp_name)
        except FileNotFoundError:
            pass
        raise


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Package an STM32F103 App .bin as a guarded CAN OTA .ota file"
    )
    parser.add_argument("--input", "-i", required=True, type=Path)
    parser.add_argument("--output", "-o", required=True, type=Path)
    parser.add_argument("--version", "-v", required=True)
    parser.add_argument(
        "--hardware-id",
        type=parse_uint,
        default=HARDWARE_ID_STM32F103,
        help="target hardware ID (default: 0xF103)",
    )
    parser.add_argument(
        "--allow-downgrade",
        action="store_true",
        help="set the explicit downgrade-authorization flag",
    )
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    try:
        firmware_version, normalized_version = parse_version(args.version)
        if not 0 < args.hardware_id <= 0xFFFF:
            raise ValueError("hardware ID must be in 1..0xFFFF")
        image = args.input.read_bytes()
        validate_image(image)
        flags = FLAG_ALLOW_DOWNGRADE if args.allow_downgrade else 0
        package, image_crc, header_crc = build_package(
            image, args.hardware_id, firmware_version, flags
        )
        write_atomic(args.output, package, args.force)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"created: {args.output}")
    print(f"hardware: 0x{args.hardware_id:04X}")
    print(f"version: {normalized_version}")
    print(f"image: {len(image)} bytes, crc32=0x{image_crc:08X}")
    print(f"header: {HEADER_SIZE} bytes, crc32=0x{header_crc:08X}")
    print(f"allow_downgrade: {bool(flags & FLAG_ALLOW_DOWNGRADE)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
