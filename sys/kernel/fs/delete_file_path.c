#include "nwos_internal.h"

int fs_path_is_file(const char* path)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int index;

    if (!fs_split_file_path(path, dir_path, sizeof(dir_path), leaf))
        return 0;

    parent = fs_resolve_dir_path(dir_path);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = fs_find(leaf);
    current_dir = saved_dir;

    return index >= 0 && directory[index].type == FS_TYPE_FILE;
}

int fs_write_binary_path(const char* path, const unsigned char* data,
                         unsigned int size)
{
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int result;

    if (path == 0)
        return 0;

    parent = fs_prepare_rootfs_path(path, leaf);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    result = fs_write_binary(leaf, data, size);
    current_dir = saved_dir;

    return result;
}

int fs_delete_file_path(const char* path)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int index;

    if (!fs_split_file_path(path, dir_path, sizeof(dir_path), leaf))
        return 0;

    parent = fs_resolve_dir_path(dir_path);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = fs_find(leaf);

    if (index < 0 || directory[index].type != FS_TYPE_FILE)
    {
        current_dir = saved_dir;
        return 0;
    }

    fs_file_release(index);
    fs_entry_clear(index);
    fs_save_directory();
    current_dir = saved_dir;

    return 1;
}

int fs_run_nwo_path(const char* path)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int index;

    if (!fs_split_file_path(path, dir_path, sizeof(dir_path), leaf))
        return 0;

    parent = fs_resolve_dir_path(dir_path);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = fs_find(leaf);

    if (index < 0 || directory[index].type != FS_TYPE_FILE)
    {
        current_dir = saved_dir;
        return 0;
    }

    nwo_run(leaf);
    current_dir = saved_dir;

    return 1;
}
