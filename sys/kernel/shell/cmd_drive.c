#include "nwos_internal.h"

int resolve_drive_arg(const char* args)
{
    int n = ata_drive_count();

    if (args == 0 || args[0] == '\0') return fs_current_drive;

    char c = args[0];

    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {
        return fs_drive_index_by_letter(c);
    }

    if (c >= '0' && c <= '9')
    {
        int num = atoi_simple(args);

        if (num >= 1 && num <= n) return num - 1;
    }

    return -1;
}

void cmd_drive(const char* args)
{
    if (ata_drive_count() == 0)
    {
        print_error("No ATA disks detected.\n");
        return;
    }

    if (args == 0 || args[0] == '\0')
    {
        ata_drive_t* d = ata_get_drive(fs_current_drive);
        unsigned long long total = (unsigned long long)d->sectors * 512ULL;

        print_title("\n=== ACTIVE DISK ===\n\n");
        print("  Disk:   ");
        putchar_os(d->letter);
        print(" (");
        print(disk_position_name(d));
        print(")\n");
        print("  Model:  ");
        print(d->model);
        putchar_os('\n');
        print("  Bytes:  ");
        print_uint64(total);
        print(" B  (");
        print_size_human(total);
        print(")\n\n");

        print("Usage: drive <letter|number>\n");
        print("  drive D  - switch to the disk with the letter D\n");
        print("  drive 2  - switch to the second detected disk\n");
        putchar_os('\n');
        return;
    }

    int idx = resolve_drive_arg(args);

    if (idx < 0)
    {
        print_error("Unknown disk. Type 'disks' to see the list.\n");
        return;
    }

    if (idx == fs_current_drive)
    {
        print_success("Already on that disk (");
        putchar_os(fs_get_drive_letter());
        print(").\n");
        return;
    }

    int res = fs_change_drive(idx);

    if (res == -2)
    {
        print_error("No " OS_NAME " filesystem on that disk (use 'install').\n");
        return;
    }

    if (res != 0)
    {
        print_error("Cannot switch to that disk.\n");
        return;
    }

    ata_drive_t* d = ata_get_drive(fs_current_drive);

    print_success("Switched to ");
    putchar_os(d->letter);
    print(": (");
    print(disk_position_name(d));
    print(")\n");
    print("  New files are now created on ");
    putchar_os(d->letter);
    print(":\n");
}

void cmd_drive_status(void)
{
    cmd_drive("");
}
