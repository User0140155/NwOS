#include "nwos_internal.h"

int nwf_fs_create(const char* name)
{
    if (name == 0 || name[0] == '\0') return 0;
    if (fs_find(name) != -1) return 0;

    fs_quiet = 1;
    fs_create(name);
    fs_quiet = 0;

    return fs_find(name) != -1;
}

int nwf_fs_mkdir(const char* name)
{
    if (name == 0 || name[0] == '\0') return 0;
    if (fs_find(name) != -1) return 0;

    fs_quiet = 1;
    fs_mkdir(name);
    fs_quiet = 0;

    return fs_find(name) != -1;
}

int nwf_fs_delete(const char* name)
{
    int index;

    if (name == 0 || name[0] == '\0') return 0;
    index = fs_find(name);
    if (index == -1) return 0;

    if (directory[index].type == FS_TYPE_DIR)
    {
        if (fs_entry_requires_admin(index)) return -1;
        fs_quiet = 1;
        fs_rmdir(name);
        fs_quiet = 0;
    }
    else
    {
        fs_quiet = 1;
        fs_delete(name);
        fs_quiet = 0;
    }

    return fs_find(name) == -1;
}

int nwf_fs_rename(const char* old_name, const char* new_name)
{
    int index;
    int len = 0;

    if (old_name == 0 || new_name == 0) return 0;
    if (old_name[0] == '\0' || new_name[0] == '\0') return 0;

    while (new_name[len] != '\0')
    {
        if (new_name[len] == ' ' || new_name[len] == '\t' ||
            new_name[len] == '\n' || new_name[len] == '\r')
            return 0;
        if (len >= MAX_FILENAME - 1)
            return 0;
        len++;
    }

    index = fs_find(old_name);
    if (index == -1) return 0;
    if (strcmp(old_name, new_name) == 0) return 1;

    for (int i = 0; i < fs_dir_cap; i++)
    {
        if (directory[i].used &&
            directory[i].parent == directory[index].parent &&
            i != index &&
            strcmp(directory[i].name, new_name) == 0)
            return 0;
    }

    strcpy(directory[index].name, new_name);
    fs_save_directory();
    return 1;
}

int nwf_fs_write(const char* name, const char* text)
{
    int index;

    if (name == 0 || name[0] == '\0') return 0;
    if (text == 0) text = "";

    index = fs_find(name);
    fs_quiet = 1;

    if (index == -1)
    {
        fs_create(name);
        index = fs_find(name);
    }

    if (index == -1)
    {
        fs_quiet = 0;
        return 0;
    }

    fs_store_data(index, text, "File written.\n");
    fs_quiet = 0;
    return 1;
}

int nwf_fs_read(const char* name, char* buffer, unsigned int max_size)
{
    if (name == 0 || buffer == 0 || max_size == 0) return 0;
    return fs_read_text(name, buffer, max_size);
}
