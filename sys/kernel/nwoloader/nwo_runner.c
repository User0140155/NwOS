#include "../nwo_loader.h"
#include "nwo_vm.h"
#include "nwo_native.h"
#include "../fb.h"
#include "../../../include/kernelpanic.h"

void graphics_begin(void);
void graphics_end(void);
void mouse_set_bounds(int width, int height);
void print(const char* text);
void print_int(int number);
void putchar_os(char c);

void print_error(const char* text);
void print_success(const char* text);

int fs_read_binary(const char* name, unsigned char* buffer, unsigned int max_size);
unsigned int fs_get_file_size(const char* name);

static int nwo_str_eq(const char* a, const char* b)
{
    if (!a || !b) return 0;
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int nwo_starts_with(const char* s, const char* p)
{
    if (!s || !p) return 0;
    while (*p) {
        if (*s++ != *p++) return 0;
    }
    return 1;
}

static const char* nwo_base_name(const char* filename)
{
    const char* base = filename ? filename : "";
    const char* p = base;
    while (*p) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
        ++p;
    }
    return base;
}

static int nwo_is_system_app(const char* filename)
{
    const char* base = nwo_base_name(filename);

    if (!filename) return 0;
    if (nwo_starts_with(filename, "/sys/") || nwo_starts_with(filename, "/bin/"))
        return 1;
    if (nwo_starts_with(filename, "sys/") || nwo_starts_with(filename, "bin/"))
        return 1;

    if (nwo_str_eq(base, "gui.nwo") ||
        nwo_str_eq(base, "testgui.nwo") ||
        nwo_str_eq(base, "shell.nwo") ||
        nwo_str_eq(base, "calculator.nwo"))
        return 1;

    return 0;
}

static int nwo_contains_bytes(const unsigned char* buf, unsigned int size, const char* needle)
{
    unsigned int nlen = 0;
    if (!buf || !needle) return 0;
    while (needle[nlen]) nlen++;
    if (nlen == 0 || size < nlen) return 0;
    for (unsigned int i = 0; i + nlen <= size; i++) {
        unsigned int j = 0;
        while (j < nlen && buf[i + j] == (unsigned char)needle[j]) j++;
        if (j == nlen) return 1;
    }
    return 0;
}

static int nwo_is_suspicious(const char* filename, const unsigned char* buf, unsigned int size)
{
    if (nwo_is_system_app(filename))
        return 0;
    if (!buf || size == 0)
        return 0;

    if (nwo_contains_bytes(buf, size, "/sys/kernel.elf") ||
        nwo_contains_bytes(buf, size, "kernel.elf") ||
        nwo_contains_bytes(buf, size, "EFI/BOOT/KERNEL.ELF") ||
        nwo_contains_bytes(buf, size, "format_disk") ||
        nwo_contains_bytes(buf, size, "fs_format") ||
        nwo_contains_bytes(buf, size, "wipefs") ||
        nwo_contains_bytes(buf, size, "MALWARE") ||
        nwo_contains_bytes(buf, size, "virus"))
        return 1;

    if (nwo_contains_bytes(buf, size, "/sys/") &&
        nwo_contains_bytes(buf, size, "delete"))
        return 1;

    return 0;
}

static void nwo_security_check(const char* filename)
{
    unsigned char header_scan[4096];
    unsigned int size = fs_get_file_size(filename);
    unsigned int read_size = size;

    if (nwo_is_system_app(filename))
        return;
    if (size == 0)
        return;

    if (read_size > sizeof(header_scan))
        read_size = sizeof(header_scan);

    for (unsigned int i = 0; i < read_size; i++)
        header_scan[i] = 0;

    if (!fs_read_binary(filename, header_scan, read_size))
        return;

    if (nwo_is_suspicious(filename, header_scan, read_size))
    {
        KPANIC_CRITICAL(ERR_SUSPICIOUS_APP,
            "Highly suspicious NWO application blocked before execution.");
    }
}

static int nwo_is_gui_name(const char* filename)
{
    const char* base = nwo_base_name(filename);

    return base[0] == 'g' && base[1] == 'u' && base[2] == 'i' &&
           base[3] == '.' && base[4] == 'n' && base[5] == 'w' &&
           base[6] == 'o' && base[7] == '\0';
}

int nwo_program_is_gui(const char* filename)
{
    unsigned char header[8];

    if (filename == 0 || filename[0] == '\0')
        return 0;
    if (nwo_is_gui_name(filename))
        return 1;

    for (int i = 0; i < 8; i++)
        header[i] = 0;

    if (!fs_read_binary(filename, header, sizeof(header)))
        return 0;

    if (header[0] != 'N' || header[1] != 'W' ||
        header[2] != 'O' || header[3] != 'B')
        return 0;

    return (header[5] & 0x02) != 0;
}

int nwo_default_window(const char* filename, const char* title);

void nwo_run(const char* filename)
{
    NwoProgram program;
    int result;
    int use_gui_canvas;
    uint32_t saved_canvas_width;
    uint32_t saved_canvas_height;

    if (filename == 0 || filename[0] == '\0')
    {
        print_error("Usage: run <file.nwo>\n");
        return;
    }

    nwo_security_check(filename);

    use_gui_canvas = nwo_program_is_gui(filename) && fb_is_available();
    saved_canvas_width = fb_get_width();
    saved_canvas_height = fb_get_height();

    if (use_gui_canvas) {
        if (!fb_set_canvas_mode(1280, 720)) {
            print_error("GUI: logical 1280x720 canvas is unavailable.\n");
            return;
        }
        mouse_set_bounds((int)fb_get_width(), (int)fb_get_height());
    }

    if (use_gui_canvas)
    {
        /* Run GUI program inside its own window while keeping caller context. */
        int rc = nwo_default_window(filename, filename);
        fb_set_canvas_mode(saved_canvas_width, saved_canvas_height);
        fb_clear(0x000000);
        mouse_set_bounds((int)fb_get_width(), (int)fb_get_height());
        if (rc < 0) print_error("\nNWO program failed.\n");
        return;
    }

    print("NWO: loading ");
    print(filename);
    print("...\n");

    if (nwo_native_try_run(filename))
    {
        graphics_end();
        return;
    }

    if (!nwo_load(filename, &program))
    {
        print_error("Cannot load NWO program: ");
        print(nwo_load_error());
        putchar_os('\n');
        return;
    }

    print("NWO: executing...\n\n");

    result = nwo_execute(&program);

    nwo_unload(&program);
    graphics_end();
    if (use_gui_canvas) {
        fb_set_canvas_mode(saved_canvas_width, saved_canvas_height);
        fb_clear(0x000000);
        mouse_set_bounds((int)fb_get_width(), (int)fb_get_height());
    }

    if (result < 0)
    {
        print_error("\nNWO program failed.\n");
        return;
    }

    print("\n");
    print_success("Program finished. Return code: ");
    print_int(result);
    putchar_os('\n');
}

int nwo_run_console(const char* filename)
{
    NwoProgram program;
    int result;

    if (filename == 0 || filename[0] == '\0')
        return -1;

    nwo_security_check(filename);

    print("NWO: loading ");
    print(filename);
    print("...\n");

    if (!nwo_load(filename, &program))
    {
        print_error("Cannot load NWO program: ");
        print(nwo_load_error());
        putchar_os('\n');
        return -1;
    }

    print("NWO: executing (Ctrl+C stops)...\n\n");
    result = nwo_execute(&program);
    nwo_unload(&program);
    return result;
}

int nwo_run_in_window(const char* filename, int x, int y, int w, int h)
{
    NwoProgram program;
    int result;
    int cancelled;
    int cx;
    int cy;
    int cw;
    int ch;
    int is_gui;

    if (filename == 0 || filename[0] == '\0')
        return -1;

    nwo_security_check(filename);

    if (!fb_is_available())
        return nwo_run_console(filename);

    is_gui = nwo_program_is_gui(filename);
    if (is_gui)
    {
        /* GUI-spawn programs manage their own window/widgets. */
        if (!nwo_load(filename, &program))
            return -1;
        nwo_vm_cancel_begin();
        result = nwo_execute(&program);
        nwo_unload(&program);
        nwo_vm_cancel_end();
        return result;
    }

    cx = x + 4;
    cy = y + 26;
    cw = w - 8;
    ch = h - 30;

    graphics_begin();
    fb_draw_window(x, y, w, h, 0x002BA3E6, filename);

    if (!fb_subsurface_begin(cx, cy, cw, ch))
        return -1;

    nwo_vm_window_begin(cx, cy, cw, ch);
    nwo_vm_cancel_begin();

    if (!nwo_load(filename, &program))
    {
        nwo_vm_cancel_end();
        nwo_vm_window_end();
        fb_subsurface_end();
        fb_draw_text(x + 20, y + 46, "Cannot load NWO program.", 0x00FF8F8F);
        fb_draw_text(x + 20, y + 66, nwo_load_error(), 0x00EAF6FF);
        return -1;
    }

    result = nwo_execute(&program);
    cancelled = nwo_vm_cancelled();
    nwo_unload(&program);

    nwo_vm_cancel_end();
    nwo_vm_window_end();
    fb_subsurface_end();

    if (cancelled)
        fb_draw_text(cx + 12, cy + 12, "Stopped by Ctrl+C.", 0x00FFD07A);

    return result;
}

int nwo_default_window(const char* filename, const char* title)
{
    (void)title;
    return nwo_run_in_window(filename, 140, 30, 1000, 590);
}
