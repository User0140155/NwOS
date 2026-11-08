#include "nwos_internal.h"

int installer_read_secret_field(const char* title,
                                       const char* prompt,
                                       char* output, int capacity,
                                       const char* hint)
{
    int length = 0;
    char mask[64];

    if (output == 0 || capacity < 2 || capacity > (int)sizeof(mask))
        return 0;

    output[0] = '\0';
    mask[0] = '\0';

    while (1)
    {
        int w = 700;
        int h = 500;
        int x = ((int)fb_get_width() - w) / 2;
        int y = ((int)fb_get_height() - h) / 2;

        for (int i = 0; i < (int)sizeof(mask); ++i)
            mask[i] = '\0';
        for (int i = 0; i < length; ++i)
            mask[i] = '*';
        mask[length] = '\0';

        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, title);
        gui_draw_text(x + 24, y + 55,
                      "Password characters are never displayed.", COLOR_BLACK);
        gui_draw_text(x + 24, y + 125, prompt, COLOR_BLACK);
        gui_rect(x + 20, y + 155, w - 40, 42, COLOR_WHITE);
        gui_draw_text(x + 32, y + 168, mask, COLOR_BLACK);
        if (hint && hint[0] != '\0')
            gui_draw_text(x + 24, y + 225, hint, COLOR_DARK_GRAY);
        gui_draw_text(x + 24, y + h - 42,
                      "Type to enter   Backspace: delete   Enter: continue   Esc: cancel",
                      COLOR_DARK_GRAY);

        int key = keyboard_getkey();
        if (key == KEY_ENTER)
        {
            output[length] = '\0';
            wipe_secret(mask, sizeof(mask));
            return 1;
        }
        if (key == KEY_ESC || key == 27)
        {
            wipe_secret(output, capacity);
            wipe_secret(mask, sizeof(mask));
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

void installer_setup_error(const char* message)
{
    int w = 700;
    int h = 500;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Account setup - Invalid value");
    gui_draw_text(x + 24, y + 90, message, COLOR_RED);
    gui_draw_text(x + 24, y + h - 42, "Press any key to try again.", COLOR_DARK_GRAY);
    (void)keyboard_getkey();
}

int installer_choose_login_requirement(void)
{
    int w = 700;
    int h = 500;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;

    while (1)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Account setup - Startup login");
        gui_draw_text(x + 24, y + 75,
                      "Require the account password every time NwOS starts?",
                      COLOR_BLACK);
        gui_draw_text(x + 40, y + 145,
                      "Y  Require login (recommended)", COLOR_GREEN);
        gui_draw_text(x + 40, y + 185,
                      "N  Boot directly to the shell", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42,
                      "Press Y or N   Esc: cancel installation",
                      COLOR_DARK_GRAY);

        int key = keyboard_getkey();
        if (key == 'y' || key == 'Y' || key == KEY_ENTER)
        {
            cfg_require_login = 1;
            return 1;
        }
        if (key == 'n' || key == 'N')
        {
            cfg_require_login = 0;
            return 1;
        }
        if (key == KEY_ESC || key == 27)
            return 0;
    }
}
