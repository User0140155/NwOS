#define NWOS_INCLUDE_ZONES_H
#include "nwos_internal.h"

int timezone_offset = 0;
int custom_time_offset = 0;
volatile unsigned long long idle_us_accum = 0;
int use_custom_time = 0;
int custom_day = -1;
int custom_month = -1;
int custom_year = -1;
void cmd_mem(void)
{
    unsigned int  ram_mb  = nwf_ram_mb();
    unsigned long used_kb = nwf_ram_used_kb();
    unsigned long free_kb = nwf_ram_free_kb();
    int           percent = nwf_ram_used_percent();

    print_title("Memory Information:\n");

    print("  Total RAM: ");
    print_int((int)ram_mb);
    print(" MiB\n");

    print("  Used:      ");
    print_size_bytes((unsigned long long)used_kb * 1024ULL);
    print("  (");
    print_int(percent);
    print("%)\n");

    print("  Free:      ");
    print_size_bytes((unsigned long long)free_kb * 1024ULL);
    putchar_os('\n');

    print("  Memory model: 64-bit\n");
    putchar_os('\n');
}

void cmd_cpu(void)
{
    char cpu_name[49];
    nwf_cpu_name(cpu_name);
    print_title("CPU Information:\n");
    print("  CPU: ");
    print(cpu_name);
    putchar_os('\n');
    print("  Architecture: x86_64\n");
    putchar_os('\n');
}

void cmd_uptime(void)
{
    unsigned long long total_sec = timer_ms() / 1000;
    unsigned long long up_m = total_sec / 60;
    unsigned long long up_h = up_m / 60;
    up_m %= 60;
    total_sec %= 60;
    print_title("Uptime:\n");
    print("  ");
    if (up_h > 0) { print_int((int)up_h); print("h "); }
    if (up_m > 0) { print_int((int)up_m); print("m "); }
    print_int((int)total_sec); print("s\n");
    putchar_os('\n');
}

void cmd_zones(void)
{
    print_title("Time Zones (all 24h worldwide):\n");
    for (int i = 0; i < TIME_ZONE_COUNT; i++)
    {
        print_int(i + 1);
        print(". ");
        print(time_zone_names[i]);
        if (timezone_offset == time_zone_offsets[i])
            print(" [ACTIVE]");
        putchar_os('\n');
    }
    putchar_os('\n');
    print("Use 'timezone <num>' to set zone.\n");
    print("Use 'date <DD> <MM> <YYYY>' to set custom date.\n");
    print("Use 'date reset' to reset date to RTC.\n\n");
}

void cmd_timezone(const char* args)
{
    if (args[0] == '\0')
    {
        print("Usage: timezone <num> (1-");
        print_int(TIME_ZONE_COUNT);
        print(")\n");
        return;
    }
    int num = atoi_simple(args);
    if (num < 1 || num > TIME_ZONE_COUNT)
    {
        print_error("Invalid timezone number.\n");
        return;
    }
    timezone_offset = time_zone_offsets[num - 1];
    print_success("Timezone set to: ");
    print(time_zone_names[num - 1]);
    putchar_os('\n');
}

void cmd_date_set(const char* args)
{
    if (args[0] == '\0')
    {
        print("Usage: date <DD> <MM> <YYYY> or date reset\n");
        return;
    }
    if (strcmp(args, "reset") == 0)
    {
        custom_day = -1;
        custom_month = -1;
        custom_year = -1;
        print_success("Date reset to RTC.\n");
        return;
    }
    char d[16], m[16], y[16];
    int i = 0, j = 0;
    while (args[i] == ' ') i++;
    while (args[i] != ' ' && args[i] != '\0' && j < 15) { d[j++] = args[i++]; }
    d[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != ' ' && args[i] != '\0' && j < 15) { m[j++] = args[i++]; }
    m[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != '\0' && j < 15) { y[j++] = args[i++]; }
    y[j] = '\0';

    int dd = atoi_simple(d);
    int mm = atoi_simple(m);
    int yy = atoi_simple(y);

    if (dd < 1 || dd > 31 || mm < 1 || mm > 12 || yy < 0)
    {
        print_error("Invalid date.\n");
        return;
    }

    custom_day = dd;
    custom_month = mm;
    custom_year = yy;
    print_success("Date set to ");
    print_int(dd); print("."); print_int(mm); print("."); print_int(yy);
    print(" (OS will display this date).\n");
}
