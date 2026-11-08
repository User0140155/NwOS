#include "nwos_internal.h"

void cmd_disk_info(const char* args)
{
    if (ata_drive_count() == 0)
    {
        print_error("No ATA disks detected.\n");
        return;
    }

    int idx = resolve_drive_arg(args);

    if (idx < 0)
    {
        print_error("Unknown disk. Type 'disks' to see the list.\n");
        return;
    }

    ata_drive_t* d = ata_get_drive(idx);

    if (d == 0)
    {
        print_error("Unknown disk.\n");
        return;
    }

    unsigned long long total = (unsigned long long)d->sectors * 512ULL;

    print_title("\n=== DISK ");
    putchar_os(d->letter);
    print(" ===\n\n");

    print("  Position:   ");
    print(disk_position_name(d));
    putchar_os('\n');

    print("  Model:      ");
    print(d->model);
    putchar_os('\n');

    print("  Sectors:    ");
    print_uint64(d->sectors);
    putchar_os('\n');

    print("  Bytes:      ");
    print_uint64(total);
    print(" B\n");

    print("  Kilobytes:  ");
    print_size_unit(total, SIZE_KB, 2, "KB");
    putchar_os('\n');

    print("  Megabytes:  ");
    print_size_unit(total, SIZE_MB, 2, "MB");
    putchar_os('\n');

    print("  Gigabytes:  ");
    print_size_unit(total, SIZE_GB, 2, "GB");
    putchar_os('\n');

    if (fs_drive_has_fs(idx))
    {
        unsigned int files = 0;
        unsigned long long used = fs_drive_used_bytes(idx, &files);
        unsigned int entries = fs_drive_entry_count(idx);
        unsigned long long overhead =
            (unsigned long long)(1 + FS_DIR_SECTORS + MAX_FILES * FS_FILE_SECTORS) * 512ULL;
        unsigned long long free = (total > overhead) ? (total - overhead) : 0;

        print("  Filesystem: ");
        print(OS_NAME);
        putchar_os('\n');

        print("  Entries:    ");
        print_uint64(entries);
        print(" (");
        print_uint64(files);
        print(" files)\n");

        print("  File data:  ");
        print_uint64(used);
        print(" B  (");
        print_size_unit(used, SIZE_KB, 2, "KB");
        print(")\n");

        print("  Overhead:   ");
        print_uint64(overhead);
        print(" B  (");
        print_size_unit(overhead, SIZE_KB, 2, "KB");
        print(")\n");

        print("  Free space: ");
        print_uint64(free);
        print(" B  (");
        print_size_unit(free, SIZE_MB, 2, "MB");
        print(")\n");
    }
    else
    {
        print("  Filesystem: none (use 'install' to create one)\n");
    }

    putchar_os('\n');
}
