#include "nwos_internal.h"

void nwfetch(void)
{
    int hour, min, sec;
    int day, month, year;
    get_time(&hour, &min, &sec);
    get_date(&day, &month, &year);

    unsigned long long total_sec = timer_ms() / 1000;
    unsigned long long up_m = total_sec / 60;
    unsigned long long up_h = up_m / 60;
    up_m %= 60;
    total_sec %= 60;

    char cpu_name[49];
    nwf_cpu_name(cpu_name);

    unsigned int ram_mb = nwf_ram_mb();
    fs_usage_t disk_usage;
    int have_disk_usage = fs_usage_current(&disk_usage);

    static const char* logo[15] =
    {
        "         /\\         ",
        "        /  \\        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "   ================ ",
        "        |  |        ",
        "        |  |        ",
        "        (  )        ",
        "                    "
    };

    static const unsigned char logo_color[15] =
    {
        TERM_COLOR_WHITE, TERM_COLOR_WHITE, TERM_COLOR_LIGHT_CYAN, TERM_COLOR_LIGHT_CYAN,
        TERM_COLOR_LIGHT_CYAN, TERM_COLOR_LIGHT_CYAN, TERM_COLOR_LIGHT_CYAN, TERM_COLOR_LIGHT_CYAN,
        TERM_COLOR_LIGHT_CYAN, TERM_COLOR_LIGHT_CYAN, TERM_COLOR_YELLOW, TERM_COLOR_BROWN,
        TERM_COLOR_BROWN, TERM_COLOR_YELLOW, TERM_COLOR_BLACK
    };

    static const unsigned char palette[8] =
    {
        TERM_COLOR_BLACK, TERM_COLOR_RED, TERM_COLOR_GREEN, TERM_COLOR_YELLOW,
        TERM_COLOR_BLUE, TERM_COLOR_MAGENTA, TERM_COLOR_CYAN, TERM_COLOR_WHITE
    };

    print("\n");

    for (int row = 0; row < 15; row++)
    {
        set_color(logo_color[row]);
        print(logo[row]);
        print("  ");

        switch (row)
        {
            case 0:
                set_color(TERM_COLOR_CYAN);  print(cfg_user);
                set_color(TERM_COLOR_WHITE); print("@");
                set_color(TERM_COLOR_CYAN);  print(cfg_hostname);
                break;

            case 1:
                set_color(TERM_COLOR_LIGHT_GRAY);
                print("----------------------");
                break;

            case 2:
                set_color(TERM_COLOR_CYAN);  print("OS: ");
                set_color(TERM_COLOR_WHITE); print(cfg_os); print(" "); print(OS_VERSION_STR); print(" x86_64");
                break;

            case 3:
                set_color(TERM_COLOR_CYAN);  print("Kernel: ");
                set_color(TERM_COLOR_WHITE); print(OS_NAME " 64-bit");
                break;

            case 4:
                set_color(TERM_COLOR_CYAN);  print("Uptime: ");
                set_color(TERM_COLOR_WHITE);
                if (up_h > 0) { print_int((int)up_h); print("h "); }
                if (up_m > 0) { print_int((int)up_m); print("m "); }
                print_int((int)total_sec); print("s");
                break;

            case 5:
                set_color(TERM_COLOR_CYAN);  print("Shell: ");
                set_color(TERM_COLOR_WHITE); print("nwsh");
                break;

            case 6:
                set_color(TERM_COLOR_CYAN);  print("Display: ");
                set_color(TERM_COLOR_WHITE); print("VGA Text 80x50");
                break;

            case 7:
                set_color(TERM_COLOR_CYAN);  print("GPU: ");
                set_color(TERM_COLOR_WHITE); nwf_print_gpu();
                break;

            case 8:
                set_color(TERM_COLOR_CYAN);  print("Terminal: ");
                set_color(TERM_COLOR_WHITE); print(OS_NAME " Console");
                break;

            case 9:
                set_color(TERM_COLOR_CYAN);  print("CPU: ");
                set_color(TERM_COLOR_WHITE); print(cpu_name);
                break;

            case 10:
                set_color(TERM_COLOR_CYAN);  print("Memory: ");
                set_color(TERM_COLOR_WHITE);
                print_int((int)(nwf_ram_used_kb() / 1024UL));
                print(" / ");
                print_int((int)ram_mb);
                print(" MiB (");
                print_int(nwf_ram_used_percent());
                print("%)");
                break;

            case 11:
                set_color(TERM_COLOR_CYAN);  print("Network: ");
                set_color(TERM_COLOR_WHITE); print("e1000");
                break;

            case 12:
                set_color(TERM_COLOR_CYAN);  print("IP: ");
                set_color(TERM_COLOR_WHITE); print("10.0.2.15");
                break;

            case 13:
                set_color(TERM_COLOR_CYAN);  print("Disk: ");
                set_color(TERM_COLOR_WHITE);
                if (have_disk_usage)
                {
                    print_int((int)disk_usage.used_percent);
                    print("% used, ");
                    print_size_human(disk_usage.free_bytes);
                    print(" free of ");
                    print_size_human(disk_usage.capacity_bytes);
                }
                else
                {
                    print("unavailable");
                }
                break;

            case 14:
                set_color(TERM_COLOR_CYAN);  print("Date: ");
                set_color(TERM_COLOR_WHITE);
                print_two_digits(day);   putchar_os('.');
                print_two_digits(month); putchar_os('.');
                print_int(year);         putchar_os(' ');
                print_two_digits(hour);  putchar_os(':');
                print_two_digits(min);   putchar_os(':');
                print_two_digits(sec);
                break;
        }

        putchar_os('\n');
    }

    print("                      ");
    for (int i = 0; i < 8; i++)
    {
        set_color(palette[i]);
        print("##");
    }
    putchar_os('\n');

    set_color(base_color);
    print("\n");
}
