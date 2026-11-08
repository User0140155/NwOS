#include "nwos_internal.h"

int fs_prepare_rootfs_path(
    const char *path,
    char leaf[MAX_FILENAME]
)
{
    if (path == 0 || path[0] == '\0' || leaf == 0)
        return -1;

    const char *last_slash = 0;

    for (const char *q = path; *q != '\0'; q++)
    {
        if (*q == '/')
            last_slash = q;
    }

    const char *leaf_src =
        last_slash ? last_slash + 1 : path;

    int leaf_len = 0;

    while (leaf_src[leaf_len] != '\0')
    {
        if (leaf_len >= MAX_FILENAME - 1)
            return -1;

        leaf[leaf_len] = leaf_src[leaf_len];
        leaf_len++;
    }

    leaf[leaf_len] = '\0';

    if (leaf_len == 0)
        return -1;

    const char *p = path;
    const char *end = last_slash ? last_slash : path;

    int parent = 0;

    while (p < end)
    {
        while (p < end && *p == '/')
            p++;

        if (p >= end)
            break;

        char part[MAX_FILENAME];
        int len = 0;

        while (p < end && *p != '/')
        {
            if (len >= MAX_FILENAME - 1)
                return -1;

            part[len++] = *p++;
        }

        part[len] = '\0';

        int child = -1;

        for (int i = 1; i < MAX_FILES; i++)
        {
            if (directory[i].used &&
                directory[i].parent == (uint16_t)parent &&
                strcmp(directory[i].name, part) == 0)
            {
                if (directory[i].type != FS_TYPE_DIR)
                    return -1;

                child = i;
                break;
            }
        }

        if (child == -1)
        {
            for (int i = 1; i < MAX_FILES; i++)
            {
                if (!directory[i].used)
                {
                    child = i;
                    break;
                }
            }

            if (child == -1)
                return -1;

            directory[child].used = 1;
            directory[child].type = FS_TYPE_DIR;
            directory[child].parent = (uint16_t)parent;
            directory[child].size = 0;

            for (int i = 0; i <= len; i++)
                directory[child].name[i] = part[i];
        }

        parent = child;
    }

    return parent;
}
