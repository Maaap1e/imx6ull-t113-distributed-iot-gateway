#!/usr/bin/env python3
"""Create deliberately invalid OTA3 packages for hardware acceptance tests."""

from __future__ import annotations

import argparse
import hashlib
import struct
import zlib
from pathlib import Path


HEADER_SIZE = 248
HARDWARE_ID_OFFSET = 8
KEY_ID_OFFSET = 172
SIGNATURE_OFFSET = 180
SIGNATURE_SIZE = 64
HEADER_CRC_OFFSET = 244


def mutate_header_byte(package: bytes, offset: int) -> bytes:
    if len(package) < HEADER_SIZE:
        raise ValueError("input is smaller than the OTA3 header")
    stored_crc = struct.unpack_from("<I", package, HEADER_CRC_OFFSET)[0]
    actual_crc = zlib.crc32(package[:HEADER_CRC_OFFSET]) & 0xFFFFFFFF
    if stored_crc != actual_crc:
        raise ValueError("input OTA3 header CRC32 is already invalid")

    output = bytearray(package)
    output[offset] ^= 0x01
    new_crc = zlib.crc32(output[:HEADER_CRC_OFFSET]) & 0xFFFFFFFF
    struct.pack_into("<I", output, HEADER_CRC_OFFSET, new_crc)
    return bytes(output)


def corrupt_signature(package: bytes) -> bytes:
    return mutate_header_byte(
        package, SIGNATURE_OFFSET + SIGNATURE_SIZE - 1
    )


def corrupt_key_id(package: bytes) -> bytes:
    return mutate_header_byte(package, KEY_ID_OFFSET)


def corrupt_hardware_id(package: bytes) -> bytes:
    return mutate_header_byte(package, HARDWARE_ID_OFFSET)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Flip one ECDSA signature bit and recompute only Header CRC32. "
            "The resulting package must be rejected by the Bootloader."
        )
    )
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--kind",
        choices=("signature", "key-id", "hardware-id"),
        default="signature",
    )
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    if args.output.exists() and not args.force:
        parser.error(f"output already exists: {args.output}")

    source = args.input.read_bytes()
    if args.kind == "signature":
        result = corrupt_signature(source)
        mutation = "ECDSA signature final byte XOR 0x01"
    elif args.kind == "key-id":
        result = corrupt_key_id(source)
        mutation = "trusted Key ID least-significant byte XOR 0x01"
    else:
        result = corrupt_hardware_id(source)
        mutation = "Hardware ID least-significant byte XOR 0x01"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)

    print(f"created: {args.output}")
    print(f"mutation: {mutation}")
    print("header_crc32: recomputed and valid")
    print(f"source_sha256: {hashlib.sha256(source).hexdigest()}")
    print(f"output_sha256: {hashlib.sha256(result).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
