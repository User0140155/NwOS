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
    unsigned int count;

    for (int i = 0; i < fs_dir_cap; i++)
        fs_entry_clear(i);

    fs_sb_ok = 0;
    fs_fat_invalidate();

    if (!fs_ram_mode && ata_drive_count() == 0) return;

    if (fs_sb_load() != 0)
        return;

    count = fs_sb.dir_size / (unsigned int)sizeof(DirEntry);

    if (count == 0)
        return;

    if (fs_dir_reserve((int)count + 64) != 0)
    {
        print_error("Warning: not enough memory for the directory.\n");
        fs_sb_ok = 0;
        return;
    }

    if (fs_chain_read(fs_sb.dir_first, 0, (unsigned char*)directory,
                      count * (unsigned int)sizeof(DirEntry)) != 0)
    {
        print_error("Warning: FS read failed.\n");
        for (int i = 0; i < fs_dir_cap; i++)
            fs_entry_clear(i);
        fs_sb_ok = 0;
        return;
    }

    for (int i = 0; i < (int)count; i++)
    {
        if (directory[i].used != 1)
            fs_entry_clear(i);
        else
            directory[i].name[MAX_FILENAME - 1] = '\0';
    }
}

void fs_save_directory(void)
{
    int last = 0;
    unsigned int bytes;
    unsigned int first;

    if (!fs_ram_mode && ata_drive_count() == 0) return;
    if (!fs_sb_ok) return;

    for (int i = 0; i < fs_dir_cap; i++)
    {
        if (directory[i].used)
            last = i;
    }

    bytes = (unsigned int)(last + 1) * (unsigned int)sizeof(DirEntry);
    first = fs_sb.dir_first;

    if (fs_chain_write(&first, (const unsigned char*)directory, bytes) != 0)
    {
        print_error("Error: cannot save the directory (disk full?).\n");
        return;
    }

    fs_sb.dir_first = first;
    fs_sb.dir_size = bytes;
    fs_sb_save();
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
