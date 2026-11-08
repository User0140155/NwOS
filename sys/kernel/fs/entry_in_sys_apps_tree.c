#include "nwos_internal.h"

int fs_name_copy(const char* name, char* buf, int max)
{
    int n = 0;

    while (name[n] != '\0' && n < max - 1) n++;

    while (n > 0 && name[n - 1] == '/') n--;

    for (int i = 0; i < n; i++) buf[i] = name[i];

    buf[n] = '\0';

    return n;
}

int fs_entry_in_tree(int index, const char* root_name)
{
    int current = index;

    for (int depth = 0;
         depth < MAX_FILES && current > 0;
         depth++)
    {
        if (current >= MAX_FILES || !directory[current].used)
            return 0;

        if (directory[current].parent == 0 &&
            strcmp(directory[current].name, root_name) == 0)
        {
            return 1;
        }

        current = directory[current].parent;
    }

    return 0;
}

int fs_entry_in_sys_apps_tree(int index)
{
    int sys_index = -1;
    int apps_index = -1;

    if (index < 0 || index >= MAX_FILES || !directory[index].used)
        return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 &&
            strcmp(directory[i].name, "sys") == 0)
        {
            sys_index = i;
            break;
        }
    }

    if (sys_index < 0)
        return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == (uint16_t)sys_index &&
            strcmp(directory[i].name, "apps") == 0)
        {
            apps_index = i;
            break;
        }
    }

    if (apps_index < 0)
        return 0;

    int current = index;
    for (int depth = 0; depth < MAX_FILES; depth++)
    {
        if (current == apps_index)
            return 1;

        if (current <= 0 || current >= MAX_FILES ||
            !directory[current].used)
            return 0;

        current = directory[current].parent;
    }

    return 0;
}

int fs_name_ends_with(const char* name, const char* suffix)
{
    int name_len = 0;
    int suffix_len = 0;

    if (name == 0 || suffix == 0)
        return 0;

    while (name[name_len] != '\0') name_len++;
    while (suffix[suffix_len] != '\0') suffix_len++;

    if (suffix_len > name_len)
        return 0;

    return strcmp(name + name_len - suffix_len, suffix) == 0;
}
