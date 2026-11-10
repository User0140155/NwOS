#include "graphics.h"
#include "fb.h"

void gui_init(void) {
    fb_clear(COLOR_BLUE);
}

void gui_putpixel(int x, int y, uint32_t color) {
    fb_draw_rect(x, y, 1, 1, color);
}

void gui_rect(int x, int y, int w, int h, uint32_t color) {
    fb_draw_rect(x, y, w, h, color);
}

void gui_frame(int x, int y, int w, int h, uint32_t color) {
    fb_draw_frame(x, y, w, h, color);
}

void gui_draw_text(int x, int y, const char* text, uint32_t color) {
    fb_draw_text(x, y, text, color);
}

void gui_draw_window(int x, int y, int w, int h, const char* title) {
    gui_rect(x + 24, y + 24, w, h, 0x00011111); // shadow
    // gui_rect(x + 14, y + 14, w, h, 0x00111111); // previous
    
    gui_rect(x, y, w, h, COLOR_LIGHT_GRAY);

    fb_draw_line(x, y, x + w - 1, y, COLOR_WHITE);
    fb_draw_line(x, y, x, y + h - 1, COLOR_WHITE);
    fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, COLOR_DARK_GRAY);
    fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, COLOR_DARK_GRAY);

    fb_draw_line(x + 2, y + 2, x + w - 3, y + 2, COLOR_DARK_GRAY);
    fb_draw_line(x + 2, y + 2, x + 2, y + h - 3, COLOR_DARK_GRAY);

    if (w > 20 && h > 20) {
        gui_rect(x + 4, y + 4, w - 8, 18, COLOR_BLUE);
        gui_draw_text(x + 8, y + 9, title, COLOR_WHITE);
    }
}

void gui_draw_button(int x, int y, int w, int h, const char* text, int focused) {
    gui_rect(x, y, w, h, COLOR_LIGHT_GRAY);

    if (focused) {
        fb_draw_line(x, y, x + w - 1, y, COLOR_DARK_GRAY);
        fb_draw_line(x, y, x, y + h - 1, COLOR_DARK_GRAY);
        fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, COLOR_WHITE);
        fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, COLOR_WHITE);
        gui_rect(x + 1, y + 1, w - 2, h - 2, COLOR_BLACK);
    } else {
        fb_draw_line(x, y, x + w - 1, y, COLOR_WHITE);
        fb_draw_line(x, y, x, y + h - 1, COLOR_WHITE);
        fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, COLOR_DARK_GRAY);
        fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, COLOR_DARK_GRAY);
    }

    int text_len = 0;
    while (text[text_len]) text_len++;
    int tx = x + (w - (text_len * 8)) / 2;
    int ty = y + (h - 8) / 2;
    gui_draw_text(tx, ty, text, COLOR_BLACK);
}