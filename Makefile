# NwOS build system - GRUB 2 (Multiboot2), BIOS/UEFI ISO, UEFI install ESP.
# Usage: make [-j$(nproc)] [all|kernel|iso|boot-sector|clean|help]
#
# This Makefile does not install GRUB to the host or to a physical disk.
# It only creates NwOS.iso and a FAT ESP image used by the NwOS installer.

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

CLANG ?= clang
LD_LLD ?= ld.lld
NASM ?= nasm
PYTHON ?= python3
SHA256SUM ?= sha256sum
XORRISO ?= xorriso
FILE ?= file
GRUB_FILE ?= grub-file
GRUB_MKRESCUE ?= grub-mkrescue
GRUB_MKSTANDALONE ?= grub-mkstandalone
MKFS_FAT ?= mkfs.fat
MMD ?= mmd
MCOPY ?= mcopy

CHECK_MULTIBOOT ?= 1
REQUIRED_TOOLS := $(CLANG) $(LD_LLD) $(NASM) $(PYTHON) $(SHA256SUM) $(FILE)

# GRUB modules embedded in the live ISO and in the installed-system EFI app.
GRUB_ISO_MODULES := all_video gfxterm part_gpt part_msdos fat iso9660 search search_fs_file multiboot2 test
GRUB_EFI_MODULES := efi_gop video part_gpt part_msdos fat search search_fs_file multiboot2 test normal

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
    -I$(ROOT)/sys/kernel \
    -I$(ROOT)/sys/net \
    -I$(ROOT)/sys/kernel/network \
    -I$(BUILD)

# Keep this list in sync with the source files in the NwOS repository.
SOURCES_REL := \
    sys/kernel/kernel.c \
    sys/kernel/usermode.c \
    sys/kernel/boot/boot_menu.c \
    sys/kernel/boot/boot_system.c \
    sys/kernel/boot/keyboard_input_test.c \
    sys/kernel/boot/startingNWOS.c \
    sys/kernel/config/change_p_color.c \
    sys/kernel/config/find_key.c \
    sys/kernel/config/settings_load.c \
    sys/kernel/config/settings_save.c \
    sys/kernel/config/shell_prompt_text.c \
    sys/kernel/audio/audio.c \
    sys/kernel/audio/hda.c \
    sys/kernel/audio/ac97.c \
    sys/kernel/audio/es1370.c \
    sys/kernel/audio/sb16.c \
    sys/kernel/audio/audiostat.c \
    sys/kernel/audio/listen.c \
    sys/kernel/audio/startup_sound.c \
    sys/kernel/fs/chain.c \
    sys/kernel/fs/create.c \
    sys/kernel/fs/current_path.c \
    sys/kernel/fs/delete_file_path.c \
    sys/kernel/fs/drive_fs_base.c \
    sys/kernel/fs/ensure_dir_path.c \
    sys/kernel/fs/entry_in_sys_apps_tree.c \
    sys/kernel/fs/format_disk.c \
    sys/kernel/fs/fs_load.c \
    sys/kernel/fs/fs_mkdir.c \
    sys/kernel/fs/fs_rmdir.c \
    sys/kernel/fs/fs_write.c \
    sys/kernel/fs/install_format_partition.c \
    sys/kernel/fs/max_seq.c \
    sys/kernel/fs/prepare_rootfs_path.c \
    sys/kernel/fs/read_directory_of.c \
    sys/kernel/fs/read_text.c \
    sys/kernel/fs/resolve_dir_path.c \
    sys/kernel/fs/rm_one.c \
	sys/kernel/sysinfo/uname.c \
	sys/kernel/sysinfo/date_create.c \
    sys/kernel/fs/split_file_path.c \
    sys/kernel/games/game_guess.c \
    sys/kernel/games/game_word.c \
    sys/kernel/games/games_menu.c \
    sys/kernel/games/hangman.c \
    sys/kernel/games/higher_lower.c \
    sys/kernel/games/tictactoe.c \
    sys/kernel/hw/acpi_find_fadt.c \
    sys/kernel/hw/acpi_find_s5.c \
    sys/kernel/hw/acpi_rsdp_valid.c \
    sys/kernel/hw/acpi_shutdown_from_tables.c \
    sys/kernel/hw/reboot.c \
    sys/kernel/input/history_load.c \
    sys/kernel/input/read_line.c \
    sys/kernel/installer/collect_setup.c \
    sys/kernel/installer/read_secret_field.c \
    sys/kernel/installer/read_setup_field.c \
    sys/kernel/installer/ui_progress.c \
    sys/kernel/installer/verify_saved_account.c \
    sys/kernel/lib/format_size_buf.c \
    sys/kernel/lib/random.c \
    sys/kernel/lib/size_unit.c \
    sys/kernel/lib/strncmp.c \
    sys/kernel/script/cmd_bash.c \
    sys/kernel/script/expand_vars.c \
    sys/kernel/script/parse_assignment.c \
    sys/kernel/script/script_run.c \
    sys/kernel/script/set_var.c \
    sys/kernel/security/login_prompt.c \
    sys/kernel/security/passwd.c \
    sys/kernel/security/read_password.c \
    sys/kernel/security/sha256_final.c \
    sys/kernel/security/sha256_transform.c \
    sys/kernel/shell/cd_path.c \
    sys/kernel/shell/cmd_disks.c \
    sys/kernel/shell/cmd_drive.c \
    sys/kernel/shell/cmd_nwc.c \
    sys/kernel/shell/death_screen.c \
    sys/kernel/shell/disk_info.c \
    sys/kernel/shell/execute_single_command.c \
    sys/kernel/shell/nwc_next_token.c \
    sys/kernel/shell/nwo_from_dir.c \
    sys/kernel/shell/shell_execute.c \
    sys/kernel/sysinfo/cpu_load.c \
    sys/kernel/sysinfo/cpu_name.c \
    sys/kernel/sysinfo/date_set.c \
    sys/kernel/sysinfo/nwfetch.c \
    sys/kernel/sysinfo/print_gpu.c \
    sys/kernel/term/gui_term.c \
    sys/kernel/term/gui_dialog.c \
    sys/kernel/term/new_line.c \
    sys/kernel/term/print.c \
    sys/kernel/term/putchar_os.c \
    sys/kernel/term/render_view.c \
    sys/kernel/nwo_loader.c \
    sys/kernel/panic/kernelpanic.c \
    sys/kernel/nwoloader/nwo_vm.c \
    sys/kernel/nwoloader/nwo_runner.c \
    sys/kernel/nwoloader/nwo_native.c \
    sys/kernel/Apps/Games/slot.c \
    sys/kernel/Apps/Games/game_memory.c \
    sys/kernel/Apps/fileStats/filestats.c \
    sys/kernel/Apps/baseconverter/baseconverter.c \
    sys/kernel/Apps/passage/passage.c \
    sys/kernel/random/entropy.c \
    sys/kernel/random/passage_module.c \
    sys/kernel/Apps/textredactor/textredactor.c \
    drivers/keyboard.c \
    drivers/mouse.c \
    drivers/disk.c \
    drivers/ahci.c \
    sys/kernel/network/e1000.c \
    sys/kernel/network/rtl8139.c \
    sys/kernel/network/rtl8169.c \
    sys/kernel/network/ne2k.c \
    sys/kernel/network/pcnet.c \
    sys/kernel/network/rtl8822.c \
    sys/kernel/network/pci.c \
    sys/kernel/network/timer.c \
    sys/kernel/network/net.c \
    sys/kernel/fb.c \
	sys/kernel/time.c \
    sys/kernel/Apps/Games/snake.c \
    sys/kernel/memory/mem.c \
    sys/kernel/graphics.c \
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
    compiler/diag/diag.c \
    compiler/nwc_runtime.c \
    compiler/nwc_stdio.c \
    compiler/nwc_kernel.c \
    sys/kernel/package.c \
	sys/kernel/installer.c \
    sys/kernel/shell/cmd_link.c

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
USER_SHELL_ASM := $(ROOT)/sys/user/shell.asm
USER_SHELL_BIN := $(BUILD)/user_shell.bin
USER_SHELL_HEADER := $(BUILD)/user_shell_blob.h
BOOT_BIN := $(BUILD)/boot.bin
VIDEO_CONFIG_H := $(BUILD)/video_config.h
GENERATED_STAMP := $(BUILD)/generated.stamp
GRUB_ISO_CFG := $(ROOT)/boot/grub/grub.cfg
GRUB_ESP_CFG := $(ROOT)/boot/grub/grub-esp.cfg
GRUB_ESP_EFI := $(BUILD)/grub-esp/BOOTX64.EFI
ESP_IMAGE := $(BUILD)/nwos-esp.img
ESP_PARTITION_SECTORS := 14336

# Optional TempleOS-style Shift+F7 passage corpus (UTF-8, one verse per line).
NWBIBLE_SOURCE := $(ROOT)/data/passages/world_english_bible.txt
NWBIBLE_NOTICE := $(ROOT)/data/licenses/WORLD-ENGLISH-BIBLE-NOTICE.txt
NWBIBLE_MODULE := $(BUILD)/world_english_bible.txt
NWBIBLE_ENABLED := $(if $(wildcard $(NWBIBLE_SOURCE)),1,0)
NWBIBLE_ISO_DEPS :=
ifeq ($(NWBIBLE_ENABLED),1)
NWBIBLE_ISO_DEPS += $(NWBIBLE_MODULE) $(NWBIBLE_NOTICE)
endif

.PHONY: all iso kernel boot-sector clean check-tools check-grub-file check-grub-tools check-boot-tools help

all: iso boot-sector

iso: $(ISO_OUT)

kernel: $(KERNEL_ELF)

boot-sector: $(BOOT_BIN)

help:
	@printf '%s\n' \
	  'NwOS GRUB build targets:' \
	  '  make -j$(nproc)  Build kernel, GRUB ISO and auxiliary boot.bin' \
	  '  make kernel      Build and validate build/kernel.elf' \
	  '  make iso         Build NwOS.iso (GRUB BIOS + UEFI)' \
	  '  make boot-sector Assemble build/boot.bin (auxiliary only)' \
	  '  make clean       Remove build/ and NwOS.iso' \
	  '' \
	  'The installed-system ESP uses standalone GRUB x86_64-efi.' \
	  'No GRUB installation is performed on the host or a physical disk.'

check-tools:
	@missing=(); \
	for tool in $(REQUIRED_TOOLS); do \
	  command -v "$$tool" >/dev/null 2>&1 || missing+=("$$tool"); \
	done; \
	if (( $${#missing[@]} )); then \
	  printf '\nMissing build tools: %s\n' "$${missing[*]}" >&2; \
	  printf 'Install with: sudo apt install clang lld nasm python3 coreutils file make\n' >&2; \
	  exit 1; \
	fi

check-grub-file:
	@command -v "$(GRUB_FILE)" >/dev/null 2>&1 || { \
	  printf 'Missing %s. Install with: sudo apt install grub-common\n' "$(GRUB_FILE)" >&2; \
	  exit 1; \
	}

check-grub-tools:
	@missing=(); \
	for tool in "$(GRUB_FILE)" "$(GRUB_MKRESCUE)" "$(GRUB_MKSTANDALONE)" "$(XORRISO)"; do \
	  command -v "$$tool" >/dev/null 2>&1 || missing+=("$$tool"); \
	done; \
	if (( $${#missing[@]} )); then \
	  printf '\nMissing GRUB/ISO tools: %s\n' "$${missing[*]}" >&2; \
	  printf 'Install with: sudo apt install grub-common grub-pc-bin grub-efi-amd64-bin xorriso\n' >&2; \
	  exit 1; \
	fi

check-boot-tools:
	@missing=(); \
	for tool in "$(MKFS_FAT)" "$(MMD)" "$(MCOPY)"; do \
	  command -v "$$tool" >/dev/null 2>&1 || missing+=("$$tool"); \
	done; \
	if (( $${#missing[@]} )); then \
	  printf '\nMissing FAT image tools: %s\n' "$${missing[*]}" >&2; \
	  printf 'Install with: sudo apt install dosfstools mtools\n' >&2; \
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

ifeq ($(NWBIBLE_ENABLED),1)
$(NWBIBLE_MODULE): $(NWBIBLE_SOURCE) | $(BUILD)
	@test -f "$(NWBIBLE_NOTICE)" || { echo 'Missing data/licenses/WORLD-ENGLISH-BIBLE-NOTICE.txt' >&2; exit 1; }
	@cp "$<" "$@.tmp"
	@cmp -s "$@.tmp" "$@" && rm -f "$@.tmp" || mv "$@.tmp" "$@"
endif

$(USER_SHELL_BIN): $(USER_SHELL_ASM) | check-tools $(BUILD)
	$(NASM) -f bin "$<" -o "$@"

$(USER_SHELL_HEADER): $(USER_SHELL_BIN) $(ROOT)/tools/bin2c.py | check-tools $(BUILD)
	$(PYTHON) $(ROOT)/tools/bin2c.py "$<" "$@"

$(BUILD)/obj/sys/kernel/usermode.o: $(USER_SHELL_HEADER)

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

$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(OBJECTS) $(ROOT)/linker.ld | check-tools check-grub-file $(BUILD)
	$(LD_LLD) -m elf_x86_64 -nostdlib -z noexecstack \
	  --gc-sections -T "$(ROOT)/linker.ld" \
	  $(KERNEL_ENTRY_OBJ) $(OBJECTS) -o "$@"
	@test -s "$@" || { echo 'Kernel was not created' >&2; exit 1; }
	@$(FILE) "$@"
ifeq ($(CHECK_MULTIBOOT),1)
	@$(GRUB_FILE) --is-x86-multiboot2 "$@" || { \
	  echo 'ERROR: kernel.elf has no valid Multiboot2 header.' >&2; \
	  echo 'Check kernel_entry.asm and linker.ld; refusing to create a broken GRUB ISO.' >&2; \
	  exit 1; \
	}
	@echo 'Multiboot2 header: OK'
else
	@echo 'WARNING: grub-file check disabled (CHECK_MULTIBOOT=0).'
endif

# The installed-system ESP is a FAT16 partition containing standalone GRUB,
# the kernel, and an embedded GRUB config. It is copied by the NwOS installer.
$(GRUB_ESP_EFI): $(GRUB_ESP_CFG) | check-grub-tools $(BUILD)
	@mkdir -p "$(@D)"
	$(GRUB_MKSTANDALONE) \
	  --format=x86_64-efi \
	  --output="$@" \
	  --install-modules="$(GRUB_EFI_MODULES)" \
	  --modules="$(GRUB_EFI_MODULES)" \
	  --locales="" --fonts="" --themes="" \
	  "boot/grub/grub.cfg=$(GRUB_ESP_CFG)"
	@test -s "$@" || { echo 'GRUB UEFI executable was not created.' >&2; exit 1; }; \
	max_bytes=$$(( $(ESP_PARTITION_SECTORS) * 512 )); \
	actual_bytes=$$(stat -c %s "$@"); \
	if (( actual_bytes >= max_bytes )); then \
	  printf 'GRUB UEFI executable is too large for the fixed %s MiB ESP (%s bytes).\n' \
	    "$$(( max_bytes / 1024 / 1024 ))" "$$actual_bytes" >&2; \
	  exit 1; \
	fi

$(ESP_IMAGE): $(KERNEL_ELF) $(GRUB_ESP_EFI) $(GRUB_ESP_CFG) $(NWBIBLE_ISO_DEPS) | check-boot-tools $(BUILD)
	@set -eu; \
	rm -f "$@"; \
	truncate -s $$(( $(ESP_PARTITION_SECTORS) * 512 )) "$@"; \
	"$(MKFS_FAT)" -F 16 -s 1 -n NWOSBOOT "$@" >/dev/null; \
	"$(MMD)" -i "$@" ::/EFI; \
	"$(MMD)" -i "$@" ::/EFI/BOOT; \
	"$(MMD)" -i "$@" ::/boot; \
	"$(MMD)" -i "$@" ::/boot/grub; \
	"$(MMD)" -i "$@" ::/boot/nwos; \
	"$(MCOPY)" -i "$@" "$(GRUB_ESP_EFI)" ::/EFI/BOOT/BOOTX64.EFI; \
	"$(MCOPY)" -i "$@" "$(KERNEL_ELF)" ::/boot/kernel.elf; \
	"$(MCOPY)" -i "$@" "$(GRUB_ESP_CFG)" ::/boot/grub/grub.cfg; \
	if [[ -f "$(NWBIBLE_MODULE)" ]]; then \
	  "$(MCOPY)" -i "$@" "$(NWBIBLE_MODULE)" ::/boot/nwos/world_english_bible.txt; \
	fi; \
	"$(FILE)" "$@"; \
	ls -lh "$@"

$(ISO_STAMP): $(KERNEL_ELF) $(ESP_IMAGE) $(GRUB_ISO_CFG) $(NWBIBLE_ISO_DEPS) $(ROOT)/Makefile | check-tools check-grub-tools $(BUILD)
	@set -eu; \
	rm -rf "$(ISO_ROOT)"; \
	mkdir -p "$(ISO_ROOT)/boot/grub" "$(ISO_ROOT)/boot/nwos" "$(ISO_ROOT)/data/licenses"; \
	install -m 0644 "$(KERNEL_ELF)" "$(ISO_ROOT)/boot/kernel.elf"; \
	install -m 0644 "$(ESP_IMAGE)" "$(ISO_ROOT)/boot/nwos/esp.img"; \
	if [[ -f "$(NWBIBLE_MODULE)" ]]; then \
	  install -m 0644 "$(NWBIBLE_MODULE)" "$(ISO_ROOT)/boot/nwos/world_english_bible.txt"; \
	  install -m 0644 "$(NWBIBLE_NOTICE)" "$(ISO_ROOT)/data/licenses/WORLD-ENGLISH-BIBLE-NOTICE.txt"; \
	fi; \
	install -m 0644 "$(GRUB_ISO_CFG)" "$(ISO_ROOT)/boot/grub/grub.cfg"; \
	touch "$@"

# grub-mkrescue creates a bootable GRUB ISO. With both platform module sets
# installed this ISO is bootable on legacy BIOS and x86-64 UEFI systems.
$(ISO_OUT): $(ISO_STAMP) $(ROOT)/Makefile | check-tools check-grub-tools
	@set -eu; \
	rm -f "$(ISO_TMP)"; \
	$(GRUB_MKRESCUE) \
	  --modules="$(GRUB_ISO_MODULES)" \
	  --output="$(ISO_TMP)" \
	  "$(ISO_ROOT)"; \
	test -s "$(ISO_TMP)" || { echo 'GRUB ISO was not created.' >&2; exit 1; }; \
	printf '\n%s\n' '=== GRUB ISO El Torito report ==='; \
	boot_report="$$($(XORRISO) -indev "$(ISO_TMP)" -report_el_torito plain 2>&1)"; \
	printf '%s\n' "$$boot_report"; \
	printf '%s\n' "$$boot_report" | grep -Eiq 'UEFI' || { \
	  echo 'ERROR: GRUB ISO has no UEFI El Torito boot entry.' >&2; \
	  echo 'Check that grub-efi-amd64-bin and mtools are installed.' >&2; \
	  exit 1; \
	}; \
	mv "$(ISO_TMP)" "$@"; \
	$(FILE) "$@"; \
	ls -lh "$@"; \
	$(SHA256SUM) "$@"; \
	printf '\n============================\n       BUILD SUCCESS\n============================\nISO: %s\nISO tree: %s\nBootloader: GRUB 2 (BIOS + UEFI ISO; UEFI-only installed ESP)\n' "$@" "$(ISO_ROOT)"

clean:
	rm -rf "$(BUILD)" "$(ISO_OUT)" "$(ISO_TMP)"

-include $(OBJECTS:.o=.d)
