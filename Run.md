# Run NwOS

To run NwOS:
1. Download a release ISO or build it locally.
2. Start `NwOS.iso` in QEMU or another VM.
3. For UEFI testing, use an OVMF firmware image; the default `run.sh` currently uses QEMU's legacy BIOS path.

## Build from source

On Ubuntu / Debian / WSL2 install the build tools:

```bash
sudo apt update
sudo apt install clang lld nasm python3 make coreutils file \
  xorriso grub-common grub-pc-bin grub-efi-amd64-bin \
  dosfstools mtools qemu-system-x86
```

Then from the project root:

```bash
make clean
make -j"$(nproc)"
./run.sh
```

The Makefile creates a GRUB 2 Multiboot2 ISO for BIOS and x86-64 UEFI. It also creates a standalone GRUB UEFI EFI System Partition image for the NwOS installer. It does not install a bootloader on the host system.

## Requirements

### Minimal
1. 12 MB RAM
2. Any potato videocard
3. Any potato CPU with x86-64 support
4. HDD/SSD 10+ MB

### Recommended
1. 128 MB RAM (for UEFI)
2. Any potato videocard
3. Any potato CPU with x86-64 support
4. HDD/SSD 100+ MB

## Install NwOS to a virtual hard disk

1. Attach `NwOS.iso` and a virtual hard disk (the unchanged `run.sh` creates a 10 MB `img/disk.img` when it is missing).
2. Boot the ISO and choose the installer. Select the target disk and confirm the installation.
3. On a blank 10 MB disk, the installer creates a 7 MiB FAT16 EFI boot partition and a 2 MiB NwFS partition.
4. Power off the VM, detach the ISO, and boot from the virtual hard disk. GRUB loads `kernel.elf` from the disk's EFI partition; the system files and settings are on the NwFS partition under `/sys`, `/home`, and `/bin`.

The installer does not resize or format existing partitions. It writes only into a region whose sectors have been verified as empty; GPT disks are not supported yet. If an NwOS installation is already detected, the kernel starts it instead of showing the installer. Running `install` from the installed system updates only `/sys/kernel.elf` and the existing NwOS EFI `/BOOT/KERNEL.ELF`; other NwFS data and EFI files are preserved. If the existing EFI kernel file has insufficient allocated space, the updater refuses rather than moving or overwriting neighboring files.

The ISO-first boot order in `run.sh` is intentionally unchanged. While the ISO remains attached and selected first, GRUB supplies the initial kernel, which detects and starts the installed NwFS system instead of entering setup. After detaching the ISO, the machine boots the kernel stored on the hard disk. The installed-system ESP built by this Makefile is UEFI-only. Before installing NwOS on a physical disk, back up important data; the installer in this source snapshot is MBR-based and is not ready to install safely alongside a Windows 11 GPT installation.
