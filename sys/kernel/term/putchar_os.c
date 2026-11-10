#include "nwos_internal.h"

extern int gui_term_capture;
void gui_term_putc(char c);
void gui_term_clear(void);

void clear(void)
{
    if (gui_term_capture)
    {
        gui_term_clear();
        return;
    }

    fb_graphics_mode = 0;

    line_start = 0;
    line_count = 0;
    view_offset = 0;
    cursor_col = 0;

    fb_clear(0x000000);
    terminal_new_line();

    terminal_display_initialized = fb_is_available();
    terminal_draw_cursor();
}
void scroll_view_up(void)
{
    int max_offset = line_count > HEIGHT ? line_count - HEIGHT : 0;

    if (view_offset < max_offset)
    {
        view_offset++;
        render_view();
    }
}
void scroll_view_down(void)
{
    if (view_offset > 0)
    {
        view_offset--;
        render_view();
    }
}
void putchar_os(char c)
{
    if (gui_term_capture)
    {
        gui_term_putc(c);
        return;
    }

    terminal_prepare_output();

    int force_render = (view_offset != 0);
    int old_line = terminal_current_line_index();
    view_offset = 0;
    int row = terminal_cursor_row();

    if (c == '\n')
    {
        int old_count = line_count;
        int old_col = cursor_col;

        if (!force_render && fb_is_available() && old_col < WIDTH)
            terminal_draw_stored_cell(old_line, old_col, row);

        terminal_new_line();

        if (fb_is_available())
        {
            if (force_render || old_count >= HEIGHT)
            {

                render_view();
            }
            else
            {
                terminal_draw_row(old_count, old_count);
                terminal_draw_cursor();
            }
        }
        return;
    }

    if (c == '\r')
    {
        int old_col = cursor_col;
        if (!force_render && fb_is_available() && old_col < WIDTH)
            terminal_draw_stored_cell(old_line, old_col, row);

        cursor_col = 0;
        if (fb_is_available())
        {
            if (force_render)
                render_view();
            else
                terminal_draw_cursor();
        }
        return;
    }

    if (c == '\b')
    {
        if (cursor_col > 0)
        {
            cursor_col--;
            terminal_set_cell(cursor_col, ' ');

            if (fb_is_available())
            {
                if (force_render)
                    render_view();
                else
                {
                    term_draw_cell_mode(cursor_col, row, ' ', text_color);
                    terminal_draw_cursor();
                }
            }
        }
        else if (force_render && fb_is_available())
        {
            render_view();
        }
        return;
    }

    if (cursor_col >= WIDTH)
    {
        int old_count = line_count;
        terminal_new_line();
        if (fb_is_available())
        {
            if (old_count >= HEIGHT)
                render_view();
            else
            {
                terminal_draw_row(old_count, old_count);
                terminal_draw_cursor();
            }
        }
        row = terminal_cursor_row();
    }

    int old_col = cursor_col;
    terminal_set_cell(old_col, c);
    cursor_col++;

    if (cursor_col >= WIDTH)
    {
        int old_count = line_count;
        terminal_new_line();

        if (fb_is_available())
        {
            if (force_render || old_count >= HEIGHT)
                render_view();
            else
            {
                terminal_draw_row(old_count, old_count);
                terminal_draw_cursor();
            }
        }
        return;
    }

    if (fb_is_available())
    {
        if (force_render)
            render_view();
        else
        {
            term_draw_cell_mode(old_col, row, c, text_color);
            terminal_draw_cursor();
        }
    }
}
