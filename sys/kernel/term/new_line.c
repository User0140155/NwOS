#define NWOS_INCLUDE_CHAR_SYMBOL_H
#define NWOS_INCLUDE_FONT_H
#define NWOS_INCLUDE_UTF8_H
#include "nwos_internal.h"

static void term_draw_cell_transparent(int cx, int cy, char c, unsigned char color);

int fb_graphics_mode = 0;

unsigned char text_color = TERM_COLOR_LIGHT_GRAY;
unsigned char base_color = TERM_COLOR_LIGHT_GRAY;
unsigned char prompt_color = TERM_COLOR_CYAN;

static volatile uint16_t* video =
    (volatile uint16_t*)0xB8000;

uint16_t line_buffer[SCROLLBACK_LINES][WIDTH];

int line_start = 0;
int line_count = 0;

int cursor_col = 0;
int view_offset = 0;

int terminal_display_initialized = 0;
void terminal_new_line(void)
{
    int idx;

    if (line_count < SCROLLBACK_LINES)
    {
        idx = (line_start + line_count) % SCROLLBACK_LINES;
        line_count++;
    }
    else
    {
        idx = line_start;
        line_start = (line_start + 1) % SCROLLBACK_LINES;
    }

    for (int x = 0; x < WIDTH; x++)
    {
        line_buffer[idx][x] =
            (uint16_t)' ' | ((uint16_t)TERM_COLOR_LIGHT_GRAY << 8);
    }

    cursor_col = 0;
}
void terminal_set_cell(int col, char c)
{
    int idx = (line_start + line_count - 1) % SCROLLBACK_LINES;

    line_buffer[idx][col] =
        (uint16_t)c | ((uint16_t)text_color << 8);
}
void term_draw_cell(int cx, int cy, char c, unsigned char color)
{
    int w = (int)fb_get_width();
    int h = (int)fb_get_height();

    if (w <= 0 || h <= 0) return;

    fb_draw_char_box(cx * w / WIDTH, cy * h / HEIGHT,
                     (cx + 1) * w / WIDTH, (cy + 1) * h / HEIGHT,
                     c, color);
}

static void term_draw_cell_transparent(int cx, int cy, char c, unsigned char color)
{
    int w = (int)fb_get_width();
    int h = (int)fb_get_height();

    if (w <= 0 || h <= 0) return;

    fb_draw_char_box_transparent(cx * w / WIDTH, cy * h / HEIGHT,
                                 (cx + 1) * w / WIDTH, (cy + 1) * h / HEIGHT,
                                 c, color);
}

int terminal_cursor_row(void)
{
    if (line_count <= 1)
        return 0;

    int row = line_count - 1;
    if (row >= HEIGHT)
        row = HEIGHT - 1;
    return row;
}

int terminal_current_line_index(void)
{
    if (line_count <= 0)
        return -1;

    return (line_start + line_count - 1) % SCROLLBACK_LINES;
}

void term_draw_cell_mode(int cx, int cy, char c, unsigned char color)
{
    if (fb_graphics_mode)
        term_draw_cell_transparent(cx, cy, c, color);
    else
        term_draw_cell(cx, cy, c, color);
}

void terminal_draw_stored_cell(int physical_line, int col, int screen_row)
{
    if (!fb_is_available() || physical_line < 0 || col < 0 || col >= WIDTH)
        return;

    uint16_t cell = line_buffer[physical_line][col];
    term_draw_cell_mode(col, screen_row, (char)(cell & 0xFFu),
                        (unsigned char)(cell >> 8));
}

void terminal_draw_row(int logical_line, int screen_row)
{
    if (!fb_is_available() || logical_line < 0 ||
        logical_line >= line_count || screen_row < 0 || screen_row >= HEIGHT)
        return;

    int idx = (line_start + logical_line) % SCROLLBACK_LINES;
    for (int x = 0; x < WIDTH; ++x)
    {
        uint16_t cell = line_buffer[idx][x];
        term_draw_cell_mode(x, screen_row, (char)(cell & 0xFFu),
                            (unsigned char)(cell >> 8));
    }
}
