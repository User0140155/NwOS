# Run NwOS

To run NwOS you need:
1. Go to releases
2. Download latest ISO
3. launch it in VM
4. enjoy

## Compile it

If you want to compile it then:
```bash
git clone https://github.com/User0140155/NwOS/
bash disk
bash build.sh
bash run.sh
```

## Compilers

### Arch linux
```bash
sudo pacman -S xorriso gcc clang nasm python qemu-desktop
```
### Linux mint
```bash
sudo apt install xorriso gcc clang nasm python3 qemu-desktop
```
### Other distros

Check guid, and packages for:
1. clang
2. gcc
3. nasm
4. python

### Windows

Download all this from official site:
1. clang
2. gcc
3. nasm
4. python

# Requirements

## Minimal
1. 12 MB RAM
2. Any potato videocard
3. Any potato CPU
4. HDD/SSD 10+ MB

## Recommended
1. 128 MB RAM (for UEFI)
2. Any potato videocard
3. Any potato CPU
4. HDD/SSD 100+ MB

## Note

It can run on some PC with UEFI
## Install NwOS to a virtual hard disk

1. Attach `NwOS.iso` and a virtual hard disk (the unchanged `run.sh` creates a 10 MB `img/disk.img` when it is missing).
2. Boot the ISO and choose the installer. Select the target disk and confirm the installation.
3. On a blank 10 MB disk, the installer creates a 7 MiB FAT16 EFI boot partition and a 2 MiB NwFS partition. It uses the first 1 MiB for the Legacy BIOS boot area when those sectors are empty.
4. Power off the VM, detach the ISO, and boot from the virtual hard disk. Limine loads `kernel.elf` from the disk's EFI partition; the system files and settings are on the NwFS partition under `/sys`, `/home`, and `/bin`.

The installer does not resize or format existing partitions. It writes only into a region whose sectors have been verified as empty; GPT disks are not supported yet. If an NwOS installation is already detected, the kernel starts it instead of showing the installer. Running `install` from the installed system updates only `/sys/kernel.elf` and the existing NwOS EFI `/BOOT/KERNEL.ELF`; other NwFS data and EFI files are preserved. If the existing EFI kernel file has insufficient allocated space, the updater refuses rather than moving or overwriting neighboring files.

The ISO-first boot order in `run.sh` is intentionally unchanged. While the ISO remains attached and selected first, its loader supplies the initial kernel, which detects and starts the installed NwFS system instead of entering setup. After detaching the ISO, the machine boots the kernel stored on the hard disk. If the installer reports UEFI-only support, enable EFI in the VM firmware settings.
