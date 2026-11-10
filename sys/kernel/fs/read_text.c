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

    for (int i = 1; i < fs_dir_cap; i++)
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
    unsigned int size;
    unsigned int copy_size;

    if (buffer == 0 || max_size == 0)
        return 0;

    buffer[0] = '\0';

    index = fs_find(name);

    if (index == -1)
        return 0;

    size = directory[index].size;
    copy_size = (size >= max_size) ? max_size - 1 : size;

    if (copy_size != 0 &&
        !fs_file_read(index, 0, (unsigned char*)buffer, copy_size))
        return 0;

    buffer[copy_size] = '\0';

    return 1;
}

int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size)
{
    int index;
    unsigned int size;
    unsigned int copy_size;

    if (name == 0 ||
        buffer == 0 ||
        max_size == 0)
    {
        return 0;
    }

    index = fs_find(name);

    if (index == -1)
        return 0;

    size = directory[index].size;
    copy_size = size < max_size ? size : max_size;

    if (copy_size == 0)
        return 1;

    return fs_file_read(index, 0, buffer, copy_size);
}
