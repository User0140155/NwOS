#include "nwos_internal.h"

static void nwf_cpuid(unsigned int leaf, unsigned int* a, unsigned int* b, unsigned int* c, unsigned int* d);

void cmd_time_cmd(void)
{
    int hour, min, sec;
    int day, month, year;
    get_time(&hour, &min, &sec);
    get_date(&day, &month, &year);
    print_title("Current Time/Date:\n");
    print("  Time: ");
    print_two_digits(hour); putchar_os(':');
    print_two_digits(min); putchar_os(':');
    print_two_digits(sec);
    putchar_os('\n');
    print("  Date: ");
    print_two_digits(day); putchar_os('.');
    print_two_digits(month); putchar_os('.');
    print_int(year);
    putchar_os('\n');
    print("  Zone: UTC");
    if (timezone_offset >= 0) putchar_os('+');
    print_int(timezone_offset);
    putchar_os('\n');
    if (custom_day != -1)
        print("  (Custom date active)\n");
    putchar_os('\n');
}

void get_time(int* hour, int* minute, int* second) {
    if (use_custom_time) {

    }
    while (rtc_read(0x0A) & 0x80);

    *second = bcd_to_bin(rtc_read(0x00));
    *minute = bcd_to_bin(rtc_read(0x02));
    *hour   = bcd_to_bin(rtc_read(0x04));

    int total = *hour + timezone_offset + custom_time_offset;
    while (total < 0) total += 24;
    while (total >= 24) total -= 24;
    *hour = total;
}

void get_date(int* day, int* month, int* year) {
    while (rtc_read(0x0A) & 0x80);

    if (custom_day != -1 && custom_month != -1 && custom_year != -1) {
        *day = custom_day;
        *month = custom_month;
        *year = custom_year;
        return;
    }

    *day   = bcd_to_bin(rtc_read(0x07));
    *month = bcd_to_bin(rtc_read(0x08));
    *year  = bcd_to_bin(rtc_read(0x09)) + 2000;
}

static void nwf_cpuid(unsigned int leaf, unsigned int* a, unsigned int* b,
                      unsigned int* c, unsigned int* d)
{
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(leaf), "c"(0));
}

void nwf_cpu_name(char* out)
{
    unsigned int a, b, c, d;

    nwf_cpuid(0x80000000, &a, &b, &c, &d);

    if (a >= 0x80000004)
    {
        unsigned int regs[12];

        for (int i = 0; i < 3; i++)
        {
            nwf_cpuid(0x80000002 + i,
                      &regs[i * 4], &regs[i * 4 + 1],
                      &regs[i * 4 + 2], &regs[i * 4 + 3]);
        }

        memcpy(out, regs, 48);
        out[48] = '\0';

        int s = 0;
        while (out[s] == ' ') s++;
        if (s)
        {
            int i = 0;
            while (out[s + i]) { out[i] = out[s + i]; i++; }
            out[i] = '\0';
        }

        int len = 0;
        while (out[len]) len++;
        while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
    }
    else
    {
        nwf_cpuid(0, &a, &b, &c, &d);
        memcpy(out,     &b, 4);
        memcpy(out + 4, &d, 4);
        memcpy(out + 8, &c, 4);
        out[12] = '\0';
    }
}
