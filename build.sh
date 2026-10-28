
#!/usr/bin/env bash

set -u

fail() {
    echo
    echo "============================"
    echo "       BUILDING FAILED"
    echo "============================"
    read -rp "Press Enter to continue..." || true
    exit 1
}

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1
ROOT="$PWD"

BUILD="$ROOT/build"
ISO_ROOT="$BUILD/iso-root"
LIMINE_DIR="$ROOT/tools/limine"

TARGET="x86_64-unknown-none-elf"

BOOT_WIDTH=680
BOOT_HEIGHT=480
GUI_WIDTH=1280
GUI_HEIGHT=720

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
)

mkdir -p "$BUILD" "$ROOT/img"

STEPS=$(( ${#SOURCES[@]} + 6 ))
N=0

step() {
    N=$((N + 1))
    echo
    echo "[$N/$STEPS] $1"
}

# Find Limine assets in common installation directories.
find_limine_asset() {
    local name="$1"
    local dir
    local found

    local dirs=(
        "$LIMINE_DIR"
        "$BUILD/limine-binary"
        "$ROOT/limine-binary"
        /usr/share/limine
        /usr/local/share/limine
        /usr/share/limine-binary
    )

    for dir in "${dirs[@]}"; do
        [[ -d "$dir" ]] || continue

        found="$(find "$dir" -type f -iname "$name" \
            -print -quit 2>/dev/null || true)"

        if [[ -n "$found" ]]; then
            printf '%s\n' "$found"
            return 0
        fi
    done

    return 1
}

echo "=== Building NwOS for x86-64 (Linux Mint) ==="

step "Checking build tools..."

for tool in clang ld.lld nasm xorriso python3 curl tar; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: missing tool: $tool"
        echo "Install build dependencies with:"
        echo "sudo apt install clang lld nasm xorriso python3 curl tar"
        fail
    fi
done

step "Preparing video config and wallpaper..."

cat > "$BUILD/video_config.h" <<EOF
#ifndef NWOS_VIDEO_CONFIG_H
#define NWOS_VIDEO_CONFIG_H

#define NWOS_BOOT_WIDTH $BOOT_WIDTH
#define NWOS_BOOT_HEIGHT $BOOT_HEIGHT
#define NWOS_GUI_WIDTH $GUI_WIDTH
#define NWOS_GUI_HEIGHT $GUI_HEIGHT

#endif
EOF

python3 tools/gen_wallpaper.py || fail

step "Preparing Limine bootloader..."

LIMINE_BIOS_SYS="$(find_limine_asset limine-bios.sys || true)"
LIMINE_BIOS_CD="$(find_limine_asset limine-bios-cd.bin || true)"
LIMINE_UEFI_CD="$(find_limine_asset limine-uefi-cd.bin || true)"
LIMINE_EFI64="$(find_limine_asset BOOTX64.EFI || true)"
LIMINE_EFI32="$(find_limine_asset BOOTIA32.EFI || true)"

# If assets are missing, download the official binary release.
if [[ -z "$LIMINE_BIOS_SYS" ||
      -z "$LIMINE_BIOS_CD" ||
      -z "$LIMINE_UEFI_CD" ||
      -z "$LIMINE_EFI64" ]]; then

    echo "Limine assets not found. Downloading official binary release..."

    mkdir -p "$BUILD/limine-binary"

    curl -fL --retry 2 \
        "https://github.com/Limine-Bootloader/Limine/releases/latest/download/limine-binary.tar.gz" \
        -o "$BUILD/limine-binary.tar.gz" || {
            echo "ERROR: could not download Limine."
            echo "Check your internet connection."
            fail
        }

    tar -xzf "$BUILD/limine-binary.tar.gz" \
        -C "$BUILD/limine-binary" || fail

    LIMINE_BIOS_SYS="$(find_limine_asset limine-bios.sys || true)"
    LIMINE_BIOS_CD="$(find_limine_asset limine-bios-cd.bin || true)"
    LIMINE_UEFI_CD="$(find_limine_asset limine-uefi-cd.bin || true)"
    LIMINE_EFI64="$(find_limine_asset BOOTX64.EFI || true)"
    LIMINE_EFI32="$(find_limine_asset BOOTIA32.EFI || true)"
fi

for asset in \
    "limine-bios.sys:$LIMINE_BIOS_SYS" \
    "limine-bios-cd.bin:$LIMINE_BIOS_CD" \
    "limine-uefi-cd.bin:$LIMINE_UEFI_CD" \
    "BOOTX64.EFI:$LIMINE_EFI64"; do

    name="${asset%%:*}"
    path="${asset#*:}"

    if [[ -z "$path" || ! -f "$path" ]]; then
        echo "ERROR: required Limine asset not found: $name"
        fail
    fi
done

step "Assembling bootloader..."

nasm -f bin sys/boot/boot.asm \
    -o "$BUILD/boot.bin" || fail

step "Assembling kernel entry..."

nasm -f elf64 sys/kernel/kernel_entry.asm \
    -o "$BUILD/kernel_entry.o" || fail

OBJS=("$BUILD/kernel_entry.o")

# Keep object filenames unique even when source basenames match.
for src in "${SOURCES[@]}"; do
    step "Compiling $src"

    if [[ ! -f "$src" ]]; then
        echo "ERROR: missing source file: $src"
        fail
    fi

    obj_name="${src//\//_}"
    obj_name="${obj_name//./_}"
    obj="$BUILD/$obj_name.o"

    clang "${CFLAGS[@]}" -c "$src" -o "$obj" || fail
    OBJS+=("$obj")
done

step "Linking kernel..."

ld.lld -m elf_x86_64 -nostdlib -z noexecstack \
    --gc-sections -T linker.ld \
    "${OBJS[@]}" -o "$BUILD/kernel.elf" || fail

step "Preparing ISO tree..."

# IMPORTANT: never delete the repository's existing ./iso directory.
# All generated ISO contents live under ./build/iso-root.
rm -rf "$ISO_ROOT"

mkdir -p \
    "$ISO_ROOT/boot/limine" \
    "$ISO_ROOT/EFI/BOOT"

cp "$BUILD/kernel.elf" "$ISO_ROOT/boot/kernel.elf" || fail

cp "$LIMINE_BIOS_SYS" "$ISO_ROOT/boot/limine/" || fail
cp "$LIMINE_BIOS_CD" "$ISO_ROOT/boot/limine/" || fail
cp "$LIMINE_UEFI_CD" "$ISO_ROOT/boot/limine/" || fail
cp "$LIMINE_EFI64" "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI" || fail

if [[ -n "$LIMINE_EFI32" && -f "$LIMINE_EFI32" ]]; then
    cp "$LIMINE_EFI32" "$ISO_ROOT/EFI/BOOT/BOOTIA32.EFI" || fail
fi

cat > "$ISO_ROOT/boot/limine/limine.conf" <<EOF
timeout: 0

/NwOS
    protocol: multiboot2
    path: boot():/boot/kernel.elf
    resolution: 640x400x32
EOF
# 640 = BOOT_WIDTH
# 400 = BOOT_HEIGHT
# resolution: ${BOOT_WIDTH}x${BOOT_HEIGHT}x32

step "Building bootable ISO..."

rm -f "$ROOT/NwOS.iso"

xorriso -as mkisofs \
    -R -r -J \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part \
    --efi-boot-image \
    --protective-msdos-label \
    "$ISO_ROOT" \
    -o "$ROOT/NwOS.iso" || fail

if [[ ! -s "$ROOT/NwOS.iso" ]]; then
    echo "ERROR: ISO was not created."
    fail
fi

echo
echo "============================"
echo "       BUILD SUCCESS"
echo "============================"
echo
ls -lh "$ROOT/NwOS.iso" "$BUILD/kernel.elf"
echo
echo "ISO root: $ISO_ROOT"
echo "Repository ./iso directory was not deleted."
