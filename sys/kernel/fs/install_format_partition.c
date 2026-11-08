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
    unsigned long long magic_end = (unsigned long long)FS_MAGIC_LBA + 1ULL;
    unsigned long long dir_end = (unsigned long long)FS_DIR_LBA + FS_DIR_SECTORS;
    unsigned long long data_end = (unsigned long long)FS_DATA_LBA +
                                  (unsigned long long)MAX_FILES * FS_FILE_SECTORS;
    unsigned long long end_lba = magic_end;

    if (dir_end > end_lba) end_lba = dir_end;
    if (data_end > end_lba) end_lba = data_end;
    if (end_lba == 0 || end_lba > 0xFFFFFFFFULL)
        return 0;
    return (unsigned int)end_lba;
}
void fs_live_init(void)
{
    fs_ram_mode = 1;
    fs_part_lba = 0;
    fs_current_drive = 0;

    for (unsigned int i = 0; i < (unsigned int)FS_RAM_LBAS * 512u; i++)
        fs_ram_disk[i] = 0;

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
