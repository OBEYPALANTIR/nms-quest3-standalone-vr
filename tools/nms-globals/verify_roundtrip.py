#!/usr/bin/env python3
"""Verify a format-3300 stock roundtrip and restore six shipped header bytes."""

import argparse
from pathlib import Path


HEADER_SIZE = 0x20
FORMAT_3300 = bytes.fromhex("cccccccccccccccce40c")
METADATA_OFFSETS = frozenset({0x0A, *range(0x18, 0x1D)})


def preserve_metadata(shipped, stock):
    if len(shipped) != len(stock) or len(shipped) < HEADER_SIZE:
        raise ValueError("MBIN lengths must agree and include the header")
    if not shipped.startswith(FORMAT_3300) or not stock.startswith(FORMAT_3300):
        raise ValueError("expected ordinary format-3300 MBIN files")
    if shipped[0x0A:0x0C] != b"\0\0" or stock[0x0A:0x0C] != b"\x04\0":
        raise ValueError("expected a shipped marker of 0 and a compiler marker of 4")
    if shipped[HEADER_SIZE:] != stock[HEADER_SIZE:]:
        raise ValueError("stock payload differs from shipped payload")
    differences = {
        offset for offset in range(HEADER_SIZE) if shipped[offset] != stock[offset]
    }
    if differences - METADATA_OFFSETS:
        raise ValueError("stock header differs outside the six approved metadata positions")

    restored = bytearray(stock)
    for offset in METADATA_OFFSETS:
        restored[offset] = shipped[offset]
    return bytes(restored)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shipped", type=Path)
    parser.add_argument("stock", type=Path)
    parser.add_argument("output", type=Path, help="new nonstock verification file")
    args = parser.parse_args()
    try:
        restored = preserve_metadata(args.shipped.read_bytes(), args.stock.read_bytes())
        with args.output.open("xb") as output:
            output.write(restored)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
