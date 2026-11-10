#include "nwos_internal.h"

static unsigned int fs_drive_seq(int idx);
static int fs_find_drive_with_fs(void);
static int fs_is_formatted(void);
static void fs_wipe_magic(void);

unsigned int nwf_disk_used_kb(void)
{
    return (unsigned int)(fs_drive_used_bytes(0, 0) / 1024ULL);
}

unsigned int nwf_disk_files(void)
{
    unsigned int files = 0;
    (void)fs_drive_used_bytes(0, &files);
    return files;
}

static unsigned int fs_drive_seq(int idx)
{
    unsigned char sector[512];
    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return 0;

    if (ata_read_sector_d(idx, base + FS_MAGIC_LBA, sector) != 0) return 0;

    return *(unsigned int*)(sector + FS_SEQ_OFFSET);
}

unsigned int fs_max_seq(void)
{
    unsigned int max = 0;

    if (fs_ram_mode)
    {
        unsigned char sector[512];

        if (fs_read_lba(FS_MAGIC_LBA, sector) == 0 &&
            *(unsigned int*)sector == FS_MAGIC_VALUE)
        {
            return *(unsigned int*)(sector + FS_SEQ_OFFSET);
        }

        return 0;
    }

    for (int i = 0; i < ata_drive_count(); i++)
    {
        unsigned int seq = fs_drive_seq(i);

        if (seq > max) max = seq;
    }

    return max;
}

static int fs_find_drive_with_fs(void)
{
    int n = ata_drive_count();
    int best = -1;
    unsigned int best_seq = 0;

    for (int i = 0; i < n; i++)
    {
        if (!fs_drive_has_fs(i)) continue;

        unsigned int seq = fs_drive_seq(i);

        if (best < 0 || seq > best_seq)
        {
            best = i;
            best_seq = seq;
        }
    }

    return best;
}

static int fs_is_formatted(void) {
    unsigned char sector[512];
    if (fs_read_lba(FS_MAGIC_LBA, sector) != 0) return 0;
    return *(unsigned int*)sector == FS_MAGIC_VALUE &&
           *(unsigned int*)(sector + FS_VER_OFFSET) == FS_FORMAT_VER;
}

void fs_mark_formatted(unsigned int seq) {
    fs_sb.seq = seq;
    fs_sb_save();
}

static void fs_wipe_magic(void) {
    unsigned char sector[512];
    for (int k = 0; k < 512; k++) sector[k] = 0;
    *(unsigned int*)sector = 0xDEADBEEF;
    fs_write_lba(FS_MAGIC_LBA, sector);
}

int fs_load_status(void) {
    if (ata_drive_count() == 0) return 0;

    int drive = fs_find_drive_with_fs();

    if (drive < 0) return 0;

    fs_set_drive(drive);
    fs_part_lba = fs_drive_fs_base(drive);

    fs_load();

    for (int i = 0; i < fs_dir_cap; i++) {
        if (directory[i].used && directory[i].parent == 0 &&
            strcmp(directory[i].name, "death.bsod") == 0) {
            return 2;
        }
    }
    return 1;
}
