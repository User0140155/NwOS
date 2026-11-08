#include "nwos_internal.h"

static void fs_rm_one(const char* name, int recursive, int force);

static void fs_rm_one(const char* name, int recursive, int force)
{
    int index = fs_find(name);

    if (index == -1)
    {
        if (!force)
            print_error("No such file or directory: ");
        if (!force)
        {
            print(name);
            putchar_os('\n');
        }
        return;
    }

    if (index == 0)
    {
        print_error("Cannot remove the root directory.\n");
        return;
    }

    if (directory[index].type == FS_TYPE_DIR)
    {
        if (!recursive)
        {
            print_error("Is a directory. Use 'rm -r' or 'rmdir'.\n");
            return;
        }

        if (fs_entry_requires_admin(index))
        {
            if (!authorize_sensitive_action(
                    "Deleting a protected system directory",
                    directory[index].name))
                return;
        }

        uint16_t parent = directory[index].parent;
        fs_removed_count = 0;
        fs_entry_release_tree(index, 0);
        fs_release_finish(parent);
        fs_save_directory();

        print_success("Directory removed: ");
        print(name);
        putchar_os('\n');
        return;
    }

    if (fs_entry_requires_admin(index))
    {
        if (!authorize_sensitive_action(
                "Deleting a protected system file",
                directory[index].name))
            return;
    }

    fs_removed_count = 0;
    fs_entry_release(index);
    fs_save_directory();
    print_success("File removed: ");
    print(name);
    putchar_os('\n');
}

void fs_rm(const char* args)
{
    const char* p = args ? args : "";
    int recursive = 0;
    int force = 0;
    int targets = 0;

    while (*p == ' ' || *p == '\t') p++;

    while (*p == '-')
    {
        p++;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',')
        {
            if (*p == 'r' || *p == 'R') recursive = 1;
            else if (*p == 'f' || *p == 'F') force = 1;
            p++;
        }
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
    }

    while (*p != '\0')
    {
        char name[MAX_FILENAME];
        int n = 0;
        int too_long = 0;

        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == '\0') break;

        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',')
        {
            if (n < MAX_FILENAME - 1)
                name[n++] = *p;
            else
                too_long = 1;
            p++;
        }
        name[n] = '\0';

        while (n > 0 && name[n - 1] == '/')
            name[--n] = '\0';

        if (n == 0)
            continue;

        targets++;

        if (too_long)
        {
            print_error("Filename is too long (maximum 31 characters).\n");
            continue;
        }

        fs_rm_one(name, recursive, force);
    }

    if (targets == 0)
        print_error("Usage: rm [-r] [-f] <file1> [file2 ...]\n");
}
