#!/usr/bin/env python3
"""Send valid and malformed gateway frames to a running T113 receiver."""

import argparse
import socket
import struct
import time
import zlib

MAGIC = 0x55AA
VERSION = 1
HEADER = struct.Struct("!HBBIIII")


def frame(payload: bytes, seq: int = 1, *, magic: int = MAGIC,
          version: int = VERSION, length: int | None = None,
          crc: int | None = None) -> bytes:
    actual_len = len(payload) if length is None else length
    actual_crc = zlib.crc32(payload) & 0xFFFFFFFF if crc is None else crc
    return HEADER.pack(magic, version, 1, seq, int(time.time()), actual_len, actual_crc) + payload


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--mode", choices=("good", "bad-crc", "bad-magic", "oversize", "fragmented", "coalesced"), default="good")
    args = parser.parse_args()
    payload = b'{"gateway":{"connected":true},"test":true}'

    if args.mode == "bad-crc":
        packets = [frame(payload, crc=0)]
    elif args.mode == "bad-magic":
        packets = [frame(payload, magic=0x1234)]
    elif args.mode == "oversize":
        packets = [frame(b"", length=4097)]
    elif args.mode == "coalesced":
        packets = [frame(payload, 1) + frame(payload, 2)]
    else:
        packets = [frame(payload)]

    with socket.create_connection((args.host, args.port), timeout=5) as sock:
        if args.mode == "fragmented":
            data = packets[0]
            for offset in range(0, len(data), 3):
                sock.sendall(data[offset:offset + 3])
                time.sleep(0.02)
        else:
            for packet in packets:
                sock.sendall(packet)
    print(f"sent mode={args.mode} to {args.host}:{args.port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
