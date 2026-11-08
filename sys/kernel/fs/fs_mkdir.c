#include "nwos_internal.h"

void fs_mkdir(const char* name) {
    if (name[0] == '\0') {
        print_error("Directory name is empty.\n");
        return;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == current_dir && strcmp(directory[i].name, name) == 0) {
            print_error("Directory already exists.\n");
            return;
        }
    }

    for (int i = 1; i < MAX_FILES; i++) {
        if (!directory[i].used) {
            directory[i].used = 1;
            directory[i].type = FS_TYPE_DIR;
            directory[i].parent = current_dir;
            strcpy(directory[i].name, name);
            directory[i].size = 0;

            fs_save_directory();
            if (!fs_quiet) print_success("Directory created.\n");
            return;
        }
    }
    print_error("No free file slots.\n");
}

void fs_ls(void) {
    print_title("\n=== DIRECTORY ===\n\n");
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == current_dir && i != 0) {
            if (directory[i].type == FS_TYPE_DIR) {
                set_color(TERM_COLOR_LIGHT_BLUE);
                print("  [DIR]  ");
                print(directory[i].name);
                print("/\n");
                set_color(base_color);
            } else {
                print("  [FILE] ");
                print(directory[i].name);
                print(" (");
                print_int((int)directory[i].size);
                print(" bytes)\n");
            }
            found = 1;
        }
    }
    if (!found) print("  (empty)\n");
    putchar_os('\n');
}

void fs_cd(const char* name) {
    if (strcmp(name, "..") == 0) {
        if (current_dir != 0) current_dir = directory[current_dir].parent;
        return;
    }

    if (name[0] == '/') {
        current_dir = 0;
    }

    const char* search_name = name;
    if (name[0] == '/') search_name = name + 1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == current_dir &&
            strcmp(directory[i].name, search_name) == 0) {
            current_dir = i;
            return;
        }
    }
}

int fs_dir_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_FILES; i++)
        if (directory[i].used && directory[i].parent == current_dir && i != 0) n++;
    return n;
}

int fs_dir_entry(int index, const char** name, int* type, unsigned int* size)
{
    int n = 0;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].parent == current_dir && i != 0)
        {
            if (n == index)
            {
                if (name) *name = directory[i].name;
                if (type) *type = directory[i].type;
                if (size) *size = directory[i].size;
                return i;
            }
            n++;
        }
    }
    return -1;
}
