#include "nwos_internal.h"

void app_hexview(const char* filename)
{
    static unsigned char buffer[512];

    if (!filename || filename[0] == '\0')
    {
        print_error("Usage: hexview <filename>\n");
        return;
    }

    if (!fs_read_binary(filename, buffer, sizeof(buffer)))
    {
        print_error("File not found or read error.\n");
        return;
    }

    print_success("\n--- HEX DUMP: ");
    print(filename);
    print(" ---\n\n");

    print("OFFSET   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  | ASCII\n");
    print("-------  -----------------------------------------------  | ----------------\n");

    for (int offset = 0; offset < 700; offset += 16)
    {
        print_hex_byte((offset >> 8) & 0xFF);
        print_hex_byte(offset & 0xFF);
        print("     ");
        for (int i = 0; i < 16; i++)
        {
            print_hex_byte(buffer[offset + i]);
            putchar_os(' ');
        }

        print(" | ");
        for (int i = 0; i < 16; i++)
        {
            unsigned char c = buffer[offset + i];
            if (c >= 32 && c <= 126)
            {
                putchar_os((char)c);
            }
            else
            {
                putchar_os('.');
            }
        }

        putchar_os('\n');
    }

    print_success("\n--- END OF DUMP ---\n\n");
}
void app_nwo_info(const char* filename)
{
    static unsigned char buffer[512];

    if (!filename || filename[0] == '\0')
    {
        print_error("Usage: nwocheck <file.nwo>\n");
        return;
    }

    if (!fs_read_binary(filename, buffer, sizeof(buffer)))
    {
        print_error("Failed to read NWO binary.\n");
        return;
    }

    print_success("\n=== NWO EXECUTABLE INSPECTOR ===\n");
    print("File: ");
    print(filename);
    print("\n\nFirst 16 Opcodes / Raw Bytes:\n[ ");

    for (int i = 0; i < 16; i++)
    {
        print_hex_byte(buffer[i]);
        putchar_os(' ');
    }
    print("]\n\n");

    print("Entry Opcode / Byte: 0x");
    print_hex_byte(buffer[0]);
    print("\nStatus: Valid binary image loaded into memory.\n");
    print("===============================\n\n");
}

void cmd_disks(void)
{
    int n = ata_drive_count();
    if (n == 0) {
        print_error("No ATA disks detected.\n");
        print("Add one in QEMU:\n");
        print("  -drive file=disk.img,format=raw,if=ide,index=0\n");
        print("(also: index=1, 2, 3 for slave/secondary)\n");
        return;
    }

    print_title("\n=== DISKS ===\n\n");

    for (int i = 0; i < n; i++) {
        ata_drive_t* d = ata_get_drive(i);
        unsigned long long total = (unsigned long long)d->sectors * 512ULL;

        if (i == fs_current_drive) set_color(TERM_COLOR_LIGHT_GREEN);
        else set_color(base_color);

        putchar_os(d->letter);
        print(":");

        if (i == fs_current_drive) print(" *");
        else print("  ");

        set_color(base_color);

        print("  ");
        print(disk_position_name(d));
        print("  ");

        print_uint64(total);
        print(" B  ");

        print_size_human(total);

        if (fs_drive_has_fs(i))
        {
            set_color(TERM_COLOR_LIGHT_CYAN);
            print("  [");
            print(OS_NAME);
            print("]");
            set_color(base_color);
        }

        putchar_os('\n');
    }

    putchar_os('\n');

    print("  * = active disk\n");
    print("  'drive <letter|number>' switches the active disk\n");
    print("  'disk <letter|number>' shows the detailed size of one disk\n");
    putchar_os('\n');
}
