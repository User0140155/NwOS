#!/usr/bin/env bash


# Unusable now,
# Run 'make' for build


set -Eeuo pipefail

on_error() {
    local status=$?
    printf '\n[BUILD ERROR] command failed (exit %s) at line %s:\n  %s\n' \
        "$status" "${BASH_LINENO[0]:-unknown}" "${BASH_COMMAND:-unknown}" >&2
    exit "$status"
}
trap on_error ERR

fail() {
    printf '\n[BUILD FAILED] %s\n' "$*" >&2
    exit 1
}

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
ROOT="$(pwd -P)"
BUILD="$ROOT/build"
ISO_ROOT="$BUILD/iso-root"
ISO_OUT="$ROOT/NwOS.iso"

TARGET="x86_64-unknown-none-elf"

BOOT_WIDTH=640
BOOT_HEIGHT=480
GUEST_WIDTH=1280
GUEST_HEIGHT=720
GUI_WIDTH=1280
GUI_HEIGHT=720

LIMINE_VERSION="12.9.6"
LIMINE_URL="https://github.com/Limine-Bootloader/Limine/releases/download/v${LIMINE_VERSION}/limine-binary.tar.gz"
LIMINE_SHA256="b38aabb042b0c0f9f562b7741be3c1204838d7bbacfa78bb411c674bd5b984ee"
LIMINE_ARCHIVE="$BUILD/limine-binary-v${LIMINE_VERSION}.tar.gz"
LIMINE_DIR="$BUILD/limine-v${LIMINE_VERSION}"


CFLAGS=(
    "--target=$TARGET"
    -m64
    -mno-red-zone
    -mcmodel=kernel
    -ffreestanding
    -fno-stack-protector
    -fno-pic
    -fno-pie
    -fno-builtin
    -nostdlib
    -fno-asynchronous-unwind-tables
    -ffunction-sections
    -fdata-sections
    -Os
    -I drivers
    -I sys/net
    -I sys/kernel/network
    -I "$BUILD"
)

# Keep this list in sync with the source files in the NwOS repository.
SOURCES=(
    sys/kernel/kernel.c
    sys/kernel/nwo_loader.c
    sys/kernel/panic/kernelpanic.c
    sys/kernel/nwoloader/nwo_vm.c
    sys/kernel/nwoloader/nwo_runner.c
    sys/kernel/Apps/Games/slot.c
    sys/kernel/Apps/Games/game_memory.c
    sys/kernel/Apps/fileStats/filestats.c
    sys/kernel/Apps/baseconverter/baseconverter.c
    sys/kernel/Apps/wordgenerator/wordgenerator.c
    sys/kernel/Apps/textredactor/textredactor.c
    drivers/keyboard.c
    drivers/mouse.c
    drivers/disk.c
    drivers/ahci.c
    drivers/usb/xhci.c
    sys/kernel/memory/pmm.c
    sys/kernel/memory/mmu.c
    sys/kernel/network/e1000.c
    sys/kernel/network/pci.c
    sys/kernel/network/timer.c
    sys/kernel/network/net.c
    sys/kernel/fb.c
    sys/kernel/Apps/Games/snake.c
    sys/kernel/apps_list.c
    sys/kernel/dead_boot.c
    sys/kernel/Apps/Games/rps.c
    sys/kernel/Apps/Games/game_coin.c
    sys/kernel/utf8.c
    compiler/lexer/lexer.c
    compiler/parser/parser.c
    compiler/codegen/codegen.c
    compiler/nwc_runtime.c
    compiler/nwc_stdio.c
    compiler/nwc_kernel.c
    sys/kernel/package.c
)

# Optional driver: compile it when this checkout contains the local addition.
if [[ -f drivers/touchpad.c ]]; then
    SOURCES+=(drivers/touchpad.c)
else
    printf 'NOTE: drivers/touchpad.c not found; building without the optional touchpad driver.\n'
fi

mkdir -p "$BUILD" "$ROOT/img"

need_tools=(clang ld.lld nasm xorriso python3 curl tar sha256sum make file)
for tool in "${need_tools[@]}"; do
    command -v "$tool" >/dev/null 2>&1 || fail "Missing tool '$tool'. Install build dependencies, e.g. sudo apt install clang lld nasm xorriso python3 curl tar coreutils make file."
done

printf '=== Building NwOS for x86_64 ===\n'
printf 'Project: %s\n' "$ROOT"
printf 'Limine:  v%s\n' "$LIMINE_VERSION"

# Generate build-time video settings.
cat > "$BUILD/video_config.h" <<EOF2
#ifndef NWOS_VIDEO_CONFIG_H
#define NWOS_VIDEO_CONFIG_H
#define NWOS_BOOT_WIDTH $BOOT_WIDTH
#define NWOS_BOOT_HEIGHT $BOOT_HEIGHT
#define NWOS_GUI_WIDTH $GUI_WIDTH
#define NWOS_GUI_HEIGHT $GUI_HEIGHT
#endif
EOF2

[[ -f tools/gen_wallpaper.py ]] || fail "tools/gen_wallpaper.py is missing"
[[ -f tools/gen_packages.py ]] || fail "tools/gen_packages.py is missing"
python3 tools/gen_wallpaper.py
python3 tools/gen_packages.py

# Download and verify a single official Limine binary release.
if [[ ! -f "$LIMINE_ARCHIVE" ]] || \
   ! printf '%s  %s\n' "$LIMINE_SHA256" "$LIMINE_ARCHIVE" | sha256sum --check --status; then
    rm -f "$LIMINE_ARCHIVE"
    printf '\nDownloading Limine v%s...\n' "$LIMINE_VERSION"
    curl --fail --location --retry 3 --output "$LIMINE_ARCHIVE" "$LIMINE_URL"
fi
printf '%s  %s\n' "$LIMINE_SHA256" "$LIMINE_ARCHIVE" | sha256sum --check -

rm -rf "$LIMINE_DIR"
mkdir -p "$LIMINE_DIR"
tar -xzf "$LIMINE_ARCHIVE" -C "$LIMINE_DIR"

find_asset() {
    local wanted="$1"
    find "$LIMINE_DIR" -type f -iname "$wanted" -print -quit
}

LIMINE_BIOS_SYS="$(find_asset limine-bios.sys)"
LIMINE_BIOS_CD="$(find_asset limine-bios-cd.bin)"
LIMINE_UEFI_CD="$(find_asset limine-uefi-cd.bin)"
LIMINE_EFI64="$(find_asset BOOTX64.EFI)"

for spec in \
    "limine-bios.sys:$LIMINE_BIOS_SYS" \
    "limine-bios-cd.bin:$LIMINE_BIOS_CD" \
    "limine-uefi-cd.bin:$LIMINE_UEFI_CD" \
    "BOOTX64.EFI:$LIMINE_EFI64"; do
    name="${spec%%:*}"
    path="${spec#*:}"
    [[ -n "$path" && -f "$path" ]] || fail "Limine v${LIMINE_VERSION}: missing $name"
done

# Build the host-side `limine` utility from the binary release package.
# The archive may have a top-level subdirectory; find its Makefile recursively.
LIMINE_TOOL="$(find "$LIMINE_DIR" -type f -name limine -perm /111 -print -quit)"
if [[ -z "$LIMINE_TOOL" ]]; then
    LIMINE_MAKEFILE="$(find "$LIMINE_DIR" -type f \
        \( -name Makefile -o -name makefile -o -name GNUmakefile \) \
        -print -quit)"
    [[ -n "$LIMINE_MAKEFILE" ]] || {
        printf '\nERROR: no Makefile found in extracted Limine binary package.\n' >&2
        printf 'Archive files (first 100):\n' >&2
        find "$LIMINE_DIR" -maxdepth 5 -type f -printf '%P\n' | sort | head -100 >&2 || true
        fail "Cannot build the Limine host utility; inspect the archive layout above"
    }

    LIMINE_MAKE_DIR="$(dirname "$LIMINE_MAKEFILE")"
    printf '\nBuilding Limine host utility in: %s\n' "$LIMINE_MAKE_DIR"
    make -C "$LIMINE_MAKE_DIR" CC=clang
    LIMINE_TOOL="$(find "$LIMINE_DIR" -type f -name limine -perm /111 -print -quit)"
fi
[[ -n "$LIMINE_TOOL" && -x "$LIMINE_TOOL" ]] || fail "Could not find/build the Limine host utility (limine)"

# Assemble the project's auxiliary boot sector and kernel entry.
nasm -f bin sys/boot/boot.asm -o "$BUILD/boot.bin"
nasm -f elf64 sys/kernel/kernel_entry.asm -o "$BUILD/kernel_entry.o"

OBJS=("$BUILD/kernel_entry.o")
for src in "${SOURCES[@]}"; do
    [[ -f "$src" ]] || fail "Source file is missing: $src"
    obj_name="${src//\//_}"
    obj_name="${obj_name//./_}"
    obj="$BUILD/$obj_name.o"

    if [[ "$src" == compiler/*.c ]]; then
        clang "${CFLAGS[@]}" -DNWC_KERNEL \
            -I "$ROOT/compiler/kernel_include" \
            -I "$ROOT/compiler" \
            -c "$src" -o "$obj"
    else
        clang "${CFLAGS[@]}" -c "$src" -o "$obj"
    fi
    OBJS+=("$obj")
done

ld.lld -m elf_x86_64 -nostdlib -z noexecstack \
    --gc-sections -T "$ROOT/linker.ld" \
    "${OBJS[@]}" -o "$BUILD/kernel.elf"

[[ -s "$BUILD/kernel.elf" ]] || fail "Kernel was not created"
file "$BUILD/kernel.elf"

# Verify the protocol header when grub-file is installed.
if command -v grub-file >/dev/null 2>&1; then
    grub-file --is-x86-multiboot2 "$BUILD/kernel.elf" || \
        fail "kernel.elf does not contain a valid Multiboot2 header"
    printf 'Multiboot2 header: OK\n'
else
    printf 'NOTE: grub-file not installed; skipped Multiboot2 header check.\n'
fi

# Assemble the ISO tree. Do not touch the repository's ./iso directory.
rm -rf "$ISO_ROOT"
mkdir -p "$ISO_ROOT/boot/limine" "$ISO_ROOT/EFI/BOOT"

install -m 0644 "$BUILD/kernel.elf" "$ISO_ROOT/boot/kernel.elf"
install -m 0644 "$LIMINE_BIOS_SYS" "$ISO_ROOT/boot/limine/limine-bios.sys"
install -m 0644 "$LIMINE_BIOS_CD" "$ISO_ROOT/boot/limine/limine-bios-cd.bin"
install -m 0644 "$LIMINE_UEFI_CD" "$ISO_ROOT/boot/limine/limine-uefi-cd.bin"
install -m 0644 "$LIMINE_EFI64" "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"

cat > "$ISO_ROOT/boot/limine/limine.conf" <<EOF2
timeout: 3

/NwOS
    protocol: multiboot2
    path: boot():/boot/kernel.elf
    resolution: ${GUEST_WIDTH}x${GUEST_HEIGHT}x32
EOF2

# Official Limine hybrid ISO layout (BIOS El Torito + UEFI El Torito/ESP).
# Create a BIOS/UEFI El Torito ISO for Ventoy.
# Ventoy presents the ISO as a virtual CD-ROM.
rm -f "$ISO_OUT"

xorriso -as mkisofs \
    -R -r -J \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -hfsplus \
    -apm-block-size 2048 \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    "$ISO_ROOT" \
    -o "$ISO_OUT"

[[ -s "$ISO_OUT" ]] || fail "ISO was not created"

"$LIMINE_TOOL" bios-install "$ISO_OUT"
# Limine's documented hybrid-ISO post-processing step (BIOS boot code).
# This edits the ISO file only; it does not write to any physical disk.

printf '\n=== ISO verification ===\n'
printf '\nEFI executable:\n'
file "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"
printf '\nISO boot catalog (must include an EFI/0xEF entry):\n'
xorriso -indev "$ISO_OUT" -report_el_torito plain
printf '\nISO file:\n'
ls -lh "$ISO_OUT"
sha256sum "$ISO_OUT"

printf '\n============================\n'
printf '       BUILD SUCCESS\n'
printf '============================\n'
printf 'ISO: %s\n' "$ISO_OUT"
printf 'ISO tree: %s\n' "$ISO_ROOT"
printf 'Limine: v%s\n' "$LIMINE_VERSION"

