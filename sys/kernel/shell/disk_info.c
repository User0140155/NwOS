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
        fs_usage_t usage;

        print("  Filesystem: ");
        print(OS_NAME);
        putchar_os('\n');

        if (!fs_drive_usage(idx, &usage))
        {
            print_error("  Cannot read filesystem usage.\n");
            putchar_os('\n');
            return;
        }

        print("  Entries:    ");
        print_uint64(usage.entries);
        print(" (");
        print_uint64(usage.files);
        print(" files)\n");

        print("  Volume:     ");
        print_size_human(usage.volume_bytes);
        putchar_os('\n');

        print("  File data:  ");
        print_size_human(usage.file_bytes);
        putchar_os('\n');

        print("  Allocated:  ");
        print_size_human(usage.used_bytes);
        print("  (");
        print_int((int)usage.used_percent);
        print("% used)\n");

        print("  Free:       ");
        print_size_human(usage.free_bytes);
        putchar_os('\n');
    }
    else
    {
        print("  Filesystem: none (use 'install' to create one)\n");
    }

    putchar_os('\n');
}
