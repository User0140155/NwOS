#include "nwos_internal.h"

void print_two_digits(int value) {
    if (value < 10) putchar_os('0');
    print_int(value);
}

void print_hex_byte(unsigned char b)
{
    const char hex[] = "0123456789ABCDEF";
    putchar_os(hex[(b >> 4) & 0x0F]);
    putchar_os(hex[b & 0x0F]);
}

void format_size_buf(unsigned long long bytes, char* buf, int max_len) {
    int len = 0;
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        unsigned long long gb = bytes / (1024ULL * 1024ULL * 1024ULL);
        char tmp[24]; int t = 0;
        if (gb == 0) tmp[t++] = '0';
        else { while (gb > 0) { tmp[t++] = '0' + (gb % 10); gb /= 10; } }
        for (int i = t - 1; i >= 0 && len < max_len - 4; i--) buf[len++] = tmp[i];
        buf[len++] = ' '; buf[len++] = 'G'; buf[len++] = 'B';
    } else {
        unsigned long long mb = bytes / (1024ULL * 1024ULL);
        if (mb == 0) mb = 1;
        char tmp[24]; int t = 0;
        if (mb == 0) tmp[t++] = '0';
        else { while (mb > 0) { tmp[t++] = '0' + (mb % 10); mb /= 10; } }
        for (int i = t - 1; i >= 0 && len < max_len - 4; i--) buf[len++] = tmp[i];
        buf[len++] = ' '; buf[len++] = 'M'; buf[len++] = 'B';
    }
    buf[len] = '\0';
}

void print_size_bytes(unsigned long long bytes)
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
        print(" bytes");
    }
}
