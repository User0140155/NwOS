#!/usr/bin/env bash

# ============================================================
# NwC compiler build script (step by step variant)
#
# Expected layout (run from the repository root):
#   compiler/
#     compilerMain.c
#     link.c
#     lexer/lexer.c
#     parser/parser.c
#     codegen/codegen.c
#     preproc/preproc.c
#     diag/diag.c
#     set/setfile.c
#     builded/            <- object files land here
#     builded/nwc         <- compiler
#     builded/link        <- static .set linker
# ============================================================

cd "$(dirname "$0")/.." || exit 1

mkdir -p compiler/builded

error_exit() {
    echo
    echo "================================"
    echo "       BUILD FAILED"
    echo "================================"
    echo

    read -rp "Press Enter to continue..." _
    exit 1
}

echo "================================"
echo "       NwC Compiler Build"
echo "================================"
echo

echo "[1/7] Compiling lexer..."
clang -std=c11 -O2 -Wall -Wextra -c compiler/lexer/lexer.c \
    -o compiler/builded/lexer.o
if [ $? -ne 0 ]; then error_exit; fi

echo "[2/7] Compiling parser..."
clang -std=c11 -O2 -Wall -Wextra -c compiler/parser/parser.c \
    -o compiler/builded/parser.o
if [ $? -ne 0 ]; then error_exit; fi

echo "[3/7] Compiling codegen..."
clang -std=c11 -O2 -Wall -Wextra -c compiler/codegen/codegen.c \
    -o compiler/builded/codegen.o
if [ $? -ne 0 ]; then error_exit; fi

echo "[4/7] Compiling preprocessor..."
clang -std=c11 -O2 -Wall -Wextra -c compiler/preproc/preproc.c \
    -o compiler/builded/preproc.o
if [ $? -ne 0 ]; then error_exit; fi

echo "[5/7] Compiling diagnostics..."
clang -std=c11 -O2 -Wall -Wextra -c compiler/diag/diag.c \
    -o compiler/builded/diag.o
if [ $? -ne 0 ]; then error_exit; fi

echo "[6/7] Compiling binary set reader/writer..."
clang -std=c11 -O2 -Wall -Wextra -c compiler/set/setfile.c \
    -o compiler/builded/setfile.o
if [ $? -ne 0 ]; then error_exit; fi

OBJECTS="compiler/builded/lexer.o compiler/builded/parser.o \
compiler/builded/codegen.o compiler/builded/preproc.o \
compiler/builded/diag.o compiler/builded/setfile.o"

echo "[7/7] Linking nwc and link..."
clang -std=c11 -O2 -Wall -Wextra compiler/compilerMain.c $OBJECTS \
    -o compiler/builded/nwc
if [ $? -ne 0 ]; then error_exit; fi

clang -std=c11 -O2 -Wall -Wextra compiler/link.c $OBJECTS \
    -o compiler/builded/link
if [ $? -ne 0 ]; then error_exit; fi

echo
echo "================================"
echo "       BUILD SUCCESS"
echo "================================"
echo
echo "Compiler: compiler/builded/nwc"
echo "Linker  : compiler/builded/link"
echo

read -rp "Press Enter to continue..." _
exit 0
