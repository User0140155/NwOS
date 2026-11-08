#include "nwos_internal.h"

int fs_entry_requires_admin(int index)
{
    if (index <= 0 || index >= MAX_FILES ||
        !directory[index].used)
    {
        return 0;
    }

    if (directory[index].parent == 0 &&
        directory[index].type == FS_TYPE_DIR &&
        (strcmp(directory[index].name, "sys") == 0 ||
         strcmp(directory[index].name, "home") == 0 ||
         strcmp(directory[index].name, "bin") == 0))
    {
        return 1;
    }

    if (directory[index].type == FS_TYPE_FILE &&
        fs_name_ends_with(directory[index].name, ".nwo"))
    {
        if (strcmp(directory[index].name, "gui.nwo") == 0)
            return 1;

        return 0;
    }

    if (directory[index].type == FS_TYPE_FILE &&
        fs_entry_in_sys_apps_tree(index))
    {
        return 0;
    }

    if (fs_entry_in_tree(index, "sys") ||
        fs_entry_in_tree(index, "bin"))
    {
        return 1;
    }

    return 0;
}

void fs_rmdir(const char* name)
{
    char buf[MAX_FILENAME + 1];

    if (name == 0 || name[0] == '\0')
    {
        print_error("Usage: rmdir <directory>\n");
        return;
    }

    if (fs_name_copy(name, buf, sizeof(buf)) == 0)
    {
        print_error("Cannot remove the root directory.\n");
        return;
    }

    int index = fs_find(buf);

    if (index == -1)
    {
        print_error("Directory not found.\n");
        return;
    }

    if (index == 0)
    {
        print_error("Cannot remove the root directory.\n");
        return;
    }

    if (directory[index].type != FS_TYPE_DIR)
    {
        print_error("Not a directory. Use 'rm' to remove files.\n");
        return;
    }

    if (fs_entry_requires_admin(index))
    {
        if (!authorize_sensitive_action(
                "Deleting a protected system directory",
                directory[index].name))
        {
            return;
        }
    }

    uint16_t parent = directory[index].parent;

    fs_removed_count = 0;
    fs_entry_release_tree(index, 0);
    fs_release_finish(parent);

    fs_save_directory();

    print_success("Directory removed.\n");
}
