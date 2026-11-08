#include "nwos_internal.h"

static int ui_len(const char* s);
static void ui_rep(unsigned char ch, int n);
static void ui_sp(int n);
static void ui_top(const char* title);
static void ui_mid(void);
static void ui_bot(void);
static void ui_empty(void);
static void ui_text(const char* text, int indent, unsigned char fg);
static void ui_progress(int percent);

static int ui_len(const char* s)
{
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void ui_rep(unsigned char ch, int n)
{
    for (int i = 0; i < n; i++) putchar_os((char)ch);
}

static void ui_sp(int n)
{
    for (int i = 0; i < n; i++) putchar_os(' ');
}

static void ui_top(const char* title)
{
    set_color(TERM_COLOR_LIGHT_CYAN);
    ui_sp(UI_LEFT);
    putchar_os((char)CP_TL2);
    if (title && title[0]) {
        int tl = ui_len(title) + 2;
        if (tl > UI_W) tl = UI_W;
        int l = (UI_W - tl) / 2;
        int r = UI_W - tl - l;
        ui_rep(CP_H2, l);
        putchar_os(' ');
        set_color(TERM_COLOR_WHITE);
        print(title);
        set_color(TERM_COLOR_LIGHT_CYAN);
        putchar_os(' ');
        ui_rep(CP_H2, r);
    } else {
        ui_rep(CP_H2, UI_W);
    }
    putchar_os((char)CP_TR2);
    putchar_os('\n');
}

static void ui_mid(void)
{
    set_color(TERM_COLOR_LIGHT_CYAN);
    ui_sp(UI_LEFT);
    putchar_os((char)CP_ML2);
    ui_rep(CP_H2, UI_W);
    putchar_os((char)CP_MR2);
    putchar_os('\n');
}

static void ui_bot(void)
{
    set_color(TERM_COLOR_LIGHT_CYAN);
    ui_sp(UI_LEFT);
    putchar_os((char)CP_BL2);
    ui_rep(CP_H2, UI_W);
    putchar_os((char)CP_BR2);
    putchar_os('\n');
    set_color(TERM_COLOR_LIGHT_GRAY);
}

static void ui_empty(void)
{
    set_color(TERM_COLOR_LIGHT_CYAN);
    ui_sp(UI_LEFT);
    putchar_os((char)CP_V2);
    ui_sp(UI_W);
    putchar_os((char)CP_V2);
    putchar_os('\n');
}

static void ui_text(const char* text, int indent, unsigned char fg)
{
    int len = ui_len(text);
    int max_len = UI_W - indent;
    if (len > max_len) len = max_len;

    set_color(TERM_COLOR_LIGHT_CYAN);
    ui_sp(UI_LEFT);
    putchar_os((char)CP_V2);
    ui_sp(indent);
    set_color(fg);
    for (int i = 0; i < len; i++) putchar_os(text[i]);
    set_color(TERM_COLOR_LIGHT_CYAN);
    int pad = UI_W - indent - len;
    if (pad > 0) ui_sp(pad);
    putchar_os((char)CP_V2);
    putchar_os('\n');
}

static void ui_progress(int percent)
{
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;

    int bar_w = 40;
    int filled = percent * bar_w / 100;

    set_color(TERM_COLOR_LIGHT_CYAN);
    ui_sp(UI_LEFT);
    putchar_os((char)CP_V2);
    ui_sp(2);

    set_color(TERM_COLOR_LIGHT_CYAN);
    putchar_os('[');
    set_color(TERM_COLOR_LIGHT_GREEN);
    ui_rep(CP_BLK, filled);
    set_color(TERM_COLOR_DARK_GRAY);
    ui_rep(CP_BLK, bar_w - filled);
    set_color(TERM_COLOR_LIGHT_CYAN);
    putchar_os(']');
    putchar_os(' ');

    set_color(TERM_COLOR_WHITE);
    print_int(percent);
    putchar_os('%');

    set_color(TERM_COLOR_LIGHT_CYAN);
    {
        int used = 2 + 1 + bar_w + 2 + 4;
        int pad = UI_W - used;
        if (pad > 0) ui_sp(pad);
    }
    putchar_os((char)CP_V2);
    putchar_os('\n');
}
