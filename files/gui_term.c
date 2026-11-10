#include "nwos_internal.h"

int shell_execute(const char* command);
const char* fs_current_path(void);

#define GT_LINES 400
#define GT_COLS  118
#define GT_IN    120
#define GT_HIST  16

int gui_term_capture = 0;

static char gt_text[GT_LINES][GT_COLS + 1];
static unsigned int gt_rgb[GT_LINES];
static int gt_count = 1;
static int gt_col = 0;

static char gt_in[GT_IN + 1];
static int gt_in_len = 0;
static char gt_hist[GT_HIST][GT_IN + 1];
static int gt_hist_n = 0;
static int gt_hist_pos = 0;
static char gt_edit_name[64];
static char gt_tmp[GT_COLS + 1];
static int gt_key_calls = 0;
static int gt_inited = 0;

static const unsigned int gt_palette[16] = {
    0x00EAF6FF, 0x006FA8FF, 0x006EE79B, 0x0062E0E8,
    0x00FF8F8F, 0x00E59BFF, 0x00E8C56A, 0x00EAF6FF,
    0x008FB8D0, 0x008CC8FF, 0x007FE8B0, 0x008FF0F8,
    0x00FF8F8F, 0x00F0A8FF, 0x00FFE08A, 0x00FFFFFF
};

static void gt_reset(void)
{
    for (int i = 0; i < GT_LINES; i++)
    {
        gt_text[i][0] = '\0';
        gt_rgb[i] = 0;
    }
    gt_count = 1;
    gt_col = 0;
}

static void gt_init(void)
{
    if (gt_inited)
        return;
    gt_inited = 1;
    gt_reset();
    gt_in[0] = '\0';
    gt_edit_name[0] = '\0';
}

static void gt_newline(void)
{
    if (gt_count >= GT_LINES)
    {
        for (int i = 1; i < GT_LINES; i++)
        {
            for (int k = 0; k <= GT_COLS; k++)
                gt_text[i - 1][k] = gt_text[i][k];
            gt_rgb[i - 1] = gt_rgb[i];
        }
        gt_count = GT_LINES - 1;
    }
    gt_text[gt_count][0] = '\0';
    gt_rgb[gt_count] = 0;
    gt_count++;
    gt_col = 0;
}

void gui_term_putc(char c)
{
    int line;

    gt_init();

    if (c == '\n')
    {
        gt_newline();
        return;
    }
    if (c == '\r')
    {
        gt_col = 0;
        return;
    }
    if (c == '\b')
    {
        if (gt_col > 0)
        {
            gt_col--;
            gt_text[gt_count - 1][gt_col] = '\0';
        }
        return;
    }
    if (c == '\t')
    {
        do
            gui_term_putc(' ');
        while ((gt_col & 3) != 0);
        return;
    }
    if ((unsigned char)c < 32 || (unsigned char)c > 126)
        return;

    if (gt_col >= GT_COLS)
        gt_newline();

    line = gt_count - 1;
    gt_text[line][gt_col++] = c;
    gt_text[line][gt_col] = '\0';

    if (c != ' ' && gt_rgb[line] == 0)
        gt_rgb[line] = gt_palette[text_color & 0x0F];
}

void gui_term_clear(void)
{
    gt_init();
    gt_reset();
}

int gui_term_fake_key(void)
{
    gt_key_calls++;
    return gt_key_calls < 64 ? ' ' : 255;
}

static void gt_puts(const char* s)
{
    while (*s)
        gui_term_putc(*s++);
}

static int gt_word_is(const char* cmd, const char* word)
{
    int n = 0;

    while (word[n] != '\0')
    {
        if (cmd[n] != word[n])
            return 0;
        n++;
    }
    return cmd[n] == '\0' || cmd[n] == ' ';
}

static void gt_refuse(const char* why)
{
    unsigned char saved = text_color;

    text_color = TERM_COLOR_LIGHT_RED;
    gt_puts(why);
    gui_term_putc('\n');
    text_color = saved;
}

static int gt_run(void)
{
    char cmd[GT_IN + 1];
    int n = 0;
    int start = 0;
    int ret = 0;
    unsigned char saved = text_color;

    gt_init();

    while (start < gt_in_len && gt_in[start] == ' ')
        start++;
    for (int i = start; i < gt_in_len && n < GT_IN; i++)
        cmd[n++] = gt_in[i];
    while (n > 0 && cmd[n - 1] == ' ')
        n--;
    cmd[n] = '\0';

    gt_in[0] = '\0';
    gt_in_len = 0;

    if (n == 0)
        return 0;

    /* history */
    if (gt_hist_n == 0 || strcmp(gt_hist[gt_hist_n - 1], cmd) != 0)
    {
        if (gt_hist_n >= GT_HIST)
        {
            for (int i = 1; i < GT_HIST; i++)
                strcpy(gt_hist[i - 1], gt_hist[i]);
            gt_hist_n = GT_HIST - 1;
        }
        strcpy(gt_hist[gt_hist_n++], cmd);
    }
    gt_hist_pos = gt_hist_n;

    if (gt_col > 0)
        gui_term_putc('\n');
    text_color = TERM_COLOR_LIGHT_CYAN;
    gt_puts("user@nwos$ ");
    gt_puts(cmd);
    gui_term_putc('\n');
    text_color = saved;

    if (gt_word_is(cmd, "exit"))
        return 14;
    if (gt_word_is(cmd, "clear") || gt_word_is(cmd, "cls"))
    {
        gt_reset();
        return 0;
    }
    if (gt_word_is(cmd, "calc"))
        return 10;
    if (gt_word_is(cmd, "files"))
        return 12;
    if (gt_word_is(cmd, "settings") || gt_word_is(cmd, "control"))
        return 13;
    if (gt_word_is(cmd, "monitor") || gt_word_is(cmd, "taskmgr"))
        return 15;
    if (gt_word_is(cmd, "editor") || gt_word_is(cmd, "edit"))
    {
        const char* p = cmd;
        int k = 0;

        while (*p && *p != ' ')
            p++;
        while (*p == ' ')
            p++;
        while (*p && k < 63)
            gt_edit_name[k++] = *p++;
        gt_edit_name[k] = '\0';
        if (k == 0)
            strcpy(gt_edit_name, "note.txt");
        return 11;
    }

    if (gt_word_is(cmd, "gui") || gt_word_is(cmd, "run") ||
        gt_word_is(cmd, "games") || gt_word_is(cmd, "game") ||
        gt_word_is(cmd, "guess") || gt_word_is(cmd, "guessh") ||
        gt_word_is(cmd, "rps") || gt_word_is(cmd, "hangman") ||
        gt_word_is(cmd, "tictactoe") || gt_word_is(cmd, "chat") ||
        gt_word_is(cmd, "passwd") || gt_word_is(cmd, "install") ||
        gt_word_is(cmd, "format") || gt_word_is(cmd, "sudo") ||
        gt_word_is(cmd, "panic") || gt_word_is(cmd, "testH") ||
        gt_word_is(cmd, "color") || gt_word_is(cmd, "listen") ||
        gt_word_is(cmd, "nw"))
    {
        gt_refuse("This command is interactive: run it in the console (close the GUI).");
        return 0;
    }
    if (strcmp(cmd, "rm -rf /") == 0 || strcmp(cmd, "rm /") == 0 ||
        strcmp(cmd, "rm disk/") == 0)
    {
        gt_refuse("Refused.");
        return 0;
    }

    gt_key_calls = 0;
    gui_term_capture = 1;
    shell_execute(cmd);
    gui_term_capture = 0;
    text_color = saved;

    if (gt_col > 0)
        gui_term_putc('\n');
    return 0;
}

static void gt_load_hist(void)
{
    int i = 0;

    while (gt_hist[gt_hist_pos][i] != '\0' && i < GT_IN)
    {
        gt_in[i] = gt_hist[gt_hist_pos][i];
        i++;
    }
    gt_in[i] = '\0';
    gt_in_len = i;
}

int gui_term_shi(int op, int a)
{
    gt_init();

    switch (op)
    {
        case 0: return gt_col > 0 ? gt_count : gt_count - 1;
        case 1:
            if (a < 0 || a >= gt_count) return 0x00EAF6FF;
            return (int)(gt_rgb[a] ? gt_rgb[a] : 0x00EAF6FF);
        case 2: gt_reset(); return 0;
        case 3: return gt_in_len;
        case 4:
            if (a >= 32 && a <= 126 && gt_in_len < GT_IN - 1)
            {
                gt_in[gt_in_len++] = (char)a;
                gt_in[gt_in_len] = '\0';
            }
            return gt_in_len;
        case 5:
            if (gt_in_len > 0)
                gt_in[--gt_in_len] = '\0';
            return gt_in_len;
        case 6: return gt_run();
        case 7:
            if (gt_hist_n > 0)
            {
                if (gt_hist_pos > 0)
                    gt_hist_pos--;
                gt_load_hist();
            }
            return gt_in_len;
        case 8:
            if (gt_hist_pos < gt_hist_n - 1)
            {
                gt_hist_pos++;
                gt_load_hist();
            }
            else
            {
                gt_hist_pos = gt_hist_n;
                gt_in[0] = '\0';
                gt_in_len = 0;
            }
            return gt_in_len;
        case 9: gt_in[0] = '\0'; gt_in_len = 0; return 0;
        case 10:
            if (a < 0 || a >= gt_count) return 0;
            { int n = 0; while (gt_text[a][n]) n++; return n; }
        case 11:
            gt_reset();
            {
                unsigned char saved = text_color;
                text_color = TERM_COLOR_LIGHT_CYAN;
                gt_puts("NwOS terminal - every console command works here.\n");
                text_color = TERM_COLOR_DARK_GRAY;
                gt_puts("Type help.  PgUp/PgDn scroll, Up/Down history, Esc closes.\n\n");
                text_color = saved;
            }
            return 0;
        default: return 0;
    }
}

const char* gui_term_shs(int op, int a)
{
    gt_init();

    switch (op)
    {
        case 0:
            if (a < 0 || a >= gt_count) return "";
            return gt_text[a];
        case 1: return gt_in;
        case 2: return gt_edit_name;
        case 3: return fs_current_path();
        case 5:
        {
            /* line (a & 4095) cut to (a >> 12) columns */
            int idx = a & 4095;
            int cols = a >> 12;
            int k = 0;

            if (idx >= gt_count) return "";
            if (cols > GT_COLS) cols = GT_COLS;
            while (k < cols && gt_text[idx][k] != '\0')
            {
                gt_tmp[k] = gt_text[idx][k];
                k++;
            }
            gt_tmp[k] = '\0';
            return gt_tmp;
        }
        default: return "";
    }
}
