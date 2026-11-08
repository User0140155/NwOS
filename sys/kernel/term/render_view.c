#include "nwos_internal.h"

static void terminal_draw_blank_row(int screen_row);

static void terminal_draw_blank_row(int screen_row)
{
    if (!fb_is_available() || screen_row < 0 || screen_row >= HEIGHT)
        return;

    for (int x = 0; x < WIDTH; ++x)
        term_draw_cell(x, screen_row, ' ', TERM_COLOR_LIGHT_GRAY);
}

void terminal_draw_cursor(void)
{
    if (!fb_is_available() || view_offset != 0 || cursor_col < 0 || cursor_col >= WIDTH)
        return;

    term_draw_cell_mode(cursor_col, terminal_cursor_row(), '_', TERM_COLOR_WHITE);
}

void terminal_prepare_output(void)
{
    int had_lines = (line_count > 0);

    if (line_count <= 0)
        terminal_new_line();

    if (fb_is_available() && !terminal_display_initialized)
    {
        if (!fb_graphics_mode)
            fb_clear(0x000000);
        terminal_display_initialized = 1;

        if (had_lines)
            render_view();
        else
            terminal_draw_cursor();
    }
}

void render_view(void)
{
    if (!fb_is_available())
        return;

    if (line_count <= 0)
    {
        if (!fb_graphics_mode)
            for (int row = 0; row < HEIGHT; ++row)
                terminal_draw_blank_row(row);
        terminal_display_initialized = 1;
        return;
    }

    int last = (line_count - 1) - view_offset;
    if (last < 0) last = 0;
    if (last >= line_count) last = line_count - 1;

    int first = last - (HEIGHT - 1);
    if (first < 0) first = 0;

    int shown = last - first + 1;
    if (shown > HEIGHT) shown = HEIGHT;

    for (int row = 0; row < shown; ++row)
        terminal_draw_row(first + row, row);

    if (!fb_graphics_mode)
    {
        for (int row = shown; row < HEIGHT; ++row)
            terminal_draw_blank_row(row);
    }

    if (view_offset == 0 && shown > 0)
    {
        int cy = shown - 1;
        if (cy >= 0 && cy < HEIGHT && cursor_col < WIDTH)
            term_draw_cell_mode(cursor_col, cy, '_', TERM_COLOR_WHITE);
    }

    terminal_display_initialized = 1;
}
void graphics_begin(void)
{
    if (fb_graphics_mode)
        return;

    fb_graphics_mode = 1;

    fb_backbuffer_begin();
    fb_clear(0x000000);
    line_start = 0;
    line_count = 0;
    view_offset = 0;
    cursor_col = 0;

    terminal_new_line();
    terminal_display_initialized = fb_is_available();
}
void graphics_end(void)
{
    if (!fb_graphics_mode)
        return;

    fb_graphics_mode = 0;

    fb_backbuffer_end();
    fb_clear(0x000000);
    line_start = 0;
    line_count = 0;
    view_offset = 0;
    cursor_col = 0;

    terminal_new_line();
    terminal_display_initialized = fb_is_available();
    terminal_draw_cursor();
}
