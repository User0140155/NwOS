# NwOS build system
# Usage: make [-j$(nproc)] [all|kernel|iso|boot-sector|clean]

SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD := $(ROOT)/build
ISO_ROOT := $(BUILD)/iso-root
ISO_OUT := $(ROOT)/NwOS.iso
ISO_TMP := $(ISO_OUT).tmp
ISO_STAMP := $(BUILD)/iso-tree.stamp

TARGET := x86_64-unknown-none-elf

BOOT_WIDTH := 640
BOOT_HEIGHT := 480
GUEST_WIDTH := 1280
GUEST_HEIGHT := 720
GUI_WIDTH := 1280
GUI_HEIGHT := 720

LIMINE_VERSION ?= 12.9.6
LIMINE_URL := https://github.com/Limine-Bootloader/Limine/releases/download/v$(LIMINE_VERSION)/limine-binary.tar.gz
LIMINE_SHA256 ?= b38aabb042b0c0f9f562b7741be3c1204838d7bbacfa78bb411c674bd5b984ee
LIMINE_ARCHIVE := $(BUILD)/limine-binary-v$(LIMINE_VERSION).tar.gz
LIMINE_DIR := $(BUILD)/limine-v$(LIMINE_VERSION)
LIMINE_STAMP := $(LIMINE_DIR)/.ready
LIMINE_TOOL := $(LIMINE_DIR)/bin/limine

CLANG ?= clang
LD_LLD ?= ld.lld
NASM ?= nasm
PYTHON ?= python3
CURL ?= curl
TAR ?= tar
SHA256SUM ?= sha256sum
XORRISO ?= xorriso
FILE ?= file

# Set CHECK_MULTIBOOT=0 only if grub-file is intentionally unavailable.
CHECK_MULTIBOOT ?= 1
REQUIRED_TOOLS := $(CLANG) $(LD_LLD) $(NASM) $(PYTHON) $(CURL) $(TAR) \
                  $(SHA256SUM) $(XORRISO) $(FILE)
ifeq ($(CHECK_MULTIBOOT),1)
REQUIRED_TOOLS += grub-file
endif

KERNEL_CFLAGS := \
    --target=$(TARGET) \
    -m64 \
    -mno-red-zone \
    -mcmodel=small \
    -ffreestanding \
    -fno-stack-protector \
    -fno-pic \
    -fno-pie \
    -fno-builtin \
    -nostdlib \
    -fno-asynchronous-unwind-tables \
    -ffunction-sections \
    -fdata-sections \
    -Os \
    -I$(ROOT)/drivers \
    -I$(ROOT)/sys/net \
    -I$(ROOT)/sys/kernel/network \
    -I$(BUILD)

# Keep this list in sync with the source files in the NwOS repository.
SOURCES_REL := \
    sys/kernel/kernel.c \
    sys/kernel/nwo_loader.c \
    sys/kernel/panic/kernelpanic.c \
    sys/kernel/nwoloader/nwo_vm.c \
    sys/kernel/nwoloader/nwo_runner.c \
    sys/kernel/Apps/Games/slot.c \
    sys/kernel/Apps/Games/game_memory.c \
    sys/kernel/Apps/fileStats/filestats.c \
    sys/kernel/Apps/baseconverter/baseconverter.c \
    sys/kernel/Apps/wordgenerator/wordgenerator.c \
    sys/kernel/Apps/textredactor/textredactor.c \
    drivers/keyboard.c \
    drivers/mouse.c \
    drivers/disk.c \
    drivers/ahci.c \
    sys/kernel/network/e1000.c \
    sys/kernel/network/pci.c \
    sys/kernel/network/timer.c \
    sys/kernel/network/net.c \
    sys/kernel/fb.c \
    sys/kernel/Apps/Games/snake.c \
    sys/kernel/apps_list.c \
    sys/kernel/dead_boot.c \
	sys/kernel/memory/pmm.c \
	sys/kernel/memory/mmu.c \
	drivers/usb/xhci.c \
    sys/kernel/Apps/Games/rps.c \
    sys/kernel/Apps/Games/game_coin.c \
    sys/kernel/utf8.c \
    compiler/lexer/lexer.c \
    compiler/parser/parser.c \
    compiler/codegen/codegen.c \
    compiler/nwc_runtime.c \
    compiler/nwc_stdio.c \
    compiler/nwc_kernel.c \
    sys/kernel/package.c

# Optional local driver.
ifneq ($(wildcard $(ROOT)/drivers/touchpad.c),)
SOURCES_REL += drivers/touchpad.c
else
$(info NOTE: drivers/touchpad.c not found; building without the optional touchpad driver.)
endif

SOURCES := $(addprefix $(ROOT)/,$(SOURCES_REL))
OBJECTS := $(patsubst $(ROOT)/%.c,$(BUILD)/obj/%.o,$(SOURCES))
KERNEL_ENTRY_OBJ := $(BUILD)/kernel_entry.o
KERNEL_ELF := $(BUILD)/kernel.elf
BOOT_BIN := $(BUILD)/boot.bin
VIDEO_CONFIG_H := $(BUILD)/video_config.h
GENERATED_STAMP := $(BUILD)/generated.stamp
LIMINE_CONF := $(BUILD)/limine.conf
VENTOY_GRUB_CONF := $(BUILD)/ventoy-grub.cfg

.PHONY: all iso kernel boot-sector clean check-tools help

all: iso boot-sector

iso: $(ISO_OUT)

kernel: $(KERNEL_ELF)

boot-sector: $(BOOT_BIN)

help:
	@printf '%s\n' \
	  'NwOS build targets:' \
	  '  make -j$(nproc)  Build kernel and hybrid BIOS/UEFI ISO (default)' \
	  '  make kernel      Build and validate build/kernel.elf' \
	  '  make iso         Build NwOS.iso' \
	  '  make boot-sector Assemble build/boot.bin (auxiliary; not added to ISO)' \
	  '  make clean       Remove build/ and NwOS.iso'

check-tools:
	@missing=(); \
	for tool in $(REQUIRED_TOOLS); do \
	  command -v "$$tool" >/dev/null 2>&1 || missing+=("$$tool"); \
	done; \
	if (( $${#missing[@]} )); then \
	  printf '\nMissing build tools: %s\n' "$${missing[*]}" >&2; \
	  printf 'On Debian/Ubuntu install: sudo apt install clang lld nasm xorriso python3 curl tar coreutils file grub-common make\n' >&2; \
	  exit 1; \
	fi

$(BUILD): | check-tools
	mkdir -p "$@"

$(ROOT)/img: | check-tools
	mkdir -p "$@"

$(VIDEO_CONFIG_H): $(ROOT)/Makefile | $(BUILD)
	@{ \
	  printf '%s\n' '#ifndef NWOS_VIDEO_CONFIG_H' '#define NWOS_VIDEO_CONFIG_H'; \
	  printf '#define NWOS_BOOT_WIDTH %s\n' '$(BOOT_WIDTH)'; \
	  printf '#define NWOS_BOOT_HEIGHT %s\n' '$(BOOT_HEIGHT)'; \
	  printf '#define NWOS_GUI_WIDTH %s\n' '$(GUI_WIDTH)'; \
	  printf '#define NWOS_GUI_HEIGHT %s\n' '$(GUI_HEIGHT)'; \
	  printf '%s\n' '#endif'; \
	} > "$@.tmp"
	@if ! cmp -s "$@.tmp" "$@"; then mv "$@.tmp" "$@"; else rm -f "$@.tmp"; fi

$(GENERATED_STAMP): $(ROOT)/tools/gen_wallpaper.py $(ROOT)/tools/gen_packages.py | $(BUILD) $(ROOT)/img
	@test -f "$(ROOT)/tools/gen_wallpaper.py" || { echo 'Missing tools/gen_wallpaper.py' >&2; exit 1; }
	@test -f "$(ROOT)/tools/gen_packages.py" || { echo 'Missing tools/gen_packages.py' >&2; exit 1; }
	cd "$(ROOT)" && $(PYTHON) tools/gen_wallpaper.py
	cd "$(ROOT)" && $(PYTHON) tools/gen_packages.py
	@touch "$@"

$(BUILD)/obj/%.o: $(ROOT)/%.c $(VIDEO_CONFIG_H) $(GENERATED_STAMP) | check-tools
	@mkdir -p "$(@D)"
	@if [[ "$<" == "$(ROOT)"/compiler/*.c ]]; then \
	  $(CLANG) $(KERNEL_CFLAGS) -DNWC_KERNEL \
	    -I"$(ROOT)/compiler/kernel_include" \
	    -I"$(ROOT)/compiler" \
	    -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"; \
	else \
	  $(CLANG) $(KERNEL_CFLAGS) \
	    -MMD -MP -MF "$(@:.o=.d)" -c "$<" -o "$@"; \
	fi

$(KERNEL_ENTRY_OBJ): $(ROOT)/sys/kernel/kernel_entry.asm | check-tools $(BUILD)
	$(NASM) -f elf64 "$<" -o "$@"

$(BOOT_BIN): $(ROOT)/sys/boot/boot.asm | check-tools $(BUILD)
	$(NASM) -f bin "$<" -o "$@"

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(OBJECTS) $(ROOT)/linker.ld | check-tools
	$(LD_LLD) -m elf_x86_64 -nostdlib -z noexecstack \
	  --gc-sections -T "$(ROOT)/linker.ld" \
	  $(KERNEL_ENTRY_OBJ) $(OBJECTS) -o "$@"
	@test -s "$@" || { echo 'Kernel was not created' >&2; exit 1; }
	@$(FILE) "$@"
ifeq ($(CHECK_MULTIBOOT),1)
	@grub-file --is-x86-multiboot2 "$@" || { \
	  echo 'ERROR: kernel.elf has no valid Multiboot2 header.' >&2; \
	  echo 'Check kernel_entry.asm and linker.ld; refusing to create a broken ISO.' >&2; \
	  exit 1; \
	}
	@echo 'Multiboot2 header: OK'
else
	@echo 'WARNING: grub-file check disabled (CHECK_MULTIBOOT=0).'
endif

$(LIMINE_ARCHIVE): | $(BUILD) check-tools
	@set -eu; \
	if [[ -f "$@" ]] && printf '%s  %s\n' '$(LIMINE_SHA256)' "$@" | $(SHA256SUM) --check --status; then \
	  echo 'Using verified Limine archive: $@'; \
	else \
	  rm -f "$@" "$@.tmp"; \
	  echo 'Downloading Limine v$(LIMINE_VERSION)...'; \
	  $(CURL) --fail --location --retry 3 --output "$@.tmp" '$(LIMINE_URL)'; \
	  printf '%s  %s\n' '$(LIMINE_SHA256)' "$@.tmp" | $(SHA256SUM) --check -; \
	  mv "$@.tmp" "$@"; \
	fi

$(LIMINE_STAMP): $(LIMINE_ARCHIVE) | check-tools
	@set -eu; \
	rm -rf "$(LIMINE_DIR)"; \
	mkdir -p "$(LIMINE_DIR)/bin"; \
	$(TAR) -xzf "$<" -C "$(LIMINE_DIR)"; \
	find_asset() { find "$(LIMINE_DIR)" -type f -iname "$$1" -print -quit; }; \
	bios_sys="$$(find_asset limine-bios.sys)"; \
	bios_cd="$$(find_asset limine-bios-cd.bin)"; \
	uefi_cd="$$(find_asset limine-uefi-cd.bin)"; \
	efi64="$$(find_asset BOOTX64.EFI)"; \
	for spec in "limine-bios.sys:$$bios_sys" "limine-bios-cd.bin:$$bios_cd" "limine-uefi-cd.bin:$$uefi_cd" "BOOTX64.EFI:$$efi64"; do \
	  name="$${spec%%:*}"; path="$${spec#*:}"; \
	  [[ -n "$$path" && -f "$$path" ]] || { echo "Limine v$(LIMINE_VERSION): missing $$name" >&2; exit 1; }; \
	done; \
	$(FILE) "$$efi64" | tee "$(LIMINE_DIR)/bin/efi-file-type.txt"; \
	$(FILE) "$$efi64" | grep -Eiq 'PE32\+.*(x86-64|x86_64)' || { echo 'BOOTX64.EFI is not an x86-64 EFI application' >&2; exit 1; }; \
	install -m 0644 "$$bios_sys" "$(LIMINE_DIR)/bin/limine-bios.sys"; \
	install -m 0644 "$$bios_cd" "$(LIMINE_DIR)/bin/limine-bios-cd.bin"; \
	install -m 0644 "$$uefi_cd" "$(LIMINE_DIR)/bin/limine-uefi-cd.bin"; \
	install -m 0644 "$$efi64" "$(LIMINE_DIR)/bin/BOOTX64.EFI"; \
	tool="$$(find "$(LIMINE_DIR)" -type f -name limine -perm /111 -print -quit)"; \
	if [[ -z "$$tool" ]]; then \
	  mkfile="$$(find "$(LIMINE_DIR)" -type f \( -name Makefile -o -name makefile -o -name GNUmakefile \) -print -quit)"; \
	  [[ -n "$$mkfile" ]] || { echo 'No Makefile found in Limine binary package' >&2; exit 1; }; \
	  make_dir="$$(dirname "$$mkfile")"; \
	  echo "Building Limine host utility in: $$make_dir"; \
	  make -C "$$make_dir" CC='$(CLANG)'; \
	  tool="$$(find "$(LIMINE_DIR)" -type f -name limine -perm /111 -print -quit)"; \
	fi; \
	[[ -n "$$tool" && -x "$$tool" ]] || { echo 'Could not find/build Limine host utility' >&2; exit 1; }; \
	install -m 0755 "$$tool" "$(LIMINE_TOOL)"; \
	touch "$@"

$(LIMINE_TOOL): $(LIMINE_STAMP)
	@test -x "$@"

# Regenerate only when the actual config content changes.
$(LIMINE_CONF): $(ROOT)/Makefile | $(BUILD)
	@{ \
	  printf '%s\n' 'timeout: 3' '' '/NwOS' \
	    '    protocol: multiboot2' \
	    '    path: boot():/boot/kernel.elf' \
	    '    resolution: $(GUEST_WIDTH)x$(GUEST_HEIGHT)x32'; \
	} > "$@.tmp"
	@if ! cmp -s "$@.tmp" "$@"; then mv "$@.tmp" "$@"; else rm -f "$@.tmp"; fi

$(VENTOY_GRUB_CONF): $(ROOT)/Makefile | $(BUILD)
	@printf '%s\n' \
	  '# NwOS Ventoy GRUB2 fallback: select a real UEFI GOP mode first.' \
	  'set root=(loop)' \
	  'insmod all_video' \
	  'insmod gfxterm' \
	  'set gfxmode=auto' \
	  'terminal_output gfxterm' \
	  'set gfxpayload=keep' \
	  'insmod iso9660' \
	  'insmod multiboot2' \
	  'multiboot2 /boot/kernel.elf' \
	  'boot' \
	  > "$@.tmp"
	@if ! cmp -s "$@.tmp" "$@"; then mv "$@.tmp" "$@"; else rm -f "$@.tmp"; fi

$(ISO_STAMP): $(KERNEL_ELF) $(LIMINE_STAMP) $(LIMINE_CONF) $(VENTOY_GRUB_CONF) $(ROOT)/Makefile | check-tools
	@set -eu; \
	rm -rf "$(ISO_ROOT)"; \
	mkdir -p "$(ISO_ROOT)/boot/limine" "$(ISO_ROOT)/boot/grub" "$(ISO_ROOT)/EFI/BOOT"; \
	install -m 0644 "$(KERNEL_ELF)" "$(ISO_ROOT)/boot/kernel.elf"; \
	install -m 0644 "$(LIMINE_DIR)/bin/limine-bios.sys" "$(ISO_ROOT)/boot/limine/limine-bios.sys"; \
	install -m 0644 "$(LIMINE_DIR)/bin/limine-bios-cd.bin" "$(ISO_ROOT)/boot/limine/limine-bios-cd.bin"; \
	install -m 0644 "$(LIMINE_DIR)/bin/limine-uefi-cd.bin" "$(ISO_ROOT)/boot/limine/limine-uefi-cd.bin"; \
	install -m 0644 "$(LIMINE_DIR)/bin/BOOTX64.EFI" "$(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI"; \
	install -m 0644 "$(LIMINE_CONF)" "$(ISO_ROOT)/boot/limine/limine.conf"; \
	install -m 0644 "$(VENTOY_GRUB_CONF)" "$(ISO_ROOT)/boot/grub/grub.cfg"; \
	touch "$@"

# Hybrid BIOS/UEFI ISO. --efi-boot-part exposes Limine's UEFI El Torito FAT image as a GPT EFI System Partition.
# Do not add -partition_offset here: this xorriso mkisofs invocation treats its argument as a source path.
$(ISO_OUT): $(ISO_STAMP) $(LIMINE_TOOL) $(ROOT)/Makefile | check-tools
	@set -eu; \
	rm -f "$(ISO_TMP)"; \
	$(XORRISO) -as mkisofs \
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
	  "$(ISO_ROOT)" \
	  -o "$(ISO_TMP)"; \
	test -s "$(ISO_TMP)"; \
	"$(LIMINE_TOOL)" bios-install "$(ISO_TMP)"; \
	system_report="$$($(XORRISO) -indev "$(ISO_TMP)" -report_system_area plain 2>&1)"; \
	printf '\n%s\n' '=== ISO system area / partition table ===' "$$system_report"; \
	report="$$( $(XORRISO) -indev "$(ISO_TMP)" -report_el_torito plain 2>&1 )"; \
	printf '\n%s\n' '=== El Torito boot catalog ===' "$$report"; \
	printf '%s\n' "$$report" | grep -Fq 'limine-uefi-cd.bin' || { \
	  echo 'ERROR: ISO has no UEFI El Torito entry for limine-uefi-cd.bin' >&2; \
	  exit 1; \
	}; \
	efi_listing="$$($(XORRISO) -indev "$(ISO_TMP)" -ls /EFI/BOOT 2>&1)"; \
	printf '%s\n' "$$efi_listing"; \
	printf '%s\n' "$$efi_listing" | grep -Eiq 'BOOTX64\.EFI' || { \
	  echo 'ERROR: /EFI/BOOT/BOOTX64.EFI is missing from the ISO filesystem' >&2; \
	  exit 1; \
	}; \
	grub_listing="$$($(XORRISO) -indev "$(ISO_TMP)" -ls /boot/grub 2>&1)"; \
	printf '%s\n' "$$grub_listing"; \
	printf '%s\n' "$$grub_listing" | grep -Fq 'grub.cfg' || { \
	  echo 'ERROR: /boot/grub/grub.cfg Ventoy fallback config is missing from ISO' >&2; \
	  exit 1; \
	}; \
	mv "$(ISO_TMP)" "$@"; \
	echo; \
	$(FILE) "$@"; \
	ls -lh "$@"; \
	$(SHA256SUM) "$@"; \
	printf '\n============================\n       BUILD SUCCESS\n============================\nISO: %s\nISO tree: %s\nLimine: v%s\n' "$@" "$(ISO_ROOT)" "$(LIMINE_VERSION)"

clean:
	rm -rf "$(BUILD)" "$(ISO_OUT)" "$(ISO_TMP)"

-include $(OBJECTS:.o=.d)