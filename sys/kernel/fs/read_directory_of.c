#include "nwos_internal.h"

static int fs_read_directory_of(int idx, DirEntry* out);

unsigned char fs_scratch[FS_DIR_SECTORS * 512];

int fs_change_drive(int idx)
{
    if (idx < 0 || idx >= ata_drive_count()) return -1;

    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return -2;

    if (idx != fs_current_drive)
    {
        fs_current_drive = idx;
        fs_part_lba = base;
        current_dir = 0;

        fs_load();
    }
    else
    {
        fs_part_lba = base;
    }

    return 0;
}

int fs_change_drive_letter(char letter)
{
    int idx = fs_drive_index_by_letter(letter);

    if (idx < 0) return -1;

    return fs_change_drive(idx);
}

static int fs_read_directory_of(int idx, DirEntry* out)
{
    if (idx < 0 || idx >= ata_drive_count()) return -1;

    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return -1;

    for (int s = 0; s < FS_DIR_SECTORS; s++)
    {
        if (ata_read_sector_d(idx, base + FS_DIR_LBA + s, fs_scratch + s * 512) != 0)
            return -1;
    }

    DirEntry* disk_entries = (DirEntry*)fs_scratch;

    for (int i = 0; i < MAX_FILES; i++)
    {
        out[i] = disk_entries[i];

        if (out[i].used != 1)
        {
            out[i].used = 0;
            out[i].name[0] = '\0';
            out[i].size = 0;
        }
    }

    return 0;
}

unsigned long long fs_drive_used_bytes(int idx, unsigned int* file_count)
{
    static DirEntry entries[MAX_FILES];

    unsigned long long total = 0;
    unsigned int files = 0;

    if (file_count) *file_count = 0;

    if (fs_read_directory_of(idx, entries) != 0) return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (!entries[i].used) continue;
        if (entries[i].type != FS_TYPE_FILE) continue;

        total += entries[i].size;
        files++;
    }

    if (file_count) *file_count = files;

    return total;
}

unsigned int fs_drive_entry_count(int idx)
{
    static DirEntry entries[MAX_FILES];

    unsigned int count = 0;

    if (fs_read_directory_of(idx, entries) != 0) return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (entries[i].used) count++;
    }

    return count;
}
