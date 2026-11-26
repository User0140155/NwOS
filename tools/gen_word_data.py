#!/usr/bin/env python3
"""Build NwOS's compact NwWord dictionary module.

Input:  UTF-8 word list, one word per line (SCOWL/ESDB en_US wordlist).
Output: little-endian NWWD v1 binary file.

NWWD v1 layout:
    Header (28 bytes):
        char[4] magic = "NWWD"
        u16     version = 1
        u16     header_size = 28
        u32     word_count
        u32     index_stride
        u32     index_count
        u32     data_size
        u32     data_crc32
    Index:
        u32[index_count] byte offsets into the data section
        Entry i is indexed at i * index_stride.
    Data:
        UTF-8 words, each followed by a NUL byte.

The sparse index keeps the file small. A kernel reader finds the nearest
indexed word, then scans at most index_stride entries to reach the target.
All offsets are relative to the beginning of the data section.
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import tempfile
import unicodedata
import zlib
from pathlib import Path
from typing import Iterable

MAGIC = b"NWWD"
VERSION = 1
HEADER = struct.Struct("<4sHHIIIII")
DEFAULT_INDEX_STRIDE = 256
DEFAULT_MIN_WORDS = 10_000
DEFAULT_MAX_WORD_BYTES = 255
APOSTROPHES = {"'", "\u2019"}
INTERNAL_PUNCTUATION = APOSTROPHES | {"-"}


def normalize_word(raw: str, max_word_bytes: int) -> str | None:
    """Return a clean NFC word, or None when the line is not a word."""
    word = unicodedata.normalize("NFC", raw.strip()).lower()
    if not word or len(word.encode("utf-8")) > max_word_bytes:
        return None

    # Punctuation is accepted only inside a word, between letters.
    if not word[0].isalpha() or not word[-1].isalpha():
        return None

    for pos, char in enumerate(word):
        if char.isalpha():
            continue
        if (char in INTERNAL_PUNCTUATION and pos > 0 and
                pos + 1 < len(word) and
                word[pos - 1].isalpha() and word[pos + 1].isalpha()):
            continue
        return None

    return word


def load_exclusions(path: Path | None, max_word_bytes: int) -> set[str]:
    """Load optional exact-word exclusions; blank lines and # comments are ignored."""
    if path is None:
        return set()

    excluded: set[str] = set()
    with path.open("r", encoding="utf-8-sig", newline=None) as source:
        for line in source:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            word = normalize_word(text, max_word_bytes)
            if word is not None:
                excluded.add(word.casefold())
    return excluded


def read_words(
    source_path: Path,
    excluded: set[str],
    max_word_bytes: int,
) -> tuple[list[str], dict[str, int], int | None]:
    """Read, validate, exclude and de-duplicate a one-word-per-line list."""
    words_by_key: dict[str, str] = {}
    stats = {
        "lines": 0, "source_entries": 0, "empty": 0,
        "invalid": 0, "excluded": 0, "duplicates": 0,
    }
    declared_count: int | None = None

    with source_path.open("r", encoding="utf-8-sig", newline=None) as source:
        for line in source:
            stats["lines"] += 1
            raw = line.strip()

            if not raw or raw.startswith("#"):
                stats["empty"] += 1
                continue

            # Accept the optional first-line entry count used by some .dic files.
            if declared_count is None and not words_by_key and raw.isdecimal():
                declared_count = int(raw)
                continue

            stats["source_entries"] += 1
            word = normalize_word(raw, max_word_bytes)
            if word is None:
                stats["invalid"] += 1
                continue

            key = word.casefold()
            if key in excluded:
                stats["excluded"] += 1
                continue
            if key in words_by_key:
                stats["duplicates"] += 1
                continue

            words_by_key[key] = word

    words = sorted(words_by_key.values(), key=lambda item: (item.casefold(), item))
    return words, stats, declared_count


def make_binary(words: list[str], index_stride: int) -> bytes:
    """Serialize words into the documented NWWD v1 format."""
    payload = bytearray()
    offsets: list[int] = []

    for index, word in enumerate(words):
        if index % index_stride == 0:
            offsets.append(len(payload))
        payload.extend(word.encode("utf-8"))
        payload.append(0)

    index_data = struct.pack(f"<{len(offsets)}I", *offsets) if offsets else b""
    header = HEADER.pack(
        MAGIC,
        VERSION,
        HEADER.size,
        len(words),
        index_stride,
        len(offsets),
        len(payload),
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    return header + index_data + payload


def write_atomic(destination: Path, content: bytes) -> None:
    """Write output atomically so an interrupted build cannot leave a partial file."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    temp_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=destination.parent,
            prefix=destination.name + ".", suffix=".tmp", delete=False,
        ) as temporary:
            temp_name = temporary.name
            temporary.write(content)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.replace(temp_name, destination)
        temp_name = None
    finally:
        if temp_name is not None:
            try:
                os.unlink(temp_name)
            except FileNotFoundError:
                pass


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert an ESDB/SCOWL word list into an NwOS NWWD module."
    )
    parser.add_argument("--input", required=True, type=Path, help="UTF-8 source word list")
    parser.add_argument("--output", required=True, type=Path, help="Output .bin file")
    parser.add_argument("--exclude", type=Path, help="Optional file with one excluded word per line")
    parser.add_argument(
        "--index-stride", type=int, default=DEFAULT_INDEX_STRIDE,
        help=f"Words per sparse-index entry (default: {DEFAULT_INDEX_STRIDE})",
    )
    parser.add_argument(
        "--min-words", type=int, default=DEFAULT_MIN_WORDS,
        help=f"Refuse to write a smaller dictionary (default: {DEFAULT_MIN_WORDS})",
    )
    parser.add_argument(
        "--max-word-bytes", type=int, default=DEFAULT_MAX_WORD_BYTES,
        help=f"Maximum UTF-8 bytes per word (default: {DEFAULT_MAX_WORD_BYTES})",
    )
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)

    if not args.input.is_file():
        print(f"ERROR: input file not found: {args.input}", file=sys.stderr)
        return 2
    if args.exclude is not None and not args.exclude.is_file():
        print(f"ERROR: exclusion file not found: {args.exclude}", file=sys.stderr)
        return 2
    if args.index_stride < 1 or args.min_words < 1 or args.max_word_bytes < 1:
        print("ERROR: index stride, minimum words and maximum word size must be positive.", file=sys.stderr)
        return 2

    try:
        excluded = load_exclusions(args.exclude, args.max_word_bytes)
        words, stats, declared_count = read_words(
            args.input, excluded, args.max_word_bytes
        )
        if len(words) < args.min_words:
            print(
                f"ERROR: only {len(words):,} valid words remain; "
                f"minimum is {args.min_words:,}. No output written.",
                file=sys.stderr,
            )
            return 1

        binary = make_binary(words, args.index_stride)
        write_atomic(args.output, binary)

    except (OSError, UnicodeError, struct.error, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    index_count = (len(words) + args.index_stride - 1) // args.index_stride
    data_size = len(binary) - HEADER.size - index_count * 4
    print("NwWord dictionary generated successfully.")
    print(f"  Words:       {len(words):,}")
    print(f"  Excluded:    {stats['excluded']:,}")
    print(f"  Duplicates:  {stats['duplicates']:,}")
    print(f"  Invalid:     {stats['invalid']:,}")
    print(f"  Empty:       {stats['empty']:,}")
    print(f"  Index:       {index_count:,} entries, stride {args.index_stride}")
    print(f"  Data:        {data_size:,} bytes")
    print(f"  Output:      {args.output} ({len(binary):,} bytes)")
    if declared_count is not None and declared_count != stats["source_entries"]:
        print(
            f"  Note: source count header says {declared_count:,}; "
            f"found {stats['source_entries']:,} non-empty entries after it."
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())