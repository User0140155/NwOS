#include "nwos_internal.h"

static void run_startup_nwo(const char* filename);

static void run_startup_nwo(const char* filename)
{
    uint16_t saved_dir = current_dir;
    int bin_index = -1;

    if (filename == 0 || filename[0] == '\0')
        return;

    current_dir = 0;

    for (int i = 1; i < fs_dir_cap; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 &&
            strcmp(directory[i].name, "bin") == 0)
        {
            bin_index = i;
            break;
        }
    }

    if (bin_index == -1)
    {
        print_error("[INIT] /bin not found.\n");
        current_dir = saved_dir;
        return;
    }

    current_dir = (uint16_t)bin_index;

    int index = fs_find(filename);

    if (index == -1)
    {
        print_error("[INIT] Application not found: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    if (directory[index].type != FS_TYPE_FILE)
    {
        print_error("[INIT] Not a file: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    print("[INIT] Starting /bin/");
    print(filename);
    putchar_os('\n');

    nwo_run(filename);

    current_dir = saved_dir;
}

void run_nwo_from_dir(
    const char* dir_path,
    const char* filename
)
{
    if (!dir_path || !filename ||
        dir_path[0] == '\0' || filename[0] == '\0')
    {
        print_error("[INIT] Invalid application path.\n");
        return;
    }

    for (int i = 0; filename[i] != '\0'; i++)
    {
        if (filename[i] == '/')
        {
            print_error("[INIT] Use a filename, not a full path.\n");
            return;
        }
    }

    uint16_t saved_dir = current_dir;

    int dir = fs_resolve_dir_path(dir_path);

    if (dir < 0)
    {
        print_error("[INIT] Directory not found: ");
        print(dir_path);
        putchar_os('\n');
        return;
    }

    current_dir = (uint16_t)dir;

    int index = fs_find(filename);

    if (index < 0)
    {
        print_error("[INIT] Application not found: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    if (directory[index].type != FS_TYPE_FILE)
    {
        print_error("[INIT] Not a file: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    print("[INIT] Running ");
    print(dir_path);
    print("/");
    print(filename);
    putchar_os('\n');

    nwo_run(filename);

    current_dir = saved_dir;
}
