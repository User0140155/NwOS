#include "nwos_internal.h"

static unsigned long long install_disk_usage(void);

int fs_install_format_partition(int drive_idx, unsigned int partition_lba)
{
    int saved_live_build;

    if (drive_idx < 0 || drive_idx >= ata_drive_count())
        return -1;

    fs_ram_mode = 0;
    fs_set_drive(drive_idx);
    fs_part_lba = partition_lba;

    saved_live_build = fs_live_build;
    fs_live_build = 1;
    fs_format_disk(drive_idx);
    fs_live_build = saved_live_build;

    return 0;
}

void fs_format(void) {
    fs_format_disk(fs_current_drive);
}

unsigned int fs_layout_sectors(void)
{
    return FS_MIN_SECTORS;
}

void fs_live_init(void)
{
    static const unsigned int mb_sizes[] = { 64, 32, 16, 8 };

    fs_ram_mode = 1;
    fs_part_lba = 0;
    fs_current_drive = 0;

    int fresh = 0;

    if (fs_ram_lbas <= FS_RAM_LBAS)
    {
        for (unsigned int k = 0; k < 4; k++)
        {
            unsigned long bytes = (unsigned long)mb_sizes[k] * 1024ul * 1024ul;
            unsigned char* mem = (unsigned char*)fs_mem_alloc(bytes);

            if (mem != 0)
            {
                fs_ram_disk = mem;
                fs_ram_lbas = (unsigned int)(bytes / 512ul);
                fresh = 1;
                break;
            }
        }
    }

    /* Fresh memory from the page allocator is zeroed; the static fallback
       must be cleared explicitly. */
    if (!fresh)
    {
        for (unsigned long i = 0; i < (unsigned long)fs_ram_lbas * 512ul; i++)
            fs_ram_disk[i] = 0;
    }

    cfg_require_login = 0;
    cfg_setup_complete = 1;

    fs_live_build = 1;
    fs_format_disk(0);
    fs_live_build = 0;

    current_dir = 0;
}

static unsigned long long install_disk_usage(void)
{
    return (unsigned long long)fs_layout_sectors() * 512ULL;
}
