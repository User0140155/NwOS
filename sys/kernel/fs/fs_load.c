#include "nwos_internal.h"

void fs_set_drive(int idx)
{
    if (fs_ram_mode)
    {
        fs_current_drive = 0;
        return;
    }

    if (idx < 0 || idx >= ata_drive_count()) return;
    fs_current_drive = idx;
}

int fs_get_drive(void)
{
    return fs_current_drive;
}

void fs_load(void)
{
    unsigned char buffer[FS_DIR_SECTORS * 512];

    for (int i = 0; i < MAX_FILES; i++) {
        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].size = 0;
    }

    if (!fs_ram_mode && ata_drive_count() == 0) return;

    for (int s = 0; s < FS_DIR_SECTORS; s++) {
        if (fs_read_lba(FS_DIR_LBA + s, buffer + s * 512) != 0) {
            print_error("Warning: FS read failed.\n");
            return;
        }
    }

    DirEntry* disk_entries = (DirEntry*)buffer;
    for (int i = 0; i < MAX_FILES; i++) {
        directory[i] = disk_entries[i];
        if (directory[i].used != 1) {
            directory[i].used = 0;
            directory[i].name[0] = '\0';
            directory[i].size = 0;
        }
    }
}
void fs_save_directory(void)
{
    if (!fs_ram_mode && ata_drive_count() == 0) return;
    unsigned char buffer[FS_DIR_SECTORS * 512];

    for (int k = 0; k < FS_DIR_SECTORS * 512; k++) buffer[k] = 0;

    DirEntry* disk_entries = (DirEntry*)buffer;

    for (int i = 0; i < MAX_FILES; i++)
    {
        disk_entries[i] = directory[i];
    }

    for (int s = 0; s < FS_DIR_SECTORS; s++)
    {
        fs_write_lba(FS_DIR_LBA + s, buffer + s * 512);
    }
}
int fs_drive_has_fs(int idx)
{
    return fs_drive_fs_base(idx) != 0xFFFFFFFFu;
}

const char* disk_position_name(ata_drive_t* d)
{
    if (d->type == DISK_TYPE_AHCI) return "SATA AHCI";

    if (d->channel == 0 && d->master) return "primary master";
    if (d->channel == 0)               return "primary slave";
    if (d->master)                     return "secondary master";

    return "secondary slave";
}

int fs_drive_index_by_letter(char letter)
{
    int n = ata_drive_count();

    if (letter >= 'a' && letter <= 'z')
        letter = (char)(letter - 'a' + 'A');

    for (int i = 0; i < n; i++)
    {
        ata_drive_t* d = ata_get_drive(i);

        if (d && d->letter == letter)
            return i;
    }

    return -1;
}

char fs_get_drive_letter(void)
{
    ata_drive_t* d = ata_get_drive(fs_current_drive);

    if (d) return d->letter;

    return 'C';
}
