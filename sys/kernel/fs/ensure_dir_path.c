#include "nwos_internal.h"

int fs_build_home_path(char* out, int capacity)
{
    const char* prefix = "/home/";
    int pos = 0;

    if (out == 0 || capacity < 1)
        return 0;

    while (*prefix)
    {
        if (pos >= capacity - 1)
            return 0;

        out[pos++] = *prefix++;
    }

    for (int i = 0; cfg_user[i] != '\0'; i++)
    {
        if (pos >= capacity - 1)
            return 0;

        out[pos++] = cfg_user[i];
    }

    out[pos] = '\0';
    return 1;
}

int fs_ensure_dir_path(const char* path)
{
    char file_path[256];
    char leaf[MAX_FILENAME];
    int len = 0;

    if (path == 0 || path[0] == '\0')
        return -1;

    while (path[len] != '\0')
    {
        if (len >= 220)
            return -1;

        file_path[len] = path[len];
        len++;
    }

    while (len > 1 && file_path[len - 1] == '/')
        len--;

    if (len == 0)
        return -1;

    if (file_path[len - 1] != '/')
        file_path[len++] = '/';

    const char marker[] = ".nwos-dir";

    for (int i = 0; marker[i] != '\0'; i++)
        file_path[len++] = marker[i];

    file_path[len] = '\0';

    int parent = fs_prepare_rootfs_path(file_path, leaf);

    if (parent >= 0)
        fs_save_directory();

    return parent;
}

int fs_create_user_home(void)
{
    char home[80];

    if (!fs_build_home_path(home, sizeof(home)))
        return -1;

    int home_index = fs_ensure_dir_path(home);

    current_dir = 0;

    return home_index;
}

int fs_find(const char* name)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].parent == current_dir &&
            strcmp(directory[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}
