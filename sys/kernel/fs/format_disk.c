#define NWOS_INCLUDE_ROOTFS_DATA_H
#include "nwos_internal.h"

static unsigned long long rootfs_total_size(void);

void fs_format_disk(int drive_idx) {
    unsigned char zero[512];
    for (int k = 0; k < 512; k++) zero[k] = 0;

    fs_set_drive(drive_idx);
    fs_quiet = 1;

    unsigned int seq = fs_max_seq() + 1;

    for (int i = 0; i < MAX_FILES * FS_FILE_SECTORS; i++) {
        fs_write_lba(FS_DATA_LBA + i, zero);
    }

    for (int i = 0; i < MAX_FILES; i++) {
        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].size = 0;
    }

    directory[0].used = 1; directory[0].type = FS_TYPE_DIR; directory[0].parent = 0; strcpy(directory[0].name, "/");

    current_dir = 0;

    directory[0].used = 1;
    directory[0].type = FS_TYPE_DIR;
    directory[0].parent = 0;
    strcpy(directory[0].name, "/");
    directory[0].size = 0;

    fs_mkdir("sys");
    fs_mkdir("home");
    fs_mkdir("bin");

    for (int i = 0; i < ROOTFS_FILE_COUNT; i++)
    {
        const char* path = rootfs_files[i].path;
        const unsigned char* data = rootfs_files[i].data;
        unsigned int size = rootfs_files[i].size;

        char file_name[MAX_FILENAME];

        int parent = fs_prepare_rootfs_path(path, file_name);

        if (parent < 0)
        {
            print_error("Cannot prepare rootfs path: ");
            print(path);
            putchar_os('\n');
            continue;
        }

        current_dir = (uint16_t)parent;

        if (!fs_live_build)
        {
            print("    [");
            print_int(i + 1);
            print("/");
            print_int(ROOTFS_FILE_COUNT);
            print("] ");
            print(path);
            print("  (");
            print_int((int)size);
            print(" bytes)\n");
        }

        if (fs_find(file_name) != -1)
            continue;

        fs_create(file_name);

        int idx = fs_find(file_name);

        if (idx == -1)
        {
            print_error("Cannot create file: ");
            print(path);
            putchar_os('\n');
            continue;
        }

        unsigned char sector[512];

        for (unsigned int s = 0; s < FS_FILE_SECTORS; s++)
        {
            unsigned int offset = s * 512U;

            if (offset >= size)
                break;

            for (int k = 0; k < 512; k++)
                sector[k] = 0;

            for (unsigned int k = 0;
                k < 512 && offset + k < size;
                k++)
            {
                sector[k] = data[offset + k];
            }

            if (fs_write_lba(
                    FS_DATA_LBA + (unsigned int)idx *
                        FS_FILE_SECTORS + s,
                    sector) != 0)
            {
                print_error("Disk write failed: ");
                print(path);
                putchar_os('\n');
                break;
            }
        }

        directory[idx].size = size;
    }

    current_dir = 0;

    fs_mark_formatted(seq);
    fs_save_directory();

    fs_quiet = 1;

    int home_ok = fs_create_user_home();

    settings_save();

    fs_quiet = 0;
    current_dir = 0;

    if (home_ok < 0)
        print_error("Warning: could not create user home directory.\n");

    if (!fs_live_build)
        print_success("Filesystem formatted. Rootfs installed.\n");
}

static unsigned long long rootfs_total_size(void)
{
    unsigned long long total = 0;

    for (int i = 0; i < ROOTFS_FILE_COUNT; i++)
    {
        total += rootfs_files[i].size;
    }

    return total;
}
