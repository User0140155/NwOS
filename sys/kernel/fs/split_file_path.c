#include "nwos_internal.h"

void fs_store_data(int index, const char* text, const char* success_message)
{
    static unsigned char buffer[FS_FILE_SECTORS * 512];

    int i = 0;

    while (text[i] != '\0' &&
           i < MAX_FILE_SIZE - 1)
    {
        buffer[i] = text[i];
        i++;
    }

    buffer[i] = '\0';

    for (int k = i + 1; k < FS_FILE_SECTORS * 512; k++)
    {
        buffer[k] = 0;
    }

    directory[index].size = (unsigned int)i;

    for (int s = 0; s < FS_FILE_SECTORS; s++)
    {
        fs_write_lba(
            FS_DATA_LBA + index * FS_FILE_SECTORS + s,
            buffer + s * 512);
    }

    fs_save_directory();

    if (text[i] != '\0')
    {
        print_error("Text is too long. Maximum is 20480 characters.\n");
        return;
    }

    if (!fs_quiet) print_success(success_message);
}

void fs_write(const char* name, const char* text)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    fs_store_data(index, text, "File written.\n");
}

int fs_write_binary(const char* name, const unsigned char* data, unsigned int size)
{
    static unsigned char sector[512];
    int index;
    int previous_quiet;
    unsigned int i;
    unsigned int sector_index;

    if (name == 0 || data == 0 || size > MAX_FILE_SIZE ||
        size > FS_FILE_SECTORS * 512u)
        return 0;

    index = fs_find(name);
    if (index == -1)
    {
        previous_quiet = fs_quiet;
        fs_quiet = 1;
        fs_create(name);
        fs_quiet = previous_quiet;
        index = fs_find(name);
    }
    if (index < 0 || directory[index].type != FS_TYPE_FILE)
        return 0;

    directory[index].size = size;
    for (sector_index = 0; sector_index < FS_FILE_SECTORS; ++sector_index)
    {
        for (i = 0; i < 512; ++i)
        {
            unsigned int offset = sector_index * 512u + i;
            sector[i] = offset < size ? data[offset] : 0;
        }
        fs_write_lba(
            FS_DATA_LBA + (unsigned int)index * FS_FILE_SECTORS + sector_index,
            sector);
    }
    fs_save_directory();
    return 1;
}

int fs_split_file_path(const char* path, char* dir_out,
                              int dir_capacity, char* leaf_out)
{
    int last_slash = -1;
    int length = 0;

    if (path == 0 || dir_out == 0 || leaf_out == 0)
        return 0;

    while (path[length] != '\0')
    {
        if (path[length] == '/')
            last_slash = length;
        length++;
    }

    if (last_slash < 0)
        return 0;

    int dir_length = (last_slash == 0) ? 1 : last_slash;

    if (dir_length >= dir_capacity)
        return 0;

    if (last_slash == 0)
    {
        dir_out[0] = '/';
    }
    else
    {
        for (int i = 0; i < dir_length; i++)
            dir_out[i] = path[i];
    }

    dir_out[dir_length] = '\0';

    const char* leaf = path + last_slash + 1;
    int leaf_length = 0;

    while (leaf[leaf_length] != '\0')
    {
        if (leaf_length >= MAX_FILENAME - 1)
            return 0;
        leaf_out[leaf_length] = leaf[leaf_length];
        leaf_length++;
    }

    leaf_out[leaf_length] = '\0';
    return leaf_length > 0;
}
