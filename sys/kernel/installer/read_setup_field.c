#include "nwos_internal.h"

static void installer_print_disks(int selected);
static void installer_setup_draw(const char* title, const char* prompt, const char* value, const char* hint);

static void installer_print_disks(int selected)
{
    int n = ata_drive_count();

    for (int i = 0; i < n; i++)
    {
        ata_drive_t* d = ata_get_drive(i);

        print("    ");

        if (selected == i) set_color(TERM_COLOR_LIGHT_GREEN);
        else set_color(base_color);

        print_int(i + 1);
        print(") ");

        putchar_os(d->letter);
        print(":  ");

        set_color(TERM_COLOR_WHITE);
        print(d->model);
        set_color(base_color);

        print("   ");
        print_size_bytes((unsigned long long)d->sectors * 512ULL);

        if (fs_drive_has_fs(i))
        {
            set_color(TERM_COLOR_LIGHT_RED);
            print("  [has " OS_NAME "]");
            set_color(base_color);
        }

        if (selected == i) set_color(TERM_COLOR_LIGHT_GREEN);
        else set_color(base_color);

        putchar_os('\n');
    }
}

static void installer_setup_draw(const char* title, const char* prompt,
                                  const char* value, const char* hint)
{
    int w = 700;
    int h = 500;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, title);
    gui_draw_text(x + 24, y + 55,
                  "Configure the account used by the installed system.",
                  COLOR_BLACK);
    gui_draw_text(x + 24, y + 125, prompt, COLOR_BLACK);
    gui_rect(x + 20, y + 155, w - 40, 42, COLOR_WHITE);
    gui_draw_text(x + 32, y + 168, value, COLOR_BLACK);

    if (hint && hint[0] != '\0')
        gui_draw_text(x + 24, y + 225, hint, COLOR_DARK_GRAY);

    gui_draw_text(x + 24, y + h - 42,
                  "Type to enter   Backspace: delete   Enter: continue   Esc: cancel",
                  COLOR_DARK_GRAY);
}

int installer_read_setup_field(const char* title,
                                      const char* prompt,
                                      char* output, int capacity,
                                      const char* hint)
{
    int length = 0;

    if (output == 0 || capacity < 2)
        return 0;

    output[0] = '\0';

    while (1)
    {
        installer_setup_draw(title, prompt, output, hint);

        int key = keyboard_getkey();
        if (key == KEY_ENTER)
        {
            output[length] = '\0';
            return 1;
        }
        if (key == KEY_ESC || key == 27)
        {
            output[0] = '\0';
            return 0;
        }
        if (key == KEY_BACKSPACE)
        {
            if (length > 0)
                output[--length] = '\0';
            continue;
        }
        if (key >= 32 && key <= 126 && length < capacity - 1)
        {
            output[length++] = (char)key;
            output[length] = '\0';
        }
    }
}
