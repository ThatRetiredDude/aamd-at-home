#!/usr/bin/env python3
"""Write a tiny 512x512 RGBA PNG for sce_sys/icon0.png (no PIL)."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path


def chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def main() -> None:
    w = h = 512
    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            # Dark teal with a brighter square in the middle.
            if 160 <= x < 352 and 160 <= y < 352:
                row += bytes((32, 200, 180, 255))
            else:
                row += bytes((12, 28, 36, 255))
        rows.append(b"\x00" + bytes(row))
    raw = b"".join(rows)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    out = Path(__file__).resolve().parents[1] / "probe/title/sce_sys/icon0.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(png)
    print(f"wrote {out} ({len(png)} bytes)")


if __name__ == "__main__":
    main()
