#include "nwos_internal.h"

static void cmd_rm_root(void);

static void cmd_rm_root(void)
{
    if (!authorize_sensitive_action(
            "Deleting everything inside the root directory",
            "/"))
    {
        return;
    }

    current_dir = 0;
    fs_removed_count = 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].parent == 0)
        {
            fs_entry_release_tree(i, 0);
        }
    }

    current_dir = 0;
    fs_save_directory();

    print_success("Root directory contents removed.\n");
}

int fs_read_text(
    const char* name,
    char* buffer,
    unsigned int max_size
)
{
    int index;
    unsigned char sector[512];
    unsigned int size;
    unsigned int copy_size;
    unsigned int copied = 0;

    if (buffer == 0 || max_size == 0)
        return 0;

    buffer[0] = '\0';

    index = fs_find(name);

    if (index == -1)
        return 0;

    size = directory[index].size;
    if (size >= max_size)
        copy_size = max_size - 1;
    else
        copy_size = size;

    while (copied < copy_size)
    {
        unsigned int sector_index = copied / 512;
        unsigned int sector_offset = copied % 512;
        unsigned int chunk = 512 - sector_offset;

        if (chunk > copy_size - copied)
            chunk = copy_size - copied;

        fs_read_lba(
            FS_DATA_LBA + index * FS_FILE_SECTORS + sector_index,
            sector
        );

        for (unsigned int i = 0; i < chunk; i++)
            buffer[copied + i] = (char)sector[sector_offset + i];

        copied += chunk;
    }

    buffer[copy_size] = '\0';

    return 1;
}

int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size)
{
    int index;

    if (name == 0 ||
        buffer == 0 ||
        max_size == 0)
    {
        return 0;
    }

    index = fs_find(name);

    if (index == -1)
        return 0;
    if (max_size > FS_FILE_SECTORS * 512)
        max_size = FS_FILE_SECTORS * 512;

    unsigned int size = directory[index].size;
    unsigned int copy_size = size < max_size ? size : max_size;
    unsigned int copied = 0;

    while (copied < copy_size)
    {
        unsigned char sector[512];
        unsigned int sector_index = copied / 512;
        unsigned int sector_offset = copied % 512;
        unsigned int chunk = 512 - sector_offset;

        if (chunk > copy_size - copied)
            chunk = copy_size - copied;

        fs_read_lba(
            FS_DATA_LBA + (unsigned int)index * FS_FILE_SECTORS + sector_index,
            sector);

        for (unsigned int i = 0; i < chunk; i++)
            buffer[copied + i] = sector[sector_offset + i];

        copied += chunk;
    }

    return 1;
}
