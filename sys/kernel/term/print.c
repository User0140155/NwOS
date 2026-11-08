#include "nwos_internal.h"

void print(const char* text)
{
    while (*text)
    {
        putchar_os(*text);
        text++;
    }
}

void set_color(unsigned char color)
{
    text_color = color;
}
void set_base_color(unsigned char color)
{
    if ((color & 0x0F) == TERM_COLOR_BLACK)
        color = TERM_COLOR_LIGHT_GRAY;
    base_color = color;
    text_color = color;
}

void print_error(const char* text)
{
    set_color(TERM_COLOR_LIGHT_RED);
    print(text);
    set_color(base_color);
}

void print_success(const char* text)
{
    set_color(TERM_COLOR_LIGHT_GREEN);
    print(text);
    set_color(base_color);
}

void print_title(const char* text)
{
    set_color(TERM_COLOR_YELLOW);
    print(text);
    set_color(base_color);
}
