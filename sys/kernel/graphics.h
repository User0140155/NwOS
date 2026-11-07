#ifndef NWOS_GRAPHICS_H
#define NWOS_GRAPHICS_H

#include <stdint.h>

#define COLOR_BLACK         0x000000
#define COLOR_BLUE          0x0000AA
#define COLOR_GREEN         0x00AA00
#define COLOR_CYAN          0x00AAAA
#define COLOR_RED           0xAA0000
#define COLOR_MAGENTA       0xAA00AA
#define COLOR_BROWN         0xAA5500
#define COLOR_LIGHT_GRAY    0xAAAAAA
#define COLOR_DARK_GRAY     0x555555
#define COLOR_LIGHT_BLUE    0x5555FF
#define COLOR_LIGHT_GREEN   0x55FF55
#define COLOR_LIGHT_CYAN    0x55FFFF
#define COLOR_LIGHT_RED     0xFF5555
#define COLOR_LIGHT_MAGENTA 0xFF55FF
#define COLOR_YELLOW        0xFFFF55
#define COLOR_WHITE         0xFFFFFF

void gui_init(void);

void gui_putpixel(int x, int y, uint32_t color);
void gui_rect(int x, int y, int w, int h, uint32_t color);
void gui_frame(int x, int y, int w, int h, uint32_t color);
void gui_draw_text(int x, int y, const char* text, uint32_t color);

void gui_draw_window(int x, int y, int w, int h, const char* title);
void gui_draw_button(int x, int y, int w, int h, const char* text, int focused);

#endif