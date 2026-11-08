#include "nwos_internal.h"

void fs_read(const char* name)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    static unsigned char buffer[MAX_FILE_SIZE];

    if (!fs_read_text(directory[index].name, (char*)buffer, sizeof(buffer)))
    {
        print_error("Read error.\n");
        return;
    }

    print_title("\n--- ");
    print(directory[index].name);
    print(" ---\n");

    print((const char*)buffer);

    putchar_os('\n');

    print_title("--- END ---\n\n");
}

void fs_edit(const char* name, const char* text)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    fs_store_data(index, text, "File edited.\n");
}

void fs_delete(const char* name)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    if (directory[index].type == FS_TYPE_DIR)
    {
        print_error("Is a directory. Use 'rmdir' to remove it.\n");
        return;
    }

    directory[index].used = 0;

    directory[index].name[0] = '\0';
    directory[index].size = 0;

    fs_save_directory();

    if (!fs_quiet) print_success("File deleted.\n");
}

void fs_create(const char* name) {
    int name_len = 0;

    if (name == 0 || name[0] == '\0') {
        print_error("Filename is empty.\n");
        return;
    }

    while (name[name_len] != '\0') {
        if (name[name_len] == ' ' || name[name_len] == '\t' ||
            name[name_len] == '\n' || name[name_len] == '\r') {
            print_error("Filename cannot contain whitespace.\n");
            return;
        }

        if (name_len >= MAX_FILENAME - 1) {
            print_error("Filename is too long (maximum 31 characters).\n");
            return;
        }

        name_len++;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == current_dir && strcmp(directory[i].name, name) == 0) {
            print_error("File already exists.\n");
            return;
        }
    }

    for (int i = 1; i < MAX_FILES; i++) {
        if (!directory[i].used) {
            directory[i].used = 1;
            directory[i].type = FS_TYPE_FILE;
            directory[i].parent = current_dir;
            strcpy(directory[i].name, name);
            directory[i].size = 0;

            unsigned char empty[512];
            for (int k = 0; k < 512; k++) empty[k] = 0;
            for (int s = 0; s < FS_FILE_SECTORS; s++)
                fs_write_lba(
                    FS_DATA_LBA + i * FS_FILE_SECTORS + s,
                    empty);

            fs_save_directory();
            if (!fs_quiet) print_success("File created.\n");
            return;
        }
    }
    print_error("No free file slots.\n");
}
