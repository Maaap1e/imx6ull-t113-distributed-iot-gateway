#!/usr/bin/env python3
"""Generate an offline P-256 OTA key and matching Bootloader C source."""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys
from pathlib import Path

try:
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import ec
except ImportError as exc:
    raise SystemExit(
        "cryptography is required: python3 -m pip install cryptography"
    ) from exc


def format_bytes(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 8):
        chunk = ", ".join(
            f"0x{value:02X}u" for value in data[offset:offset + 8]
        )
        lines.append(f"    {chunk}")
    return ",\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--public-c", required=True, type=Path)
    parser.add_argument(
        "--password-env",
        help="environment variable holding the encryption password",
    )
    parser.add_argument(
        "--development-unencrypted",
        action="store_true",
        help="allow a plaintext private key for a lab-only setup",
    )
    parser.add_argument(
        "--replace-placeholder",
        action="store_true",
        help="replace only the repository's invalid public-key placeholder",
    )
    args = parser.parse_args()

    if args.private_key.exists():
        print("error: refusing to overwrite an existing private key", file=sys.stderr)
        return 1
    if args.public_c.exists():
        existing = args.public_c.read_text(encoding="utf-8")
        if (
            not args.replace_placeholder
            or "Deliberately invalid placeholder" not in existing
        ):
            print(
                "error: refusing to overwrite public C source unless it is "
                "the invalid placeholder and --replace-placeholder is used",
                file=sys.stderr,
            )
            return 1

    password = None
    if args.password_env:
        password_text = os.environ.get(args.password_env)
        if not password_text:
            print(f"error: {args.password_env} is unset or empty", file=sys.stderr)
            return 1
        password = password_text.encode("utf-8")
    elif not args.development_unencrypted:
        print(
            "error: use --password-env for a protected key, or explicitly "
            "--development-unencrypted for a lab key",
            file=sys.stderr,
        )
        return 1

    private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())
    encryption = (
        serialization.BestAvailableEncryption(password)
        if password is not None
        else serialization.NoEncryption()
    )
    private_pem = private_key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        encryption,
    )
    numbers = private_key.public_key().public_numbers()
    public = numbers.x.to_bytes(32, "big") + numbers.y.to_bytes(32, "big")
    key_id = struct.unpack("<I", hashlib.sha256(public).digest()[:4])[0]

    public_source = f"""#include "ota_trusted_key.h"

/* Generated public verification key. The private key must remain offline. */
const uint32_t g_ota_trusted_key_id = 0x{key_id:08X}u;
const uint8_t g_ota_trusted_public_key[OTA_TRUSTED_PUBLIC_KEY_SIZE] = {{
{format_bytes(public)}
}};
"""

    args.private_key.parent.mkdir(parents=True, exist_ok=True)
    args.public_c.parent.mkdir(parents=True, exist_ok=True)
    args.private_key.write_bytes(private_pem)
    with args.public_c.open("w", encoding="utf-8", newline="\n") as public_file:
        public_file.write(public_source)
    print(f"private key: {args.private_key}")
    print(f"bootloader public key: {args.public_c}")
    print(f"key_id: 0x{key_id:08X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
