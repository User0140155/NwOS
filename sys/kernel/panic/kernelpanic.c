#include "../../../include/kernelpanic.h"

#include "../kernel.h"
#include "../../../drivers/keyboard.h"
#include "nwos_internal.h"

void reboot(void);

#include "../fb.h" 

extern void print_int(int number);
extern void putchar_os(char c);

#define COLOR_YELLOW     14
#define COLOR_WHITE      15

#define PANIC_COLOR      0x1F
#define PANIC_COLOR_CRIT 0x4F

static int p_x = 0;
static int p_y = 0;
static int p_cols = 80;
static int p_rows = 25;
static unsigned char panic_attr = 0x1F;

static void panic_update_metrics(void) {
    if (fb_is_available()) {
        p_cols = (int)(fb_get_width() / 8);
        p_rows = (int)(fb_get_height() / 8);
    } else {
        p_cols = 80;
        p_rows = 25;
    }
    if (p_cols < 40) p_cols = 40;
    if (p_rows < 15) p_rows = 15;
}

static void panic_clear_attr(unsigned char attr) {
    panic_attr = attr;
    panic_update_metrics();
    p_x = 0;
    p_y = 0;

    if (fb_is_available()) {
        for (int y = 0; y < p_rows; y++) {
            for (int x = 0; x < p_cols; x++) {
                fb_draw_char(x, y, ' ', panic_attr);
            }
        }
    }
}

static void panic_clear(void) {
    panic_clear_attr(PANIC_COLOR);
}

static void panic_putc(char c) {
    if (c == '\n') {
        p_x = 0;
        p_y++;
        return;
    }

    if (fb_is_available() && p_y < p_rows && p_x < p_cols) {
        fb_draw_char(p_x, p_y, c, panic_attr);
    }

    p_x++;
    if (p_x >= p_cols) {
        p_x = 0;
        p_y++;
    }
}

static void panic_puts(const char* s) {
    while (*s) {
        panic_putc(*s++);
    }
}

static void panic_hex(unsigned int v) {
    const char* hex = "0123456789ABCDEF";
    panic_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        panic_putc(hex[(v >> i) & 0xF]);
    }
}

static void panic_int(int v) {
    char buf[12];
    int i = 0;
    if (v == 0) { panic_putc('0'); return; }
    if (v < 0) { panic_putc('-'); v = -v; }
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i > 0) panic_putc(buf[--i]);
}

static void panic_hline(void) {
    int w = p_cols - 4;
    if (w > 76) w = 76;
    if (w < 20) w = 20;
    panic_puts("  +");
    for (int i = 0; i < w; i++) panic_putc('=');
    panic_puts("+\n");
}

static void panic_hline_pad(const char* text) {
    int w = p_cols - 4;
    if (w > 76) w = 76;
    if (w < 20) w = 20;
    int len = 0;
    while (text[len]) len++;
    int left = (w - len) / 2;
    if (left < 0) left = 0;
    int right = w - len - left;
    if (right < 0) right = 0;
    panic_puts("  |");
    for (int i = 0; i < left; i++) panic_putc(' ');
    panic_puts(text);
    for (int i = 0; i < right; i++) panic_putc(' ');
    panic_puts("|\n");
}

const char* kernel_panic_code_name(unsigned int code) {
    switch (code) {
        case ERR_NONE:             return "NO_ERROR";
        case ERR_DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
        case ERR_DEVICE_TIMEOUT:   return "DEVICE_TIMEOUT";
        case ERR_DISK_READ:        return "DISK_READ_ERROR";
        case ERR_DISK_WRITE:       return "DISK_WRITE_ERROR";
        case ERR_FS_CORRUPT:       return "FILESYSTEM_CORRUPTED";
        case ERR_OUT_OF_MEMORY:    return "OUT_OF_MEMORY";
        case ERR_CPU_EXCEPTION:    return "CPU_EXCEPTION";
        case ERR_DIVIDE_BY_ZERO:   return "DIVIDE_BY_ZERO";
        case ERR_PAGE_FAULT:       return "PAGE_FAULT";
        case ERR_BAD_STATE:        return "BAD_KERNEL_STATE";
        case ERR_TRIPLE_FAULT:     return "TRIPLE_FAULT";
        case ERR_GUI_LAG:          return "GUI_LAG";
        case ERR_VIRUS:            return "MALWARE_DETECTED";
        case ERR_SYSTEM_MISSING:   return "SYSTEM_COMPONENT_MISSING";
        case ERR_SUSPICIOUS_APP:   return "SUSPICIOUS_APPLICATION";
        case DISK_DEATH:           return "DISK_FAILURE";
        case ERR_DEATH:            return "CRITICAL_SYSTEM_DESTRUCTION";
        case ERR_UNKNOW:           return "UNKNOWN_ERROR";
        default:                   return "UNSPECIFIED_ERROR";
    }
}

static int panic_is_critical(unsigned int code, PanicLevel level) {
    if (level == PANIC_CRITICAL) return 1;
    return code == ERR_DEATH ||
           code == DISK_DEATH ||
           code == ERR_TRIPLE_FAULT ||
           code == ERR_SYSTEM_MISSING ||
           code == ERR_VIRUS ||
           code == ERR_SUSPICIOUS_APP;
}

static void panic_fatal(const char* message, unsigned int code, const char* file, int line, PanicLevel level)
{
    __asm__ volatile ("cli");

    panic_clear_attr(panic_is_critical(code, level) ? PANIC_COLOR_CRIT : PANIC_COLOR);

    panic_puts("\n");
    panic_hline();
    panic_hline_pad("");
    if (panic_is_critical(code, level))
        panic_hline_pad("!!! CRITICAL KERNEL PANIC !!!");
    else
        panic_hline_pad("!!! KERNEL PANIC !!!");
    panic_hline_pad("");
    panic_hline();
    panic_puts("\n");

    panic_puts("   ERROR:       ");
    panic_puts(message ? message : "(no message)");
    panic_puts("\n\n");

    panic_puts("   ERROR CODE:  ");
    panic_puts(kernel_panic_code_name(code));
    panic_puts("\n");

    panic_puts("   NUM CODE:    ");
    panic_hex(code);
    panic_puts("\n\n");

    panic_puts("   AT:          ");
    panic_puts(file ? file : "(unknown)");
    panic_puts(":");
    panic_int(line);
    panic_puts("\n\n");

    panic_hline();
    panic_puts("\n");

    if (code == ERR_DEATH) {
        panic_puts("        * You just deleted the root filesystem. Why? *\n\n");
    }
    if (code == ERR_SUSPICIOUS_APP || code == ERR_VIRUS) {
        panic_puts("        * A highly suspicious application was blocked. *\n\n");
    }

    panic_puts("        The system has halted to prevent data corruption.\n\n");
    panic_puts("        If this is the first time you have seen this screen,\n");
    panic_puts("        restart your computer (power button or press R).\n");
    panic_puts("        If the panic appears again, try the following:\n");
    panic_puts("        1. Check your memory (600M+ recommended).\n");
    panic_puts("        2. Check your hardware and system files.\n");
    panic_puts("        3. Open an issue and explain what happened.\n\n");
    panic_puts("        Contact: TikTok (@csplayer421)\n");
    panic_puts("        Issues:  https://github.com/User0140155/NwOS/\n\n");

    panic_puts("        * Press Ctrl+Alt+Del in QEMU to restart. *\n");
    panic_puts("        * Press the power button or R to restart on real hardware. *\n");

    for (;;)
    {
        int k = keyboard_getkey();
        if (k == 'r' || k == 'R') {
            reboot();
        }
        __asm__ volatile ("hlt");
    }
}

void unknow_panic(unsigned int code) {
    print("\nUnknown error\nERROR CODE: ");
    print(kernel_panic_code_name(code));
    print("\nNUM CODE: ");
    print_int((int)code);
    print("\n");
    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

void kernel_panic(const char* message, unsigned int code, const char* file, int line, PanicLevel level)
{
    if (level == PANIC_FATAL || level == PANIC_CRITICAL)
    {
        panic_fatal(message, code, file, line, level);
    }
    else
    {
        extern void set_color(unsigned char);
        extern void set_base_color(unsigned char);
        extern void print(const char*);
        set_color(COLOR_YELLOW);
        print("\n[!] KERNEL WARNING: ");
        set_color(COLOR_WHITE);
        print(message ? message : "(no message)");
        print(" (");
        print(kernel_panic_code_name(code));
        print(", ");
        print_int((int)code);
        print(")\n");
        set_base_color(7);
    }
}
