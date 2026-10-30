#include "textredactor.h"
#include "../../kernel.h"
#include "../../../../drivers/keyboard.h"

/* Draw directly to the character framebuffer instead of printing a complete
 * screen after every key. These functions are implemented by NwOS's fb layer. */
void fb_draw_char(int x, int y, char c, unsigned char color);
void print(const char* text);
void print_int(int number);
void print_error(const char* text);
void print_success(const char* text);
void putchar_os(char c);
void clear(void);

int fs_find(const char* name);
int fs_read_text(const char* name, char* buffer, unsigned int max_size);
void fs_create(const char* name);
void fs_write(const char* name, const char* text);
int fs_write_binary(const char* name, const unsigned char* data, unsigned int size);
int nwc_compile_source(const char* source, unsigned int source_size, unsigned char* nwo_buffer, unsigned int nwo_capacity, unsigned int* nwo_size);
void nwc_capture_begin(char* buffer, unsigned int capacity);
void nwc_capture_end(void);

#ifndef WIDTH
#define WIDTH 80
#endif
#ifndef HEIGHT
#define HEIGHT 25
#endif
#ifndef COLOR_LIGHT_GRAY
#define COLOR_LIGHT_GRAY 7
#endif
#ifndef COLOR_WHITE
#define COLOR_WHITE 15
#endif

#define REDACTOR_MAX_LINES 32
#define REDACTOR_LINE_SIZE 121
#define REDACTOR_FILE_SIZE 4096
#define REDACTOR_VIEW_LINES 16
#define REDACTOR_TEXT_X 4
#define REDACTOR_TEXT_Y 3
#define REDACTOR_BINARY_MAX 20480u
#define REDACTOR_FILENAME_MAX 32

static char editor_buffer[REDACTOR_MAX_LINES][REDACTOR_LINE_SIZE];
static int editor_lines = 1;
static int cursor_line = 0;
static int cursor_col = 0;
static int editor_dirty = 0;
static char editor_status[80] = "Ready";
static unsigned char editor_nwo_output[REDACTOR_BINARY_MAX];
static char editor_compile_log[256];

static int editor_visible_lines(void)
{
    int lines = HEIGHT - 6;
    if (lines > REDACTOR_VIEW_LINES + 3) lines = REDACTOR_VIEW_LINES + 3;
    if (lines < 1) lines = 1;
    return lines;
}

static int editor_text_width(void)
{
    int width = WIDTH - REDACTOR_TEXT_X;
    return width > 1 ? width : 1;
}

static void editor_set_status(const char* text)
{
    int i = 0;
    if (text == 0) text = "";
    while (text[i] != '\0' && i < (int)sizeof(editor_status) - 1)
    {
        editor_status[i] = text[i];
        i++;
    }
    editor_status[i] = '\0';
}

static int editor_line_length(int line)
{
    int length = 0;
    if (line < 0 || line >= editor_lines) return 0;
    while (length < REDACTOR_LINE_SIZE - 1 &&
           editor_buffer[line][length] != '\0')
        length++;
    return length;
}

static void editor_clear_buffer(void)
{
    int line;
    for (line = 0; line < REDACTOR_MAX_LINES; line++)
        editor_buffer[line][0] = '\0';

    editor_lines = 1;
    cursor_line = 0;
    cursor_col = 0;
    editor_dirty = 0;
    editor_set_status("Ready");
}

static void editor_load_file(const char* filename)
{
    char file_data[REDACTOR_FILE_SIZE];
    int i = 0;

    editor_clear_buffer();
    file_data[0] = '\0';
    if (!fs_read_text(filename, file_data, sizeof(file_data))) return;
    file_data[sizeof(file_data) - 1] = '\0';

    editor_lines = 0;
    while (file_data[i] != '\0' && editor_lines < REDACTOR_MAX_LINES)
    {
        int column = 0;
        int line = editor_lines;
        while (file_data[i] != '\0' && file_data[i] != '\n' &&
               column < REDACTOR_LINE_SIZE - 1)
            editor_buffer[line][column++] = file_data[i++];
        editor_buffer[line][column] = '\0';
        editor_lines++;
        if (file_data[i] == '\n') i++;
        else if (file_data[i] != '\0')
        {
            /* The current line exceeded the supported width: skip its tail. */
            while (file_data[i] != '\0' && file_data[i] != '\n') i++;
            if (file_data[i] == '\n') i++;
        }
    }

    if (editor_lines == 0) editor_lines = 1;
    cursor_line = 0;
    cursor_col = 0;
    editor_dirty = 0;
    editor_set_status("Loaded");
}

static void editor_draw_char_at(int x, int y, char c, unsigned char color)
{
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        fb_draw_char(x, y, c, color);
}

static void editor_fill_row(int row, char c, unsigned char color)
{
    int x;
    for (x = 0; x < WIDTH; x++)
        editor_draw_char_at(x, row, c, color);
}

static void editor_draw_text_at(int x, int y, const char* text,
                                unsigned char color)
{
    int i = 0;
    if (text == 0) return;
    while (text[i] != '\0' && x + i < WIDTH)
    {
        editor_draw_char_at(x + i, y, text[i], color);
        i++;
    }
}

static int editor_first_line(void)
{
    int first = 0;
    int visible = editor_visible_lines();
    if (cursor_line >= visible) first = cursor_line - visible + 1;
    if (first + visible > editor_lines) first = editor_lines - visible;
    if (first < 0) first = 0;
    return first;
}

static int editor_horizontal_start(void)
{
    int width = editor_text_width();
    if (cursor_col >= width) return cursor_col - width + 1;
    return 0;
}

static void editor_draw_line(int line, int screen_row, int horizontal_start)
{
    int text_width = editor_text_width();
    int length;
    int j;
    char number[4];

    editor_fill_row(screen_row, ' ', COLOR_LIGHT_GRAY);
    if (line < 0 || line >= editor_lines)
    {
        editor_draw_char_at(0, screen_row, '~', COLOR_LIGHT_GRAY);
        return;
    }

    /* Fixed-width line numbers keep the text and cursor aligned. */
    number[0] = (char)('0' + ((line + 1) / 10));
    number[1] = (char)('0' + ((line + 1) % 10));
    number[2] = ':';
    number[3] = '\0';
    editor_draw_text_at(0, screen_row, number, COLOR_LIGHT_GRAY);
    editor_draw_char_at(3, screen_row, ' ', COLOR_LIGHT_GRAY);

    length = editor_line_length(line);
    for (j = 0; j < text_width; j++)
    {
        int source_col = horizontal_start + j;
        char c = ' ';
        unsigned char color = COLOR_LIGHT_GRAY;

        if (source_col < length) c = editor_buffer[line][source_col];
        if (line == cursor_line && source_col == cursor_col)
            color = COLOR_WHITE;
        if (line == cursor_line && source_col == cursor_col && source_col >= length)
            c = '_';

        editor_draw_char_at(REDACTOR_TEXT_X + j, screen_row, c, color);
    }

    /* End-of-line caret is visible in the cell just after the last character. */
    if (line == cursor_line && cursor_col == length && cursor_col < horizontal_start + text_width)
    {
        int x = REDACTOR_TEXT_X + cursor_col - horizontal_start;
        if (x >= REDACTOR_TEXT_X && x < WIDTH)
            editor_draw_char_at(x, screen_row, '_', COLOR_WHITE);
    }
}

static void editor_draw_status(void)
{
    int row = HEIGHT - 1;
    int x = 0;
    const char* prefix = "Status: ";
    editor_fill_row(row, ' ', COLOR_WHITE);
    while (prefix[x] != '\0' && x < WIDTH)
    {
        editor_draw_char_at(x, row, prefix[x], COLOR_WHITE);
        x++;
    }
    editor_draw_text_at(x, row, editor_status, COLOR_WHITE);
    if (editor_dirty)
        editor_draw_text_at(WIDTH - 10, row, "[Modified]", COLOR_WHITE);
}

static void editor_draw_header(const char* filename)
{
    int row;
    const char* title = "NwOS nano     F5: Build .nw     Ctrl+S: Save     Ctrl+X: Exit";
    int help_row = HEIGHT - 2;
    int separator_row = HEIGHT - 3;

    editor_fill_row(0, ' ', COLOR_WHITE);
    editor_draw_text_at(0, 0, title, COLOR_WHITE);

    editor_fill_row(1, ' ', COLOR_LIGHT_GRAY);
    editor_draw_text_at(0, 1, "File: ", COLOR_LIGHT_GRAY);
    editor_draw_text_at(6, 1, filename, COLOR_LIGHT_GRAY);
    if (editor_dirty)
        editor_draw_text_at(WIDTH - 10, 1, "[Modified]", COLOR_WHITE);

    editor_fill_row(2, '-', COLOR_LIGHT_GRAY);
    editor_fill_row(separator_row, '-', COLOR_LIGHT_GRAY);

    editor_fill_row(help_row, ' ', COLOR_LIGHT_GRAY);
    editor_draw_text_at(0, help_row,
        "Arrows Move  Enter Newline  Backspace Delete  Tab Spaces  F5 Build",
        COLOR_LIGHT_GRAY);

    /* Avoid stale characters if the display is taller than the editor pane. */
    for (row = 3 + editor_visible_lines(); row < separator_row; row++)
        editor_fill_row(row, ' ', COLOR_LIGHT_GRAY);
}

static void editor_draw_all(const char* filename)
{
    int row;
    int first = editor_first_line();
    int horizontal_start = editor_horizontal_start();
    int visible = editor_visible_lines();

    editor_draw_header(filename);
    for (row = 0; row < visible; row++)
        editor_draw_line(first + row, REDACTOR_TEXT_Y + row, horizontal_start);
    editor_draw_status();
}

static void editor_draw_one_or_two_lines(const char* filename, int old_line,
                                         int old_first, int old_hstart)
{
    int first = editor_first_line();
    int hstart = editor_horizontal_start();
    int visible = editor_visible_lines();
    int old_screen_row;
    int new_screen_row;
    (void)filename;

    if (first != old_first || hstart != old_hstart)
    {
        int row;
        for (row = 0; row < visible; row++)
            editor_draw_line(first + row, REDACTOR_TEXT_Y + row, hstart);
        return;
    }

    old_screen_row = REDACTOR_TEXT_Y + old_line - first;
    new_screen_row = REDACTOR_TEXT_Y + cursor_line - first;
    if (old_screen_row >= REDACTOR_TEXT_Y && old_screen_row < REDACTOR_TEXT_Y + visible)
        editor_draw_line(old_line, old_screen_row, hstart);
    if (cursor_line != old_line && new_screen_row >= REDACTOR_TEXT_Y &&
        new_screen_row < REDACTOR_TEXT_Y + visible)
        editor_draw_line(cursor_line, new_screen_row, hstart);
}

static int editor_build_text(char* output, unsigned int max_size)
{
    unsigned int position = 0;
    int i, j;
    if (output == 0 || max_size == 0) return 0;

    for (i = 0; i < editor_lines; i++)
    {
        for (j = 0; editor_buffer[i][j] != '\0'; j++)
        {
            if (position >= max_size - 1)
            {
                output[position] = '\0';
                return 0;
            }
            output[position++] = editor_buffer[i][j];
        }
        if (i < editor_lines - 1)
        {
            if (position >= max_size - 1)
            {
                output[position] = '\0';
                return 0;
            }
            output[position++] = '\n';
        }
    }
    output[position] = '\0';
    return 1;
}

static int editor_save(const char* filename)
{
    char output[REDACTOR_FILE_SIZE];
    if (!editor_build_text(output, sizeof(output)))
    {
        editor_set_status("Save failed: buffer exceeds file limit");
        return 0;
    }
    if (fs_find(filename) == -1) fs_create(filename);
    fs_write(filename, output);
    editor_dirty = 0;
    editor_set_status("File saved");
    return 1;
}

static int editor_make_output_name(const char* filename,
                                   char* output_name,
                                   unsigned int output_capacity)
{
    unsigned int length = 0;
    unsigned int base_length;
    unsigned int i;

    if (!filename || !output_name || output_capacity == 0)
        return 0;
    while (filename[length] != '\0') {
        /* The current filesystem writer uses the current-directory filename API. */
        if (filename[length] == '/' || filename[length] == '\\')
            return 0;
        if (length >= output_capacity - 1)
            return 0;
        ++length;
    }
    if (length < 4 || filename[length - 3] != '.' ||
        (filename[length - 2] != 'n' && filename[length - 2] != 'N') ||
        (filename[length - 1] != 'w' && filename[length - 1] != 'W'))
        return 0;

    base_length = length - 3;
    if (base_length + 5 > output_capacity ||
        base_length + 5 > REDACTOR_FILENAME_MAX)
        return 0;
    for (i = 0; i < base_length; ++i)
        output_name[i] = filename[i];
    output_name[base_length] = '.';
    output_name[base_length + 1] = 'n';
    output_name[base_length + 2] = 'w';
    output_name[base_length + 3] = 'o';
    output_name[base_length + 4] = '\0';
    return 1;
}

static void editor_status_from_compile_log(void)
{
    unsigned int i = 0;
    if (editor_compile_log[0] == '\0') {
        editor_set_status("Build failed: check NwC source syntax");
        return;
    }
    while (editor_compile_log[i] != '\0' &&
           editor_compile_log[i] != '\n' &&
           editor_compile_log[i] != '\r' &&
           i < sizeof(editor_status) - 1) {
        editor_status[i] = editor_compile_log[i];
        ++i;
    }
    editor_status[i] = '\0';
}

static void editor_append_status_text(char* dst, unsigned int cap,
                                      unsigned int* pos, const char* text)
{
    unsigned int i = 0;
    if (!dst || !pos || !cap || !text) return;
    while (text[i] && *pos < cap - 1)
        dst[(*pos)++] = text[i++];
    dst[*pos] = '\0';
}

static void editor_append_status_uint(char* dst, unsigned int cap,
                                      unsigned int* pos, unsigned int value)
{
    char digits[12];
    unsigned int n = 0;
    do {
        digits[n++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value && n < sizeof(digits));
    while (n && *pos < cap - 1)
        dst[(*pos)++] = digits[--n];
    dst[*pos] = '\0';
}

static void editor_compile(const char* filename)
{
    char source[REDACTOR_FILE_SIZE];
    char output_name[REDACTOR_FILENAME_MAX];
    char status[80];
    unsigned int source_size = 0;
    unsigned int nwo_size = 0;
    unsigned int pos = 0;
    int compiled;

    if (!editor_make_output_name(filename, output_name, sizeof(output_name))) {
        editor_set_status("Build failed: open a short .nw filename first");
        return;
    }
    if (!editor_build_text(source, sizeof(source))) {
        editor_set_status("Build failed: source buffer too large");
        return;
    }
    while (source[source_size] != '\0') ++source_size;

    editor_compile_log[0] = '\0';
    nwc_capture_begin(editor_compile_log, sizeof(editor_compile_log));
    compiled = nwc_compile_source(source, source_size,
                                  editor_nwo_output,
                                  sizeof(editor_nwo_output),
                                  &nwo_size);
    nwc_capture_end();

    if (!compiled || nwo_size == 0 || nwo_size > sizeof(editor_nwo_output)) {
        editor_status_from_compile_log();
        return;
    }
    if (!fs_write_binary(output_name, editor_nwo_output, nwo_size)) {
        editor_set_status("Build OK, but could not write .nwo file");
        return;
    }

    status[0] = '\0';
    editor_append_status_text(status, sizeof(status), &pos, "Build OK: ");
    editor_append_status_text(status, sizeof(status), &pos, output_name);
    editor_append_status_text(status, sizeof(status), &pos, " (");
    editor_append_status_uint(status, sizeof(status), &pos, nwo_size);
    editor_append_status_text(status, sizeof(status), &pos, " bytes)");
    editor_set_status(status);
}

static int editor_insert_char(char value)
{
    int length = editor_line_length(cursor_line);
    int i;
    if (length >= REDACTOR_LINE_SIZE - 1)
    {
        editor_set_status("Line is full (120 characters)");
        return 0;
    }
    for (i = length; i >= cursor_col; i--)
        editor_buffer[cursor_line][i + 1] = editor_buffer[cursor_line][i];
    editor_buffer[cursor_line][cursor_col] = value;
    cursor_col++;
    editor_dirty = 1;
    editor_set_status("Modified");
    return 1;
}

/* Returns 1 if the line structure changed, otherwise 0. */
static int editor_backspace(void)
{
    int length = editor_line_length(cursor_line);
    int i;
    if (cursor_col > 0)
    {
        for (i = cursor_col - 1; i < length; i++)
            editor_buffer[cursor_line][i] = editor_buffer[cursor_line][i + 1];
        cursor_col--;
        editor_dirty = 1;
        editor_set_status("Modified");
        return 0;
    }

    if (cursor_line > 0)
    {
        int previous_length = editor_line_length(cursor_line - 1);
        int current_length = editor_line_length(cursor_line);
        if (previous_length + current_length >= REDACTOR_LINE_SIZE)
        {
            editor_set_status("Cannot join: combined line is too long");
            return 0;
        }
        for (i = 0; i <= current_length; i++)
            editor_buffer[cursor_line - 1][previous_length + i] = editor_buffer[cursor_line][i];
        for (i = cursor_line; i < editor_lines - 1; i++)
        {
            int j;
            for (j = 0; j < REDACTOR_LINE_SIZE; j++)
                editor_buffer[i][j] = editor_buffer[i + 1][j];
        }
        editor_buffer[editor_lines - 1][0] = '\0';
        editor_lines--;
        cursor_line--;
        cursor_col = previous_length;
        editor_dirty = 1;
        editor_set_status("Modified");
        return 1;
    }
    return 0;
}

static int editor_newline(void)
{
    int length, suffix_length, i;
    if (editor_lines >= REDACTOR_MAX_LINES)
    {
        editor_set_status("Maximum number of lines reached (32)");
        return 0;
    }

    length = editor_line_length(cursor_line);
    suffix_length = length - cursor_col;
    for (i = editor_lines; i > cursor_line + 1; i--)
    {
        int j;
        for (j = 0; j < REDACTOR_LINE_SIZE; j++)
            editor_buffer[i][j] = editor_buffer[i - 1][j];
    }
    for (i = 0; i < suffix_length; i++)
        editor_buffer[cursor_line + 1][i] = editor_buffer[cursor_line][cursor_col + i];
    editor_buffer[cursor_line + 1][suffix_length] = '\0';
    editor_buffer[cursor_line][cursor_col] = '\0';
    editor_lines++;
    cursor_line++;
    cursor_col = 0;
    editor_dirty = 1;
    editor_set_status("Modified");
    return 1;
}

/* The keyboard driver must translate Ctrl+S into byte 19 and Ctrl+X into 24.
 * If it currently returns plain 's'/'x', that driver must be updated too. */
static int editor_is_ctrl_s(int key)
{
    if (key == 19) return 1;
#ifdef KEY_CTRL_S
    if (key == KEY_CTRL_S) return 1;
#endif
    return 0;
}

static int editor_is_ctrl_x(int key)
{
    if (key == 24) return 1;
#ifdef KEY_CTRL_X
    if (key == KEY_CTRL_X) return 1;
#endif
    return 0;
}

/* Returns 1 when exit should proceed, 0 when editing should continue. */
static int editor_confirm_exit(const char* filename)
{
    int key;
    if (!editor_dirty) return 1;

    editor_set_status("Save changes? Y=save N=discard other=cancel");
    editor_draw_status();
    key = keyboard_getkey();

    if (key == 'y' || key == 'Y')
    {
        if (editor_save(filename)) return 1;
        return 0;
    }
    if (key == 'n' || key == 'N') return 1;

    editor_set_status("Exit cancelled");
    return 0;
}

void redactor(const char* filename)
{
    editor_load_file(filename);
    clear();
    editor_draw_all(filename);

    while (1)
    {
        int key = keyboard_getkey();
        int old_line = cursor_line;
        int old_first = editor_first_line();
        int old_hstart = editor_horizontal_start();
        int structural_change = 0;
        int redraw_all = 0;
        int length;

        if (key == KEY_F5)
        {
            editor_compile(filename);
            /* Compiler diagnostics are captured; restore the editor view. */
            editor_draw_all(filename);
            continue;
        }

        if (editor_is_ctrl_s(key))
        {
            editor_save(filename);
            editor_draw_status();
            continue;
        }

        if (editor_is_ctrl_x(key) || key == KEY_ESC)
        {
            if (editor_confirm_exit(filename))
            {
                clear();
                print("Editor closed.\n");
                return;
            }
            editor_draw_status();
            continue;
        }

        if (key == KEY_LEFT)
        {
            if (cursor_col > 0) cursor_col--;
            else if (cursor_line > 0)
            {
                cursor_line--;
                cursor_col = editor_line_length(cursor_line);
            }
        }
        else if (key == KEY_RIGHT)
        {
            length = editor_line_length(cursor_line);
            if (cursor_col < length) cursor_col++;
            else if (cursor_line + 1 < editor_lines)
            {
                cursor_line++;
                cursor_col = 0;
            }
        }
        else if (key == KEY_UP)
        {
            if (cursor_line > 0)
            {
                cursor_line--;
                length = editor_line_length(cursor_line);
                if (cursor_col > length) cursor_col = length;
            }
        }
        else if (key == KEY_DOWN)
        {
            if (cursor_line + 1 < editor_lines)
            {
                cursor_line++;
                length = editor_line_length(cursor_line);
                if (cursor_col > length) cursor_col = length;
            }
        }
        else if (key == KEY_BACKSPACE)
        {
            structural_change = editor_backspace();
            redraw_all = structural_change;
        }
        else if (key == KEY_ENTER)
        {
            structural_change = editor_newline();
            redraw_all = structural_change;
        }
        else if (key == KEY_TAB)
        {
            int count;
            for (count = 0; count < 4; count++)
                if (!editor_insert_char(' ')) break;
        }
        else if (key >= 32 && key <= 255)
        {
            editor_insert_char((char)key);
        }
        else
        {
            /* Ignore unmapped function and modifier keys. */
        }

        if (redraw_all)
        {
            int row;
            int first = editor_first_line();
            int visible = editor_visible_lines();
            int hstart = editor_horizontal_start();
            for (row = 0; row < visible; row++)
                editor_draw_line(first + row, REDACTOR_TEXT_Y + row, hstart);
        }
        else
        {
            editor_draw_one_or_two_lines(filename, old_line, old_first, old_hstart);
        }
        editor_draw_status();
    }
}
