#include "nwos_internal.h"

#define FS_PART_START_LBA 2048u

static void fs_bytes_zero(unsigned char* p, unsigned int n);

static unsigned char fs_ram_static[FS_RAM_LBAS * 512];

uint16_t current_dir = 0;

int fs_current_drive = 0;
int fs_quiet = 0;

int fs_ram_mode = 0;
unsigned int fs_part_lba = 0;
int fs_live_build = 0;
unsigned char* fs_ram_disk = fs_ram_static;
unsigned int fs_ram_lbas = FS_RAM_LBAS;

static void fs_bytes_zero(unsigned char* p, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++) p[i] = 0;
}

int fs_read_lba(unsigned int lba, unsigned char* buf)
{
    if (buf == 0) return -1;

    if (fs_ram_mode)
    {
        unsigned long long abs = (unsigned long long)fs_part_lba + lba;

        if (abs >= fs_ram_lbas)
        {
            fs_bytes_zero(buf, 512);
            return -1;
        }

        unsigned char* src = fs_ram_disk + (unsigned int)abs * 512u;

        for (unsigned int i = 0; i < 512; i++) buf[i] = src[i];

        return 0;
    }

    if (ata_drive_count() == 0) return -1;
    if (fs_current_drive < 0 || fs_current_drive >= ata_drive_count()) return -1;

    return ata_read_sector_d(fs_current_drive, fs_part_lba + lba, buf);
}

int fs_write_lba(unsigned int lba, const unsigned char* buf)
{
    if (buf == 0) return -1;

    if (fs_ram_mode)
    {
        unsigned long long abs = (unsigned long long)fs_part_lba + lba;

        if (abs >= fs_ram_lbas) return -1;

        unsigned char* dst = fs_ram_disk + (unsigned int)abs * 512u;

        for (unsigned int i = 0; i < 512; i++) dst[i] = buf[i];

        return 0;
    }

    if (ata_drive_count() == 0) return -1;
    if (fs_current_drive < 0 || fs_current_drive >= ata_drive_count()) return -1;

    return ata_write_sector_d(fs_current_drive, fs_part_lba + lba, buf);
}

int fs_drive_has_fs_at(int idx, unsigned int base)
{
    unsigned char sector[512];

    if (idx < 0 || idx >= ata_drive_count()) return 0;
    if (ata_read_sector_d(idx, base + FS_MAGIC_LBA, sector) != 0) return 0;

    return *(unsigned int*)sector == FS_MAGIC_VALUE &&
           *(unsigned int*)(sector + FS_VER_OFFSET) == FS_FORMAT_VER;
}

unsigned int fs_drive_fs_base(int idx)
{
    unsigned char mbr[512];

    if (idx < 0 || idx >= ata_drive_count()) return 0xFFFFFFFFu;

    if (ata_read_sector_d(idx, 0, mbr) == 0 &&
        mbr[510] == 0x55 && mbr[511] == 0xAA)
    {
        for (int p = 0; p < 4; p++)
        {
            unsigned char* entry = mbr + 446 + p * 16;

            if (entry[4] == 0) continue;

            unsigned int start =
                (unsigned int)entry[8] |
                ((unsigned int)entry[9] << 8) |
                ((unsigned int)entry[10] << 16) |
                ((unsigned int)entry[11] << 24);

            if (start == 0) continue;

            if (fs_drive_has_fs_at(idx, start)) return start;
        }
    }

    if (fs_drive_has_fs_at(idx, 0)) return 0;

    return 0xFFFFFFFFu;
}
