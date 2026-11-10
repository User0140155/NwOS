#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"
mkdir -p builded

CC="${CC:-clang}"
CFLAGS="${CFLAGS:--std=c11 -O2 -Wall -Wextra}"

"$CC" $CFLAGS -I. nwasm.c -o builded/nwasm
"$CC" $CFLAGS -I. link.c -o builded/link

echo "NwAsm build successful."
echo "Assembler: $PWD/builded/nwasm"
echo "Linker   : $PWD/builded/link"
