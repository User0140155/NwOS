#include "nwos_internal.h"

void run_gui(void) {
    int saved_dir = current_dir;
    uint32_t saved_width = fb_get_width();
    uint32_t saved_height = fb_get_height();

    if (saved_width == 0 || saved_height == 0) {
        saved_width = NWOS_BOOT_WIDTH;
        saved_height = NWOS_BOOT_HEIGHT;
    }

    if (!fb_set_mode(NWOS_GUI_WIDTH, NWOS_GUI_HEIGHT)) {
        print_error("GUI: cannot switch video mode.\n");
        return;
    }

    mouse_set_bounds((int)fb_get_width(), (int)fb_get_height());

    current_dir = 0;
    fs_cd("bin");
    nwo_run("gui.nwo");
    current_dir = saved_dir;

    fb_set_mode(saved_width, saved_height);
    mouse_set_bounds((int)fb_get_width(), (int)fb_get_height());
    clear();
}

int boot_menu(int has_installed)
{
    static const char* items_inst[] = {
        "Boot installed " OS_NAME,
        "Live mode (runs in RAM, disks are not modified)",
        "Installer (use unallocated MBR space; existing partitions preserved)"
    };
    static const char* items_new[] = {
        "Live mode (runs in RAM, disks are not modified)",
        "Installer (use unallocated MBR space; existing partitions preserved)"
    };
    static const int acts_inst[] = {
        BOOT_ACTION_SYSTEM, BOOT_ACTION_LIVE, BOOT_ACTION_SETUP
    };
    static const int acts_new[] = {
        BOOT_ACTION_LIVE, BOOT_ACTION_SETUP
    };

    const char** items = has_installed ? items_inst : items_new;
    const int*   acts  = has_installed ? acts_inst  : acts_new;
    int count = has_installed ? 3 : 2;
    int sel   = 0;

    int w = 600;
    int h = 400;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;

    clear();

    while (1) {
        fb_clear(COLOR_BLUE);

        gui_draw_window(x, y, w, h, OS_NAME " " OS_VERSION_STR " Boot Menu");

        if (!has_installed) {
            gui_draw_text(x + 202, y + 580,
                        "copyright (c) User0140155 2026", COLOR_WHITE);
            gui_draw_text(x + 276, y + 595,
                        "NwOS", COLOR_WHITE);
            gui_draw_text(x + 20, y + 50, "No installed " OS_NAME " filesystem was found.", COLOR_RED);
            gui_draw_text(x + 20, y + 70, "Choose Live mode or start the installer.", COLOR_BLACK);
        } else {
            gui_draw_text(x + 215, y + 520,
                        "copyright (c) User0140155 2026", COLOR_WHITE);
            gui_draw_text(x + 300, y + 505,
                        "NwOS", COLOR_WHITE);
            gui_draw_text(x + 20, y + 50, "Use arrow keys to select, Enter to boot.", COLOR_BLACK);
        }

        for (int i = 0; i < count; i++) {
            int item_y = y + 120 + i * 40;

            if (i == sel) {
                gui_rect(x + 10, item_y - 5, w - 20, 30, COLOR_BLUE);
                gui_draw_text(x + 20, item_y, ">", COLOR_WHITE);
                gui_draw_text(x + 40, item_y, items[i], COLOR_WHITE);
            } else {
                gui_draw_text(x + 40, item_y, items[i], COLOR_BLACK);
            }
        }

        gui_draw_text(x + 20, y + h - 30, "Arrows: Move   Enter: Select   R: Reboot", COLOR_DARK_GRAY);

        int key = keyboard_getkey();

        if (key == KEY_UP)        sel = (sel + count - 1) % count;
        else if (key == KEY_DOWN) sel = (sel + 1) % count;
        else if (key == KEY_ENTER) {
            clear();
            return acts[sel];
        }
        else if (key == 27 || key == KEY_ESC || key == 'r' || key == 'R') {
            reboot();
        }
        else if (key >= '1' && key <= '9') {
            int idx = key - '1';
            if (idx < count) {
                clear();
                return acts[idx];
            }
        }
    }
}
