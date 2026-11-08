#include "nwos_internal.h"

uint16_t fs_removed[MAX_FILES];
int fs_removed_count = 0;

int fs_resolve_dir_path(const char* path)
{
    if (path == 0 || path[0] == '\0')
        return -1;

    int dir = (path[0] == '/') ? 0 : current_dir;
    const char* p = path;

    while (*p)
    {
        while (*p == '/')
            p++;

        if (*p == '\0')
            break;

        char part[MAX_FILENAME];
        int len = 0;

        while (*p && *p != '/')
        {
            if (len >= MAX_FILENAME - 1)
                return -1;

            part[len++] = *p++;
        }

        part[len] = '\0';

        if (strcmp(part, ".") == 0)
            continue;

        if (strcmp(part, "..") == 0)
        {
            if (dir != 0)
                dir = directory[dir].parent;

            continue;
        }

        int found = -1;

        for (int i = 1; i < MAX_FILES; i++)
        {
            if (directory[i].used &&
                directory[i].type == FS_TYPE_DIR &&
                directory[i].parent == (uint16_t)dir &&
                strcmp(directory[i].name, part) == 0)
            {
                found = i;
                break;
            }
        }

        if (found == -1)
            return -1;

        dir = found;
    }

    return dir;
}

unsigned int fs_get_file_size(const char* name)
{
    int index = fs_find(name);

    if (index == -1)
        return 0;

    return directory[index].size;
}

void fs_entry_release(int index)
{
    if (fs_removed_count < MAX_FILES)
        fs_removed[fs_removed_count++] = (uint16_t)index;

    directory[index].used = 0;
    directory[index].type = 0;
    directory[index].parent = 0;
    directory[index].name[0] = '\0';
    directory[index].size = 0;
}

void fs_entry_release_tree(int index, int depth)
{
    if (depth > 32) return;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (i != index && directory[i].used &&
            directory[i].parent == index)
        {
            fs_entry_release_tree(i, depth + 1);
        }
    }

    fs_entry_release(index);
}

void fs_release_finish(uint16_t fallback)
{
    for (int i = 0; i < fs_removed_count; i++)
    {
        if (current_dir == fs_removed[i])
        {
            current_dir = fallback;
            return;
        }
    }
}
