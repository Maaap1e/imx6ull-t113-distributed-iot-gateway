#!/usr/bin/env python3
"""Create a signed, resumable STM32F103 dual-image A/B CAN OTA3 package."""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys
import tempfile
import zlib
from dataclasses import dataclass
from pathlib import Path

try:
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec, utils
except ImportError as exc:
    raise SystemExit(
        "cryptography is required: python3 -m pip install cryptography"
    ) from exc

PACKAGE_MAGIC = 0x3341544F
HEADER_SIZE = 248
FORMAT_VERSION = 3
HARDWARE_ID_STM32F103 = 0xF103
FLAG_ALLOW_DOWNGRADE = 0x01
HASH_SHA256 = 1
SIGNATURE_ECDSA_P256 = 1
AUTH_BYTES = 180
SIGNATURE_SIZE = 64
HEADER_CRC_BYTES = 244
BLOCK_SIZE = 0x800
SLOT_A_ADDRESS = 0x08010000
SLOT_B_ADDRESS = 0x08047000
SLOT_SIZE = 0x00037000
SRAM_START = 0x20000000
SRAM_END = 0x20010000


@dataclass(frozen=True)
class BuiltPackage:
    data: bytes
    image_a_sha256: bytes
    image_b_sha256: bytes
    image_a_crc32: int
    image_b_crc32: int
    block_hashes_a: bytes
    block_hashes_b: bytes
    key_id: int
    signature: bytes


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
        raise argparse.ArgumentTypeError(
            "version components must be decimal"
        ) from exc
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


def validate_image(image: bytes, slot_address: int, slot_name: str) -> None:
    if len(image) < 8:
        raise ValueError(f"{slot_name} image has no vector table")
    if len(image) > SLOT_SIZE:
        raise ValueError(
            f"{slot_name} image is {len(image)} bytes; slot limit is "
            f"{SLOT_SIZE} bytes"
        )
    initial_sp, reset_handler = struct.unpack_from("<II", image)
    reset_address = reset_handler & ~1
    if not SRAM_START <= initial_sp <= SRAM_END:
        raise ValueError(f"{slot_name} invalid initial SP 0x{initial_sp:08X}")
    if (reset_handler & 1) == 0:
        raise ValueError(
            f"{slot_name} reset handler is not Thumb code: "
            f"0x{reset_handler:08X}"
        )
    if not slot_address <= reset_address < slot_address + len(image):
        raise ValueError(
            f"{slot_name} reset handler 0x{reset_handler:08X} is outside "
            f"its linked image at 0x{slot_address:08X}"
        )


def public_key_bytes(private_key: ec.EllipticCurvePrivateKey) -> bytes:
    numbers = private_key.public_key().public_numbers()
    return numbers.x.to_bytes(32, "big") + numbers.y.to_bytes(32, "big")


def public_key_id(public_key: bytes) -> int:
    return struct.unpack("<I", hashlib.sha256(public_key).digest()[:4])[0]


def load_private_key(path: Path, password: bytes | None):
    key = serialization.load_pem_private_key(
        path.read_bytes(), password=password, backend=default_backend()
    )
    if not isinstance(key, ec.EllipticCurvePrivateKey):
        raise ValueError("signing key is not an EC private key")
    if not isinstance(key.curve, ec.SECP256R1):
        raise ValueError("signing key must use ECDSA P-256 (secp256r1)")
    return key


def build_block_hash_table(image: bytes) -> bytes:
    return b"".join(
        hashlib.sha256(image[offset : offset + BLOCK_SIZE]).digest()
        for offset in range(0, len(image), BLOCK_SIZE)
    )


def build_package(
    image_a: bytes,
    image_b: bytes,
    hardware_id: int,
    firmware_version: int,
    flags: int,
    private_key: ec.EllipticCurvePrivateKey,
) -> BuiltPackage:
    validate_image(image_a, SLOT_A_ADDRESS, "Slot A")
    validate_image(image_b, SLOT_B_ADDRESS, "Slot B")
    if flags & ~FLAG_ALLOW_DOWNGRADE:
        raise ValueError("unknown package flags")

    a_crc = zlib.crc32(image_a) & 0xFFFFFFFF
    b_crc = zlib.crc32(image_b) & 0xFFFFFFFF
    a_sha = hashlib.sha256(image_a).digest()
    b_sha = hashlib.sha256(image_b).digest()
    a_blocks = build_block_hash_table(image_a)
    b_blocks = build_block_hash_table(image_b)
    a_block_count = len(a_blocks) // 32
    b_block_count = len(b_blocks) // 32
    a_table_sha = hashlib.sha256(a_blocks).digest()
    b_table_sha = hashlib.sha256(b_blocks).digest()
    key_id = public_key_id(public_key_bytes(private_key))

    authenticated = (
        struct.pack(
            "<IHBBHBBIIII",
            PACKAGE_MAGIC,
            HEADER_SIZE,
            FORMAT_VERSION,
            flags,
            hardware_id,
            SIGNATURE_ECDSA_P256,
            HASH_SHA256,
            firmware_version,
            BLOCK_SIZE,
            len(image_a),
            a_crc,
        )
        + a_sha
        + struct.pack("<HH", a_block_count, 0)
        + a_table_sha
        + struct.pack("<II", len(image_b), b_crc)
        + b_sha
        + struct.pack("<HH", b_block_count, 0)
        + b_table_sha
        + struct.pack("<II", key_id, 0)
    )
    if len(authenticated) != AUTH_BYTES:
        raise AssertionError("internal authenticated-header layout error")

    manifest_digest = hashlib.sha256(authenticated).digest()
    der_signature = private_key.sign(
        manifest_digest,
        ec.ECDSA(utils.Prehashed(hashes.SHA256())),
    )
    r, s = utils.decode_dss_signature(der_signature)
    signature = r.to_bytes(32, "big") + s.to_bytes(32, "big")
    header_without_crc = authenticated + signature
    if len(header_without_crc) != HEADER_CRC_BYTES:
        raise AssertionError("internal signed-header layout error")
    header_crc = zlib.crc32(header_without_crc) & 0xFFFFFFFF
    header = header_without_crc + struct.pack("<I", header_crc)

    return BuiltPackage(
        data=header + a_blocks + b_blocks + image_a + image_b,
        image_a_sha256=a_sha,
        image_b_sha256=b_sha,
        image_a_crc32=a_crc,
        image_b_crc32=b_crc,
        block_hashes_a=a_blocks,
        block_hashes_b=b_blocks,
        key_id=key_id,
        signature=signature,
    )


def write_atomic(path: Path, data: bytes, force: bool) -> None:
    if path.exists() and not force:
        raise FileExistsError(f"output already exists: {path} (use --force)")
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def password_from_env(name: str | None) -> bytes | None:
    if name is None:
        return None
    value = os.environ.get(name)
    if value is None:
        raise ValueError(f"private-key password environment variable {name} is unset")
    return value.encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Package Slot A and Slot B STM32F103 binaries as a signed "
            "SHA-256/ECDSA-P256 CAN OTA3 file with persistent block resume"
        )
    )
    parser.add_argument("--slot-a", required=True, type=Path)
    parser.add_argument("--slot-b", required=True, type=Path)
    parser.add_argument("--output", "-o", required=True, type=Path)
    parser.add_argument("--version", "-v", required=True)
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--key-password-env")
    parser.add_argument(
        "--hardware-id",
        type=parse_uint,
        default=HARDWARE_ID_STM32F103,
    )
    parser.add_argument("--allow-downgrade", action="store_true")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    try:
        version, normalized_version = parse_version(args.version)
        if not 0 < args.hardware_id <= 0xFFFF:
            raise ValueError("hardware ID must be in 1..0xFFFF")
        key = load_private_key(
            args.private_key, password_from_env(args.key_password_env)
        )
        image_a = args.slot_a.read_bytes()
        image_b = args.slot_b.read_bytes()
        built = build_package(
            image_a,
            image_b,
            args.hardware_id,
            version,
            FLAG_ALLOW_DOWNGRADE if args.allow_downgrade else 0,
            key,
        )
        write_atomic(args.output, built.data, args.force)
    except (OSError, TypeError, ValueError, argparse.ArgumentTypeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"created: {args.output}")
    print(f"hardware: 0x{args.hardware_id:04X}")
    print(f"version: {normalized_version}")
    print(f"key_id: 0x{built.key_id:08X}")
    print(
        f"slot_a: {len(image_a)} bytes "
        f"blocks={len(built.block_hashes_a) // 32} "
        f"crc32=0x{built.image_a_crc32:08X} "
        f"sha256={built.image_a_sha256.hex()}"
    )
    print(
        f"slot_b: {len(image_b)} bytes "
        f"blocks={len(built.block_hashes_b) // 32} "
        f"crc32=0x{built.image_b_crc32:08X} "
        f"sha256={built.image_b_sha256.hex()}"
    )
    print("signature: ECDSA-P256 raw r||s")
    print(f"allow_downgrade: {args.allow_downgrade}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
