#!/usr/bin/env bash

# ============================================================
# NwC host compiler build script
#
# Expected layout:
#   compiler/
#     compilerMain.c
#     lexer/lexer.c
#     lexer/lexer.h
#     parser/parser.c
#     parser/parser.h
#     codegen/codegen.c
#     codegen/codegen.h
#     preproc/preproc.c
#     preproc/preproc.h
#     diag/diag.c
#     diag/diag.h
#     set/setfile.c
#     set/setfile.h
#     link.c               <- generated as builded/link
#     nwc_kernel.c
#     nwc_kernel.h
#     nwc_runtime.c
#     nwc_runtime.h
#     builded/
#       nwc                <- generated here
#
# NOTE:
#   nwc_kernel.* and nwc_runtime.* are kernel-side sources.
#   They are NOT linked into the NwC executable.
# ============================================================

cd "$(dirname "$0")" || exit 1

ROOT="$PWD"
OUT="$ROOT/builded"
NWC_OUT="$OUT/nwc"
LINK_OUT="$OUT/link"

COMMON_SOURCES="
    $ROOT/lexer/lexer.c
    $ROOT/parser/parser.c
    $ROOT/codegen/codegen.c
    $ROOT/preproc/preproc.c
    $ROOT/diag/diag.c
    $ROOT/set/setfile.c
"

mkdir -p "$OUT"

if ! command -v clang >/dev/null 2>&1; then
    echo "[NwC] ERROR: clang was not found in PATH."
    echo "[NwC] Add LLVM/Clang to PATH and run this again."
    exit 1
fi

if ! command -v lld-link >/dev/null 2>&1; then
    if ! command -v ld.lld >/dev/null 2>&1; then
        echo "[NwC] ERROR: LLVM LLD was not found."
        echo "[NwC] Expected lld-link or ld.lld in PATH."
        exit 1
    fi
fi

echo
echo "========================================"
echo "          Building NwC compiler"
echo "========================================"
echo

echo "[1/3] Compiling and linking nwc..."

clang -std=c11 -O2 -Wall -Wextra -I"$ROOT" \
    "$ROOT/compilerMain.c" $COMMON_SOURCES \
    -fuse-ld=lld \
    -o "$NWC_OUT"

if [ $? -ne 0 ]; then
    echo
    echo "[NwC] BUILD FAILED."
    exit 1
fi

echo "[2/3] Compiling and linking link..."

clang -std=c11 -O2 -Wall -Wextra -I"$ROOT" \
    "$ROOT/link.c" $COMMON_SOURCES \
    -fuse-ld=lld \
    -o "$LINK_OUT"

if [ $? -ne 0 ]; then
    echo
    echo "[NwC] LINK TOOL BUILD FAILED."
    exit 1
fi

echo
echo "[3/3] Checking output..."

for tool in "$NWC_OUT" "$LINK_OUT"; do
    if [ ! -f "$tool" ]; then
        echo "[NwC] ERROR: $tool was not created."
        exit 1
    fi
done

NWC_SIZE=$(stat -c%s "$NWC_OUT" 2>/dev/null || wc -c < "$NWC_OUT")
LINK_SIZE=$(stat -c%s "$LINK_OUT" 2>/dev/null || wc -c < "$LINK_OUT")

echo
echo "========================================"
echo "NwC build successful."
echo "Compiler: $NWC_OUT ($NWC_SIZE bytes)"
echo "Linker  : $LINK_OUT ($LINK_SIZE bytes)"
echo "========================================"
echo

exit 0
