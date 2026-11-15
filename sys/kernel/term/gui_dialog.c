#include "nwos_internal.h"

static char dialog_value[128];

static void dlg_copy(char* dst, const char* src, int max)
{
    int i = 0;
    if (!dst || max <= 0) return;
    if (src)
    {
        while (src[i] != '\0' && i < max - 1)
        {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static int dlg_len(const char* s)
{
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void dlg_frame(const char* title, const char* message, const char* value,
                      int confirm, int yes_hot, int no_hot)
{
    int x = 390;
    int y = 250;
    int w = 500;
    int h = 180;

    graphics_begin();
    fb_draw_wallpaper();
    fb_draw_glass(0, 0, 1280, 720, 0, 0x000000, 110);
    fb_draw_window(x, y, w, h, 0x002BA3E6, title);

    if (message && message[0])
        fb_draw_text(x + 22, y + 42, message, 0x001A3348);

    if (!confirm)
    {
        fb_draw_rect(x + 20, y + 72, w - 40, 32, 0x00FFFFFF);
        fb_draw_frame(x + 20, y + 72, w - 40, 32, 0x007FA9C4);
        fb_draw_text(x + 28, y + 83, value ? value : "", 0x000E3A5C);
        fb_draw_rect(x + 28 + dlg_len(value) * 8, y + 80, 7, 14, 0x002BA3E6);
        fb_draw_text(x + 22, y + 128, "Enter = OK    Esc = Cancel", 0x004F7896);
    }
    else
    {
        fb_draw_rect(x + 292, y + 116, 88, 30, yes_hot ? 0x009FD8F6 : 0x00D6E8F4);
        fb_draw_text(x + 324, y + 126, "Yes", 0x000E3A5C);
        fb_draw_rect(x + 392, y + 116, 88, 30, no_hot ? 0x009FD8F6 : 0x00D6E8F4);
        fb_draw_text(x + 424, y + 126, "No", 0x000E3A5C);
        fb_draw_text(x + 22, y + 128, "Y / Enter = Yes    N / Esc = No", 0x004F7896);
    }

    fb_present();
    fb_draw_cursor(mouse_x(), mouse_y());
}

const char* nwf_dialog_input(const char* prompt, const char* initial)
{
    int len;

    dialog_value[0] = '\0';
    dlg_copy(dialog_value, initial, sizeof(dialog_value));
    len = dlg_len(dialog_value);

    while (1)
    {
        dlg_frame(prompt ? prompt : "Input", "Enter a name:", dialog_value,
                  0, 0, 0);
        int key = keyboard_getkey();

        if (key == KEY_ENTER)
        {
            dialog_value[len] = '\0';
            return dialog_value;
        }
        if (key == KEY_ESC)
        {
            dialog_value[0] = '\0';
            return dialog_value;
        }
        if (key == KEY_BACKSPACE)
        {
            if (len > 0) dialog_value[--len] = '\0';
            continue;
        }
        if (key >= 32 && key <= 126 && len < (int)sizeof(dialog_value) - 1)
            dialog_value[len++] = (char)key;
    }
}

int nwf_dialog_confirm(const char* message)
{
    int last_btn = 0;

    while (1)
    {
        int mx = mouse_x();
        int my = mouse_y();
        int btn = mouse_buttons();
        int yes_hot = (mx >= 682 && mx <= 770 && my >= 366 && my <= 396);
        int no_hot = (mx >= 782 && mx <= 870 && my >= 366 && my <= 396);

        dlg_frame("Confirm", message, "", 1, yes_hot, no_hot);

        if (btn == 1 && last_btn == 0)
        {
            if (yes_hot) return 1;
            if (no_hot) return 0;
        }
        last_btn = btn;

        int key = keyboard_getkey();
        if (key == 'y' || key == 'Y' || key == KEY_ENTER) return 1;
        if (key == 'n' || key == 'N' || key == KEY_ESC) return 0;
    }
}

static void dlg_append(char* dst, int* pos, const char* src)
{
    int i = 0;
    if (!src) return;
    while (src[i] != '\0' && *pos < 100)
        dst[(*pos)++] = src[i++];
    dst[*pos] = '\0';
}

static void dlg_append_uint(char* dst, int* pos, unsigned int value)
{
    char tmp[16];
    int n = 0;
    if (value == 0)
    {
        dlg_append(dst, pos, "0");
        return;
    }
    while (value > 0 && n < 15)
    {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0)
    {
        dst[(*pos)++] = tmp[--n];
        dst[*pos] = '\0';
    }
}

static void dlg_info_frame(const char* title, const char* line1, const char* line2)
{
    int x = 390;
    int y = 250;
    int w = 500;
    int h = 180;

    graphics_begin();
    fb_draw_wallpaper();
    fb_draw_glass(0, 0, 1280, 720, 0, 0x000000, 110);
    fb_draw_window(x, y, w, h, 0x002BA3E6, title);
    if (line1 && line1[0]) fb_draw_text(x + 22, y + 46, line1, 0x001A3348);
    if (line2 && line2[0]) fb_draw_text(x + 22, y + 70, line2, 0x004F7896);
    fb_draw_text(x + 22, y + 128, "Enter / Esc = Close", 0x004F7896);
    fb_present();
    fb_draw_cursor(mouse_x(), mouse_y());
}

void nwf_dialog_info(const char* title, const char* line1, const char* line2)
{
    int last_btn = 0;
    while (1)
    {
        dlg_info_frame(title, line1, line2);
        int key = keyboard_getkey();
        int btn = mouse_buttons();
        if (key == KEY_ENTER || key == KEY_ESC) return;
        if (btn == 1 && last_btn == 0) return;
        last_btn = btn;
    }
}

void nwf_fs_props(int index)
{
    char line1[112];
    char line2[112];
    const char* name = 0;
    int type = 1;
    unsigned int size = 0;
    int p = 0;

    line1[0] = '\0';
    line2[0] = '\0';

    if (index < 0)
    {
        name = fs_current_path();
        type = 1;
    }
    else
    {
        fs_dir_entry(index, &name, &type, &size);
    }

    dlg_copy(line1, name ? name : "?", sizeof(line1));
    dlg_append(line2, &p, type == 1 ? "Folder" : "File");
    dlg_append(line2, &p, "    ");
    dlg_append_uint(line2, &p, size);
    dlg_append(line2, &p, " bytes");

    nwf_dialog_info("Properties", line1, line2);
}
