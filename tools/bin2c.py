#!/usr/bin/env python3
import sys

if len(sys.argv) != 3:
    print("usage: bin2c.py input.bin output.h", file=sys.stderr)
    sys.exit(1)

with open(sys.argv[1], "rb") as src:
    data = src.read()

with open(sys.argv[2], "w", encoding="utf-8") as dst:
    dst.write("static const unsigned char user_shell_blob[] = {\n")
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        dst.write("    " + ",".join(str(byte) for byte in chunk) + ",\n")
    dst.write("};\n")
    dst.write("static const unsigned int user_shell_blob_len = %d;\n" % len(data))
