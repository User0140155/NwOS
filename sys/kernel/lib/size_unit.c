#include "nwos_internal.h"

int read_int(void)
{
    char buffer[32];

    read_line(buffer, 32);

    return atoi_simple(buffer);
}

void print_int(int number)
{
    char buffer[12];
    int i = 0;

    if (number == 0)
    {
        putchar_os('0');
        return;
    }

    if (number < 0)
    {
        putchar_os('-');
        number = -number;
    }

    while (number > 0)
    {
        buffer[i] = '0' + (number % 10);
        number /= 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        putchar_os(buffer[i]);
    }
}

void print_uint64(unsigned long long value)
{
    char buffer[24];
    int i = 0;

    if (value == 0)
    {
        putchar_os('0');
        return;
    }

    while (value > 0 && i < 24)
    {
        buffer[i++] = (char)('0' + (int)(value % 10));
        value /= 10;
    }

    while (i > 0)
    {
        i--;
        putchar_os(buffer[i]);
    }
}

void print_size_unit(
    unsigned long long bytes,
    unsigned long long unit,
    int decimals,
    const char* suffix
)
{
    static const unsigned long long pow10[3] = { 1ULL, 10ULL, 100ULL };

    unsigned long long whole = bytes / unit;
    unsigned long long frac = 0;

    if (decimals > 0 && decimals <= 3)
    {
        frac = ((bytes % unit) * pow10[decimals - 1]) / unit;
    }

    print_uint64(whole);

    if (decimals > 0)
    {
        putchar_os('.');

        for (int i = decimals - 1; i >= 0; i--)
        {
            putchar_os((char)('0' + (int)((frac / pow10[i]) % 10)));
        }
    }

    print(" ");
    print(suffix);
}

void print_size_human(unsigned long long bytes)
{
    if (bytes >= SIZE_GB)
        print_size_unit(bytes, SIZE_GB, 2, "GB");
    else if (bytes >= SIZE_MB)
        print_size_unit(bytes, SIZE_MB, 2, "MB");
    else if (bytes >= SIZE_KB)
        print_size_unit(bytes, SIZE_KB, 2, "KB");
    else
    {
        print_uint64(bytes);
        print(" B");
    }
}
