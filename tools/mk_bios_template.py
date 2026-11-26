#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import struct
import sys

SECTOR_SIZE = 512
MBR_PARTITION_TABLE_OFFSET = 446
MBR_SIGNATURE_OFFSET = 510
EFI_PARTITION_TYPE = 0xEF


def main() -> int:
    if len(sys.argv) != 5:
        print(f"usage: {sys.argv[0]} <esp.img> <output.img> <start-lba> <sectors>", file=sys.stderr)
        return 2

    esp_path = pathlib.Path(sys.argv[1])
    output_path = pathlib.Path(sys.argv[2])
    start_lba = int(sys.argv[3], 0)
    sectors = int(sys.argv[4], 0)

    if start_lba < 2048 or start_lba > 0xFFFFFFFF:
        raise SystemExit("start-lba must be >= 2048 and fit in an MBR entry")
    if sectors <= 0 or sectors > 0xFFFFFFFF:
        raise SystemExit("sectors must be positive and fit in an MBR entry")

    esp = esp_path.read_bytes()
    expected_size = sectors * SECTOR_SIZE
    if len(esp) != expected_size:
        raise SystemExit(f"ESP size mismatch: expected {expected_size} bytes, got {len(esp)}")

    total_sectors = start_lba + sectors
    if total_sectors > 0xFFFFFFFF:
        raise SystemExit("template image exceeds MBR LBA range")
    image = bytearray(total_sectors * SECTOR_SIZE)

    entry = bytearray(16)
    entry[0] = 0x80
    entry[1:4] = b"\xfe\xff\xff"
    entry[4] = EFI_PARTITION_TYPE
    entry[5:8] = b"\xfe\xff\xff"
    struct.pack_into("<II", entry, 8, start_lba, sectors)

    mbr = memoryview(image)[:SECTOR_SIZE]
    mbr[MBR_PARTITION_TABLE_OFFSET:MBR_PARTITION_TABLE_OFFSET + 16] = entry
    mbr[MBR_SIGNATURE_OFFSET:MBR_SIGNATURE_OFFSET + 2] = b"\x55\xaa"
    image[start_lba * SECTOR_SIZE:(start_lba + sectors) * SECTOR_SIZE] = esp

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
