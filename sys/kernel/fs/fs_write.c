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
    if (name == 0 || name[0] == '\0') return 0;
    if (fs_find(name) == -1) return 0;

    fs_quiet = 1;
    fs_delete(name);
    fs_quiet = 0;

    return fs_find(name) == -1;
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
