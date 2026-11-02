#include "../../drivers/keyboard.h"
#include "../../drivers/mouse.h"
#include "../../drivers/disk.h"
#include "../../include/kernelpanic.h"

#include "Apps/wordgenerator/wordgenerator.h"
#include "Apps/textredactor/textredactor.h"
#include "nwoloader/nwo_vm.h"
#include "nwoloader/nwo_runner.h"

#include "Apps/Games/slot.h"
#include "Apps/Games/game_memory.h"
#include "Apps/Games/snake.h"
#include "kernel.h"
#include "../../compiler/nwc_kernel.h"

#include "Apps/fileStats/filestats.h"
#include "Apps/baseconverter/baseconverter.h"
#include "rootfs_data.h"

#include "package.h"
#include "words_data.h"


#include "network/net.h"

#include "nwo_loader.h"

#include "network/timer.h"
#include "zones.h"

#include "../../include/LogoNwOS.h"

#include "font.h"
#include "char_symbol.h"
#include "utf8.h"

#include "fb.h"
#include "video_config.h"

#include <stdint.h>

void disk_death(void);
void game_snake(void);



/*
* I am original User014015
* My new nickname is: User0140155
* I lost my account so now i have this
*/



/*
* MADE BY: User014015 (Lost account, new is: User0140155)
* Created: 28 july 2026 (v.0.1)
* Name: NwOS
* Github: https://github.com/User0140155/NwOS
* Old github: https://github.com/User014015/NwOS
* License MIT: (github file: LICENSE)
*/

void putchar_os(char c);

void print(const char* text);
void fs_mkdir(const char* name);
void fs_rmdir(const char* name);
void fs_rm(const char* args);
void clear(void);
void unknow_panic(unsigned int code);
static const char* shell_prompt_text(void);
static int shell_execute(const char* command);
void nwo_run(const char* filename);
void nwf_cpu_name(char* out);
unsigned int nwf_ram_mb(void);
unsigned int nwf_disk_used_kb(void);
unsigned int nwf_disk_files(void);
const char* nwf_random_word(void);
void read_line(char* buffer, int max);
void sleep(unsigned int ms);

unsigned int rand_simple(void);
const char* fs_current_path(void);
void set_p_shell(void);
void print_shell_p(void);
void change_p_color(void);
static void cmd_more_help(void);
void print_title(const char* text);
void settings_load(void);
void settings_save(void);
void cmd_settings(void);
void fs_write(const char* name, const char* text);
void fs_load(void);
void fs_save_directory(void);
int random_range(int min, int max);
void run_gui(void);
void fs_cd(const char* name);
void print_error(const char* text);
void print_success(const char* text);

void *memcpy(void *dest, const void *src, unsigned int n);
static void cmd_disks(void);

void fs_set_drive(int idx);
int  fs_get_drive(void);
void fs_format_disk(int drive_idx);
void installer(void);

static void print_size_bytes(unsigned long long bytes);

static int installer_cancelled = 0;

static void cmd_install(void);

void app_nwo_info(const char* filename);
void app_hexview(const char* filename);
void fs_create(const char* name);
static void print_hex_byte(unsigned char b);

void print_int(int number);

void game_guess(void);
void game_rps(void);
void game_word(void);
void game_coin(void);
void game_dice(void);
void game_higher_lower(void);
static void cmd_passwd(void);
void game_math(void);
void game_hangman(void);
void game_tictactoe(void);

void reboot(void);






void generate_num(void);

void games_menu(void);
int strncmp(const char* a, const char* b, int n);
void nwo_run(const char* filename);


int fs_read_text(
    const char* name,
    char* buffer,
    unsigned int max_size
);
int fs_find(const char* name);

int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size);

void reboot(void);

typedef unsigned short uint16_t;

#define WIDTH 80
#define HEIGHT 50

#define MAX_FILES 64
#define MAX_FILENAME 32
#define MAX_FILE_SIZE 20480

#define FS_TYPE_FILE 0
#define FS_TYPE_DIR  1

#define MAX_HISTORY_COMMANDS 256
#define MAX_CMD_LENGTH 1024
#define HISTORY_MAX_BYTES 10240

typedef struct __attribute__((packed)) {
    uint8_t used;
    uint8_t type;
    uint16_t parent;
    char name[MAX_FILENAME];
    uint32_t size;
} DirEntry;

typedef struct {
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_char)(int x, int y, char c, uint8_t color);
    int  (*getkey)(void);
    void (*sleep)(unsigned int ms);
} gui_api_t;

#define FS_FILE_SECTORS 40
#define FS_MAGIC_LBA   199
#define FS_MAGIC_VALUE 0x4E774653  // "NwFS"
#define FS_SEQ_OFFSET  4           /* install sequence inside the magic sector */
#define FS_VER_OFFSET  8           /* filesystem layout version */
#define FS_FORMAT_VER  2

static DirEntry directory[MAX_FILES];
static uint16_t current_dir = 0;

static int fb_graphics_mode = 0;

static unsigned char text_color = COLOR_LIGHT_GRAY;
static unsigned char base_color = COLOR_LIGHT_GRAY;
static unsigned char prompt_color = COLOR_CYAN;
static int console_f7_enabled = 0;

static int timezone_offset = 0; /* UTC+0 by default */
static int custom_time_offset = 0;
static volatile unsigned long long idle_us_accum = 0;
static int use_custom_time = 0;
static int custom_day = -1;
static int custom_month = -1;
static int custom_year = -1;

void fs_load(void);
void fs_save_directory(void);

int shell_p = 0;
char invite_user[28] = OS_NAME "> ";

static int cmd_sudo = 0;

#define SETTINGS_FILE "sys.conf"
#define SETTINGS_MAX  2048

char cfg_user[32] = "user";
char cfg_os[32] = OS_NAME;
static char cfg_hostname[64] = "nwos";
static char cfg_root_salt[17] = "";
static char cfg_root_hash[65] = "";
static int cfg_setup_complete = 0;
static int cfg_require_login = 0;
static int cfg_disk_history = 0;

static unsigned char color_from_name(const char* name);
static int cfg_find_key(const char* buffer, const char* key, char* out, int out_max);

unsigned char rtc_read(unsigned char reg);
unsigned char bcd_to_bin(unsigned char value);

void get_time(int* hour, int* minute, int* second);
void get_date(int* day, int* month, int* year);

void print_two_digits(int value);

static volatile uint16_t* video =
    (volatile uint16_t*)0xB8000;

#define SCROLLBACK_LINES 300

static uint16_t line_buffer[SCROLLBACK_LINES][WIDTH];

static int line_start = 0;
static int line_count = 0;

static int cursor_col = 0;
static int view_offset = 0;  
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
            (uint16_t)' ' | ((uint16_t)COLOR_LIGHT_GRAY << 8);
    }

    cursor_col = 0;
}

void *memcpy(void *dest, const void *src, unsigned int n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    for (unsigned int i = 0; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
}
void terminal_set_cell(int col, char c)
{
    int idx = (line_start + line_count - 1) % SCROLLBACK_LINES;

    line_buffer[idx][col] =
        (uint16_t)c | ((uint16_t)text_color << 8);
}
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}
void render_view(void)
{
    if (!fb_is_available()) return;

    int shown = 0;
    int pad = HEIGHT;
    int first = 0;

    if (line_count > 0)
    {
        int last = (line_count - 1) - view_offset;
        if (last < 0) last = 0;
        first = last - (HEIGHT - 1);
        if (first < 0) first = 0;
        shown = last - first + 1;
        pad = HEIGHT - shown;
    }

    if (!fb_graphics_mode)
    {
        for (int row = 0; row < pad; row++)
            for (int x = 0; x < WIDTH; x++)
                fb_draw_char(x, row, ' ', COLOR_LIGHT_GRAY);
    }

    for (int i = 0; i < shown; i++)
    {
        int idx = (line_start + first + i) % SCROLLBACK_LINES;
        for (int x = 0; x < WIDTH; x++)
        {
            uint16_t cell = line_buffer[idx][x];
            if (fb_graphics_mode)
                fb_draw_char_transparent(x, pad + i, cell & 0xFF, (cell >> 8) & 0xFF);
            else
                fb_draw_char(x, pad + i, cell & 0xFF, (cell >> 8) & 0xFF);
        }
    }
    
    int cy = pad + shown - 1;
    if (cy >= 0 && cy < HEIGHT) {
        if (fb_graphics_mode)
            fb_draw_char_transparent(cursor_col, cy, '_', COLOR_WHITE);
        else
            fb_draw_char(cursor_col, cy, '_', COLOR_WHITE);
    }
}
static void cmd_mem(void)
{
    unsigned int ram_mb = nwf_ram_mb();
    print_title("Memory Information:\n");
    print("  Total RAM: ");
    print_int((int)ram_mb);
    print(" MiB\n");
    print("  Memory model: 32-bit\n");
    putchar_os('\n');
}

static void cmd_cpu(void)
{
    char cpu_name[49];
    nwf_cpu_name(cpu_name);
    print_title("CPU Information:\n");
    print("  CPU: ");
    print(cpu_name);
    putchar_os('\n');
    print("  Architecture: x86_64\n");
    putchar_os('\n');
}

static void cmd_uptime(void)
{
    unsigned long long total_sec = timer_ms() / 1000;
    unsigned long long up_m = total_sec / 60;
    unsigned long long up_h = up_m / 60;
    up_m %= 60;
    total_sec %= 60;
    print_title("Uptime:\n");
    print("  ");
    if (up_h > 0) { print_int((int)up_h); print("h "); }
    if (up_m > 0) { print_int((int)up_m); print("m "); }
    print_int((int)total_sec); print("s\n");
    putchar_os('\n');
}

static void cmd_zones(void)
{
    print_title("Time Zones (all 24h worldwide):\n");
    for (int i = 0; i < TIME_ZONE_COUNT; i++)
    {
        print_int(i + 1);
        print(". ");
        print(time_zone_names[i]);
        if (timezone_offset == time_zone_offsets[i])
            print(" [ACTIVE]");
        putchar_os('\n');
    }
    putchar_os('\n');
    print("Use 'timezone <num>' to set zone.\n");
    print("Use 'date <DD> <MM> <YYYY>' to set custom date.\n");
    print("Use 'date reset' to reset date to RTC.\n\n");
}

static void cmd_timezone(const char* args)
{
    if (args[0] == '\0')
    {
        print("Usage: timezone <num> (1-");
        print_int(TIME_ZONE_COUNT);
        print(")\n");
        return;
    }
    int num = atoi_simple(args);
    if (num < 1 || num > TIME_ZONE_COUNT)
    {
        print_error("Invalid timezone number.\n");
        return;
    }
    timezone_offset = time_zone_offsets[num - 1];
    print_success("Timezone set to: ");
    print(time_zone_names[num - 1]);
    putchar_os('\n');
}

static void cmd_date_set(const char* args)
{
    if (args[0] == '\0')
    {
        print("Usage: date <DD> <MM> <YYYY> or date reset\n");
        return;
    }
    if (strcmp(args, "reset") == 0)
    {
        custom_day = -1;
        custom_month = -1;
        custom_year = -1;
        print_success("Date reset to RTC.\n");
        return;
    }
    char d[16], m[16], y[16];
    int i = 0, j = 0;
    while (args[i] == ' ') i++;
    while (args[i] != ' ' && args[i] != '\0' && j < 15) { d[j++] = args[i++]; }
    d[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != ' ' && args[i] != '\0' && j < 15) { m[j++] = args[i++]; }
    m[j] = '\0';
    while (args[i] == ' ') i++;
    j = 0;
    while (args[i] != '\0' && j < 15) { y[j++] = args[i++]; }
    y[j] = '\0';
    
    int dd = atoi_simple(d);
    int mm = atoi_simple(m);
    int yy = atoi_simple(y);
    
    if (dd < 1 || dd > 31 || mm < 1 || mm > 12 || yy < 0)
    {
        print_error("Invalid date.\n");
        return;
    }
    
    custom_day = dd;
    custom_month = mm;
    custom_year = yy;
    print_success("Date set to ");
    print_int(dd); print("."); print_int(mm); print("."); print_int(yy);
    print(" (OS will display this date).\n");
}

static void cmd_time_cmd(void)
{
    int hour, min, sec;
    int day, month, year;
    get_time(&hour, &min, &sec);
    get_date(&day, &month, &year);
    print_title("Current Time/Date:\n");
    print("  Time: ");
    print_two_digits(hour); putchar_os(':');
    print_two_digits(min); putchar_os(':');
    print_two_digits(sec);
    putchar_os('\n');
    print("  Date: ");
    print_two_digits(day); putchar_os('.');
    print_two_digits(month); putchar_os('.');
    print_int(year);
    putchar_os('\n');
    print("  Zone: UTC");
    if (timezone_offset >= 0) putchar_os('+');
    print_int(timezone_offset);
    putchar_os('\n');
    if (custom_day != -1)
        print("  (Custom date active)\n");
    putchar_os('\n');
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
    render_view();
}

void clear(void)
{
    fb_graphics_mode = 0;

    line_start = 0;
    line_count = 0;
    view_offset = 0;
    cursor_col = 0;

    fb_clear(0x000000);

    terminal_new_line();
    render_view();
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
    view_offset = 0;

    if (c == '\n')
    {
        terminal_new_line();
        render_view();
        return;
    }

    if (c == '\r')
    {
        cursor_col = 0;
        render_view();
        return;
    }

    if (c == '\b')
    {
        if (cursor_col > 0)
        {
            cursor_col--;
            terminal_set_cell(cursor_col, ' ');
            
            // Стираем символ на экране (быстро, без render_view)
            if (fb_is_available()) {
                fb_draw_char(cursor_col, HEIGHT - 1, ' ', text_color);
                fb_draw_char(cursor_col, HEIGHT - 1, '_', COLOR_WHITE);
            }
        }
        return;
    }

    terminal_set_cell(cursor_col, c);
    int old_col = cursor_col;
    cursor_col++;

    if (cursor_col >= WIDTH)
    {
        terminal_new_line();
        render_view();
        return;
    }

    // Рисуем только новую букву (мгновенно)
    if (fb_is_available()) {
        fb_draw_char(old_col, HEIGHT - 1, c, text_color);
        fb_draw_char(cursor_col, HEIGHT - 1, '_', COLOR_WHITE);
    }
}
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
    base_color = color;
    text_color = color;
}
int strcmp(const char* a, const char* b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }

    return *a - *b;
}

int strncmp(const char* a, const char* b, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return (unsigned char)a[i] -
                   (unsigned char)b[i];
        }

        if (a[i] == '\0')
        {
            return 0;
        }
    }

    return 0;
}


int starts_with(const char* text, const char* prefix)
{
    while (*prefix)
    {
        if (*text != *prefix)
            return 0;

        text++;
        prefix++;
    }

    return 1;
}


int atoi_simple(const char* text)
{
    int value = 0;

    while (*text >= '0' && *text <= '9')
    {
        value = value * 10 + (*text - '0');
        text++;
    }

    return value;
}

static void cfg_strcpy(char* dest, const char* src, int max)
{
    int i = 0;

    while (src[i] != '\0' && i < max - 1)
    {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

static int normalize_username(char* s)
{
    int n = 0;

    while (s[n] != '\0')
    {
        if (n >= 31)
            return 0;

        if (s[n] >= 'A' && s[n] <= 'Z')
            s[n] = (char)(s[n] - 'A' + 'a');

        char c = s[n];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return 0;

        if (n == 0 && (c == '-' || (c >= '0' && c <= '9')))
            return 0;

        n++;
    }

    if (n == 0 || strcmp(s, "root") == 0)
        return 0;

    return 1;
}

static int normalize_hostname(char* s)
{
    int n = 0;

    while (s[n] != '\0')
    {
        if (n >= 63)
            return 0;

        if (s[n] >= 'A' && s[n] <= 'Z')
            s[n] = (char)(s[n] - 'A' + 'a');

        char c = s[n];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '-'))
            return 0;

        if (n == 0 && c == '-')
            return 0;

        n++;
    }

    if (n == 0 || s[n - 1] == '-')
        return 0;

    return 1;
}

static int read_password(char* buffer, int max)
{
    int length = 0;

    if (buffer == 0 || max < 2)
        return -1;

    while (1)
    {
        int key = keyboard_getkey();

        if (key == KEY_ENTER)
        {
            buffer[length] = '\0';
            putchar_os('\n');
            return length;
        }

        if (key == KEY_BACKSPACE)
        {
            if (length > 0)
            {
                length--;
                buffer[length] = '\0';
                putchar_os('\b');
            }

            continue;
        }

        if (key >= 32 && key <= 126 && length < max - 1)
        {
            buffer[length++] = (char)key;
            putchar_os('*');
        }
    }
}

typedef struct
{
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} nw_sha256_ctx_t;

static const uint32_t nw_sha_k[64] =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t nw_rotr32(uint32_t x, unsigned int n)
{
    return (x >> n) | (x << (32 - n));
}

static void nw_sha256_transform(nw_sha256_ctx_t* ctx,
                                const uint8_t data[64])
{
    uint32_t m[64];

    for (int i = 0; i < 16; i++)
    {
        m[i] =
            ((uint32_t)data[i * 4] << 24) |
            ((uint32_t)data[i * 4 + 1] << 16) |
            ((uint32_t)data[i * 4 + 2] << 8) |
            ((uint32_t)data[i * 4 + 3]);
    }

    for (int i = 16; i < 64; i++)
    {
        uint32_t s0 =
            nw_rotr32(m[i - 15], 7) ^
            nw_rotr32(m[i - 15], 18) ^
            (m[i - 15] >> 3);

        uint32_t s1 =
            nw_rotr32(m[i - 2], 17) ^
            nw_rotr32(m[i - 2], 19) ^
            (m[i - 2] >> 10);

        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];

    for (int i = 0; i < 64; i++)
    {
        uint32_t s1 =
            nw_rotr32(e, 6) ^
            nw_rotr32(e, 11) ^
            nw_rotr32(e, 25);

        uint32_t ch = (e & f) ^ (~e & g);

        uint32_t temp1 =
            h + s1 + ch + nw_sha_k[i] + m[i];

        uint32_t s0 =
            nw_rotr32(a, 2) ^
            nw_rotr32(a, 13) ^
            nw_rotr32(a, 22);

        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void nw_sha256_init(nw_sha256_ctx_t* ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;

    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void nw_sha256_update(nw_sha256_ctx_t* ctx,
                             const uint8_t* data,
                             uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        ctx->data[ctx->datalen++] = data[i];

        if (ctx->datalen == 64)
        {
            nw_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void nw_sha256_final(nw_sha256_ctx_t* ctx,
                            uint8_t hash[32])
{
    uint32_t i = ctx->datalen;

    ctx->data[i++] = 0x80;

    if (i > 56)
    {
        while (i < 64)
            ctx->data[i++] = 0;

        nw_sha256_transform(ctx, ctx->data);

        for (i = 0; i < 56; i++)
            ctx->data[i] = 0;
    }
    else
    {
        while (i < 56)
            ctx->data[i++] = 0;
    }

    ctx->bitlen += (uint64_t)ctx->datalen * 8;

    for (i = 0; i < 8; i++)
        ctx->data[63 - i] =
            (uint8_t)(ctx->bitlen >> (i * 8));

    nw_sha256_transform(ctx, ctx->data);

    for (i = 0; i < 32; i++)
    {
        hash[i] = (uint8_t)(
            ctx->state[i / 4] >> (24 - (i % 4) * 8)
        );
    }
}

static void nw_password_hash(const char* password,
                             const char* salt,
                             char output[65])
{
    static const char hex[] = "0123456789abcdef";
    nw_sha256_ctx_t ctx;
    uint8_t digest[32];
    uint32_t salt_len = 0;
    uint32_t pass_len = 0;

    while (salt[salt_len] != '\0') salt_len++;
    while (password[pass_len] != '\0') pass_len++;

    nw_sha256_init(&ctx);
    nw_sha256_update(&ctx, (const uint8_t*)salt, salt_len);

    const uint8_t separator = ':';
    nw_sha256_update(&ctx, &separator, 1);

    nw_sha256_update(&ctx, (const uint8_t*)password, pass_len);
    nw_sha256_final(&ctx, digest);

    for (int i = 0; i < 32; i++)
    {
        output[i * 2]     = hex[digest[i] >> 4];
        output[i * 2 + 1] = hex[digest[i] & 0x0F];
    }

    output[64] = '\0';
}

static void make_root_salt(char output[17])
{
    static const char hex[] = "0123456789abcdef";

    //random_init();

    for (int block = 0; block < 2; block++)
    {
        uint32_t lo, hi;

        __asm__ volatile ("rdtsc"
                          : "=a"(lo), "=d"(hi));

        uint32_t value =
            rand_simple() ^ lo ^ (hi * 0x9E3779B9u);

        for (int j = 0; j < 8; j++)
        {
            output[block * 8 + j] =
                hex[(value >> (28 - j * 4)) & 0x0F];
        }
    }

    output[16] = '\0';
}

static int root_password_is_set(void)
{
    return cfg_root_salt[0] != '\0' &&
           cfg_root_hash[0] != '\0';
}

static int verify_root_password(const char* password)
{
    if (!password || !root_password_is_set())
        return 0;

    char hash[65];
    unsigned int diff = 0;

    nw_password_hash(password, cfg_root_salt, hash);

    for (int i = 0; i < 64; i++)
        diff |= (unsigned char)hash[i] ^
                (unsigned char)cfg_root_hash[i];

    for (int i = 0; i < 65; i++)
        hash[i] = '\0';

    return diff == 0;
}

static void wipe_secret(char* secret, int size)
{
    volatile char* p = secret;

    for (int i = 0; i < size; i++)
        p[i] = '\0';
}

static int authenticate_root_password(void)
{
    if (!root_password_is_set())
    {
        print_error(
            "Root password is not configured. Run passwd first.\n"
        );
        return 0;
    }

    for (int attempt = 0; attempt < 3; attempt++)
    {
        char password[64];

        print("Root password: ");
        read_password(password, sizeof(password));

        int valid = verify_root_password(password);
        wipe_secret(password, sizeof(password));

        if (valid)
        {
            print_success("Authentication successful.\n");
            return 1;
        }

        print_error("Incorrect password.\n");
        sleep(750);
    }

    print_error("Authentication failed.\n");
    return 0;
}

void wait_key(void)
{
    print("\nPress any key to continue...\n");
    keyboard_getkey();
}

static char cmd_history[MAX_HISTORY_COMMANDS][MAX_CMD_LENGTH];
static int history_count = 0;
static int history_pos = -1;

static void history_check_size(void)
{
    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1) return;
    current_dir = sys_dir;

    unsigned int size = fs_get_file_size("history.txt");
    if (size > HISTORY_MAX_BYTES)
    {
        fs_delete("history.txt");
        history_count = 0;
        history_pos = -1;
    }

    current_dir = prev_dir;
}

void history_load(void)
{
    if (!cfg_disk_history) return;

    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1) return;
    current_dir = sys_dir;

    history_check_size();

    char buffer[2048];
    if (fs_read_text("history.txt", buffer, sizeof(buffer)))
    {
        int i = 0;
        int line_idx = 0;
        int char_idx = 0;

        while (buffer[i] != '\0' && line_idx < MAX_HISTORY_COMMANDS)
        {
            if (buffer[i] == '\n')
            {
                cmd_history[line_idx][char_idx] = '\0';
                line_idx++;
                char_idx = 0;
            }
            else if (buffer[i] != '\r')
            {
                if (char_idx < MAX_CMD_LENGTH - 1)
                {
                    cmd_history[line_idx][char_idx++] = buffer[i];
                }
            }
            i++;
        }
        history_count = line_idx;
    }

    current_dir = prev_dir;
}

void history_save(void)
{
    if (!cfg_disk_history) return;

    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1) return;
    current_dir = sys_dir;

    history_check_size();

    char accum[2048];
    int pos = 0;
    accum[0] = '\0';

    for (int i = 0; i < history_count; i++)
    {
        int j = 0;
        while (cmd_history[i][j] != '\0' && pos < (int)sizeof(accum) - 3)
        {
            accum[pos++] = cmd_history[i][j++];
        }
        accum[pos++] = '\n';
    }
    accum[pos] = '\0';

    if (fs_find("history.txt") == -1)
    {
        fs_create("history.txt");
    }
    fs_write("history.txt", accum);

    current_dir = prev_dir;
}

void history_add_command(const char* cmd)
{
    if (cmd == 0 || cmd[0] == '\0') return;
    
    if (history_count > 0 && strcmp(cmd_history[history_count - 1], cmd) == 0)
        return;

    if (history_count < MAX_HISTORY_COMMANDS)
    {
        strcpy(cmd_history[history_count++], cmd);
    }
    else
    {
        for (int i = 0; i < MAX_HISTORY_COMMANDS - 1; i++)
        {
            strcpy(cmd_history[i], cmd_history[i + 1]);
        }
        strcpy(cmd_history[MAX_HISTORY_COMMANDS - 1], cmd);
    }

    history_pos = history_count;
    history_save();
}

void read_line(char* buffer, int max)
{
    int length = 0;
    history_pos = history_count;

    while (1)
    {
        int key = keyboard_getkey();

        if (console_f7_enabled && key == KEY_F7)
        {
            const char* word = nwf_random_word();
            int start = length;

            while (start > 0 && buffer[start - 1] != ' ')
                start--;

            while (length > start)
            {
                putchar_os('\b');
                length--;
            }

            buffer[length] = '\0';

            int i = 0;
            while (word[i] != '\0' && length < max - 1)
            {
                buffer[length++] = word[i];
                putchar_os(word[i]);
                i++;
            }

            if (length < max - 1)
            {
                buffer[length++] = ' ';
                putchar_os(' ');
            }

            buffer[length] = '\0';
            continue;
        }

        if (key == KEY_TAB)
        {
            for (int i = 0; i < 4; i++)
            {
                if (length >= max - 1)
                    break;

                buffer[length++] = ' ';
                putchar_os(' ');
            }

            continue;
        }

        if (key == KEY_ENTER)
        {
            buffer[length] = '\0';
            putchar_os('\n');
            
            if (length > 0)
            {
                history_add_command(buffer);
            }
            return;
        }

        if (key == KEY_BACKSPACE)
        {
            if (length > 0)
            {
                length--;
                buffer[length] = '\0';
                putchar_os('\b');
            }

            continue;
        }

        if (key == KEY_UP)
        {
            if (history_count > 0 && history_pos > 0)
            {
                history_pos--;
                
                while (length > 0)
                {
                    putchar_os('\b');
                    length--;
                }

                int i = 0;
                while (cmd_history[history_pos][i] != '\0' && i < max - 1)
                {
                    buffer[i] = cmd_history[history_pos][i];
                    putchar_os(buffer[i]);
                    i++;
                }
                length = i;
                buffer[length] = '\0';
            }
            continue;
        }

        if (key == KEY_DOWN)
        {
            if (history_count > 0 && history_pos < history_count)
            {
                history_pos++;

                while (length > 0)
                {
                    putchar_os('\b');
                    length--;
                }

                if (history_pos < history_count)
                {
                    int i = 0;
                    while (cmd_history[history_pos][i] != '\0' && i < max - 1)
                    {
                        buffer[i] = cmd_history[history_pos][i];
                        putchar_os(buffer[i]);
                        i++;
                    }
                    length = i;
                    buffer[length] = '\0';
                }
                else
                {
                    buffer[0] = '\0';
                    length = 0;
                }
            }
            continue;
        }

        if (key > 0 && key < 256)
        {
            if (length < max - 1)
            {
                buffer[length] = (char)key;
                length++;
                putchar_os((char)key);
            }
        }
    }
}

void print_error(const char* text)
{
    set_color(COLOR_LIGHT_RED);
    print(text);
    set_color(base_color);
}

void print_success(const char* text)
{
    set_color(COLOR_LIGHT_GREEN);
    print(text);
    set_color(base_color);
}

void print_title(const char* text)
{
    set_color(COLOR_YELLOW);
    print(text);
    set_color(base_color);
}

void delay(unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        __asm__ volatile ("nop");
    }
}

static unsigned int random_seed = 123456789;

int random_range(int min, int max)
{
    return min +
        (rand_simple() % (max - min + 1));
}

unsigned int rand_simple(void)
{
    random_seed =
        random_seed * 1103515245 + 12345;

    return random_seed;
}

const char* nwf_random_word(void)
{
    int index;

    if (NWOS_WORD_COUNT <= 0)
        return "";

    index = random_range(0, NWOS_WORD_COUNT - 1);
    return nw_words[index];
}

void random_init(void)
{
    unsigned int seed;

    __asm__ volatile (
        "rdtsc"
        : "=a"(seed)
        :
        : "edx"
    );

    random_seed ^= seed;
}

void game_guess(void)
{
    char buffer[32];

    int secret = random_range(1, 100);

    print("\n=== GUESS THE NUMBER ===\n");
    print("I chose a number from 1 to 100.\n");
    print_error("Type quit to exit.\n\n");

    while (1)
    {
        print("your guess: ");

        read_line(buffer, 32);

        if (strcmp(buffer, "quit") == 0)
        {
            print("You left the game.\n");
            wait_key();
            return;
        }

        int number = atoi_simple(buffer);

        if (number == secret)
        {
            print_success("Correct! You win!\n");
            wait_key();
            return;
        }

        if (number < secret)
        {
            print("Too small!\n");
        }
        else {
            print("Too big!\n");
        }
    }
}

void game_guessh(void)
{
    char bufferh[32];

    int secreth = random_range(1, 250);

    print("\n=== GUESS THE NUMBER HARD===\n");
    print("I chose a number from 1 to 250.\n");
    print_error("Type quit to exit.\n\n");

    while (1)
    {
        print("your guess: ");

        read_line(bufferh, 32);

        if (strcmp(bufferh, "quit") == 0)
        {
            print("You left the game.\n");
            wait_key();
            return;
        }

        int number = atoi_simple(bufferh);

        if (number == secreth)
        {
            print_success("Correct! You win!\n");
            wait_key();
            return;
        }

        if (number < secreth)
            print("Too small!\n");
        else
            print("Too big!\n");
    }
}

void game_dice(void)
{
    int dice;

    print_title("\n=== DICE ===\n\n");

    dice = (rand_simple() % 6) + 1;

    print_success("You rolled: ");
    print_int(dice);
    putchar_os('\n');

    putchar_os('\n');
    wait_key();
}

void game_higher_lower(void)
{
    char input[16];

    int current;
    int next;

    int score = 0;

    print_title("\n=== HIGHER OR LOWER ===\n\n");

    current = (rand_simple() % 100) + 1;

    while (1)
    {
        print("Current number: ");
        print_int(current);
        putchar_os('\n');

        print("Will the next number be higher or lower?\n");
        print("1 - Higher\n");
        print("2 - Lower\n");
        print("0 - Quit\n\n");

        print("Your choice: ");
        read_line(input, 16);

        if (strcmp(input, "0") == 0)
            break;

        if (strcmp(input, "1") != 0 &&
            strcmp(input, "2") != 0)
        {
            print_error("Invalid choice.\n\n");
            continue;
        }

        next = (rand_simple() % 100) + 1;

        print("Next number: ");
        print_int(next);
        putchar_os('\n');

        if (next == current)
        {
            print("Same number! No points.\n\n");
            continue;
        }

        if ((input[0] == '1' && next > current) ||
            (input[0] == '2' && next < current))
        {
            score++;

            print_success("Correct!\n");
        }
        else
        {
            print_error("Wrong!\n");
        }

        print("Score: ");
        print_int(score);
        putchar_os('\n');
        putchar_os('\n');

        current = next;
    }

    print("Final score: ");
    print_int(score);
    putchar_os('\n');
    wait_key();
}

void game_math(void)
{
    char input[32];

    int a;
    int b;
    int operation;
    int answer;
    int user_answer;

    int score = 0;

    print_title("\n=== MATH QUIZ ===\n\n");

    for (int round = 0; round < 5; round++)
    {
        a = (rand_simple() % 20) + 1;
        b = (rand_simple() % 20) + 1;

        operation = rand_simple() % 2;

        print("Question ");
        print_int(round + 1);
        print(": ");

        if (operation == 0)
        {
            answer = a + b;

            print_int(a);
            print(" + ");
            print_int(b);
        }
        else
        {
            if (a < b)
            {
                int temp = a;
                a = b;
                b = temp;
            }

            answer = a - b;

            print_int(a);
            print(" - ");
            print_int(b);
        }

        print(" = ");

        read_line(input, 32);

        user_answer = atoi_simple(input);

        if (user_answer == answer)
        {
            print_success("Correct!\n");
            score++;
        }
        else
        {
            print_error("Wrong! Answer: ");
            print_int(answer);
            putchar_os('\n');
        }
    }

    print("\nScore: ");
    print_int(score);
    print("/5\n\n");
    wait_key();
}


char to_upper_letter(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - 'a' + 'A');
    }

    return c;
}

void game_hangman(void)
{
    static const char* words[] =
    {
        "KERNEL", "MEMORY", "POINTER", "COMPILE",
        "BINARY", "ARRAY", "LINKER", "BOOTLOADER"
    };

    const int word_count = 8;

    const char* word = words[random_range(0, word_count - 1)];

    int length = 0;

    while (word[length] != '\0')
    {
        length++;
    }

    char revealed[32];

    for (int i = 0; i < length; i++)
    {
        revealed[i] = '_';
    }

    revealed[length] = '\0';

    char guessed[26];

    for (int i = 0; i < 26; i++)
    {
        guessed[i] = 0;
    }

    int lives = 6;
    int found = 0;

    print_title("\n=== HANGMAN ===\n");
    print("Guess the word, one letter at a time.\n");
    print_error("Type quit to exit.\n\n");

    while (lives > 0 && found < length)
    {
        print("Word: ");
        print(revealed);
        print("\nLives: ");
        print_int(lives);
        print("\n\nGuess a letter: ");

        char input[8];

        read_line(input, 8);

        if (strcmp(input, "quit") == 0)
        {
            print("You left the game. The word was: ");
            print(word);
            putchar_os('\n');
            wait_key();
            return;
        }

        char c = to_upper_letter(input[0]);

        if (c < 'A' || c > 'Z')
        {
            print_error("Please enter a single letter.\n\n");
            continue;
        }

        int idx = c - 'A';

        if (guessed[idx])
        {
            print_error("Already guessed that letter.\n\n");
            continue;
        }

        guessed[idx] = 1;

        int hit = 0;

        for (int i = 0; i < length; i++)
        {
            if (word[i] == c)
            {
                revealed[i] = c;
                hit = 1;
                found++;
            }
        }

        if (hit)
        {
            print_success("Good guess!\n\n");
        }
        else
        {
            lives--;
            print_error("Wrong!\n\n");
        }
    }

    if (found == length)
    {
        print_success("You win! The word was: ");
        print(word);
        putchar_os('\n');
    }
    else
    {
        print_error("You lose! The word was: ");
        print(word);
        putchar_os('\n');
    }
    wait_key();
}


void ttt_print_board(char board[9])
{
    print("\n");

    for (int row = 0; row < 3; row++)
    {
        print(" ");
        putchar_os(board[row * 3]);
        print(" | ");
        putchar_os(board[row * 3 + 1]);
        print(" | ");
        putchar_os(board[row * 3 + 2]);
        print("\n");

        if (row < 2)
        {
            print("---+---+---\n");
        }
    }

    print("\n");
}

int ttt_check_win(char board[9], char player)
{
    int wins[8][3] =
    {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
        {0, 4, 8}, {2, 4, 6}
    };

    for (int i = 0; i < 8; i++)
    {
        if (board[wins[i][0]] == player &&
            board[wins[i][1]] == player &&
            board[wins[i][2]] == player)
        {
            return 1;
        }
    }

    return 0;
}

int ttt_board_full(char board[9])
{
    for (int i = 0; i < 9; i++)
    {
        if (board[i] != 'X' && board[i] != 'O')
        {
            return 0;
        }
    }

    return 1;
}

void game_tictactoe(void)
{
    char board[9];

    for (int i = 0; i < 9; i++)
    {
        board[i] = (char)('1' + i);
    }

    print_title("\n=== TIC-TAC-TOE ===\n");
    print("You are X, computer is O.\n");
    print("Enter a number 1-9 to place your mark.\n");

    while (1)
    {
        ttt_print_board(board);

        char input[8];

        print("Your move: ");
        read_line(input, 8);

        int move = atoi_simple(input);

        if (move < 1 || move > 9 ||
            board[move - 1] == 'X' || board[move - 1] == 'O')
        {
            print_error("Invalid move.\n");
            continue;
        }

        board[move - 1] = 'X';

        if (ttt_check_win(board, 'X'))
        {
            ttt_print_board(board);
            print_success("You win!\n\n");
            wait_key();
            return;
        }

        if (ttt_board_full(board))
        {
            ttt_print_board(board);
            print("It's a draw!\n\n");
            wait_key();
            return;
        }

        int move_o;

        do
        {
            move_o = random_range(0, 8);
        }
        while (board[move_o] == 'X' || board[move_o] == 'O');

        board[move_o] = 'O';

        if (ttt_check_win(board, 'O'))
        {
            ttt_print_board(board);
            print_error("Computer wins!\n\n");
            wait_key();
            return;
        }

        if (ttt_board_full(board))
        {
            ttt_print_board(board);
            print("It's a draw!\n\n");
            wait_key();
            return;
        }
    }
}


void games(void)
{
    print("\nGames:\n");

    print("  guess - Guess the Number\n");
    print("  guessh - Guess number (hard)\n");
    print("  rps   - Rock Paper Scissors\n");
    print("  word - Guess word\n");
    print("  hangman - Hangman\n");
    print("  tictactoe - Tic-Tac-Toe\n");
    print("  game coin - flip a coin\n");
    print("  game dice - roll a dice\n");
    print("  game highlow - guess higher or lower\n");
    print("  game math - game math\n");

    print("\n");
}

void testH(void)
{
    print_success("Hello!\n");
}

int read_int(void)
{
    char buffer[32];

    read_line(buffer, 32);

    return atoi_simple(buffer);
}

void print_int(int number)
{
    char buffer[12];
    int i = 0;

    if (number == 0)
    {
        putchar_os('0');
        return;
    }

    if (number < 0)
    {
        putchar_os('-');
        number = -number;
    }

    while (number > 0)
    {
        buffer[i] = '0' + (number % 10);
        number /= 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        putchar_os(buffer[i]);
    }
}

#define SIZE_KB 1024ULL
#define SIZE_MB (1024ULL * 1024ULL)
#define SIZE_GB (1024ULL * 1024ULL * 1024ULL)

void print_uint64(unsigned long long value)
{
    char buffer[24];
    int i = 0;

    if (value == 0)
    {
        putchar_os('0');
        return;
    }

    while (value > 0 && i < 24)
    {
        buffer[i++] = (char)('0' + (int)(value % 10));
        value /= 10;
    }

    while (i > 0)
    {
        i--;
        putchar_os(buffer[i]);
    }
}

static void print_size_unit(
    unsigned long long bytes,
    unsigned long long unit,
    int decimals,
    const char* suffix
)
{
    static const unsigned long long pow10[3] = { 1ULL, 10ULL, 100ULL };

    unsigned long long whole = bytes / unit;
    unsigned long long frac = 0;

    if (decimals > 0 && decimals <= 3)
    {
        frac = ((bytes % unit) * pow10[decimals - 1]) / unit;
    }

    print_uint64(whole);

    if (decimals > 0)
    {
        putchar_os('.');

        for (int i = decimals - 1; i >= 0; i--)
        {
            putchar_os((char)('0' + (int)((frac / pow10[i]) % 10)));
        }
    }

    print(" ");
    print(suffix);
}

static void print_size_human(unsigned long long bytes)
{
    if (bytes >= SIZE_GB)
        print_size_unit(bytes, SIZE_GB, 2, "GB");
    else if (bytes >= SIZE_MB)
        print_size_unit(bytes, SIZE_MB, 2, "MB");
    else if (bytes >= SIZE_KB)
        print_size_unit(bytes, SIZE_KB, 2, "KB");
    else
    {
        print_uint64(bytes);
        print(" B");
    }
}

void dateCr()
{
    set_color(COLOR_WHITE);
    print(OS_MADE_DATE "\n");
    print("v." OS_VERSION_STR "\n");
    set_color(base_color);
}

void game_word(void)
{
    char answer[32];

    print_title("\n=== GUESS THE WORD ===\n");
    print("Hint: programming language.\n");
    print("You have 3 attempts.\n\n");

    for (int attempt = 1; attempt <= 3; attempt++)
    {
        print("Answer: ");
        read_line(answer, 32);

        if (strcmp(answer, "Rust") == 0)
        {
            print_success("Correct! You win!\n");
            wait_key();
            return;
        }

        print_error("Wrong answer!\n");
    }

    print_error("The answer was: Rust\n");
    wait_key();
}
static void port_outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void port_outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static unsigned char port_inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
unsigned char rtc_read(unsigned char reg) {
    port_outb(0x70, reg);
    return port_inb(0x71);
}

unsigned char bcd_to_bin(unsigned char value) {
    return (value & 0x0F) + ((value / 16) * 10);
}

void get_time(int* hour, int* minute, int* second) {
    if (use_custom_time) {
        /* For custom time, just use RTC + offsets if set */
    }
    while (rtc_read(0x0A) & 0x80); 
    
    *second = bcd_to_bin(rtc_read(0x00));
    *minute = bcd_to_bin(rtc_read(0x02));
    *hour   = bcd_to_bin(rtc_read(0x04));
    
    int total = *hour + timezone_offset + custom_time_offset;
    while (total < 0) total += 24;
    while (total >= 24) total -= 24;
    *hour = total;
}

void get_date(int* day, int* month, int* year) {
    while (rtc_read(0x0A) & 0x80);
    
    if (custom_day != -1 && custom_month != -1 && custom_year != -1) {
        *day = custom_day;
        *month = custom_month;
        *year = custom_year;
        return;
    }
    
    *day   = bcd_to_bin(rtc_read(0x07));
    *month = bcd_to_bin(rtc_read(0x08));
    *year  = bcd_to_bin(rtc_read(0x09)) + 2000;
}
void print_two_digits(int value) {
    if (value < 10) putchar_os('0');
    print_int(value);
}

static void nwf_cpuid(unsigned int leaf, unsigned int* a, unsigned int* b,
                      unsigned int* c, unsigned int* d)
{
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(leaf), "c"(0));
}

void nwf_cpu_name(char* out)
{
    unsigned int a, b, c, d;

    nwf_cpuid(0x80000000, &a, &b, &c, &d);

    if (a >= 0x80000004)
    {
        unsigned int regs[12];

        for (int i = 0; i < 3; i++)
        {
            nwf_cpuid(0x80000002 + i,
                      &regs[i * 4], &regs[i * 4 + 1],
                      &regs[i * 4 + 2], &regs[i * 4 + 3]);
        }

        memcpy(out, regs, 48);
        out[48] = '\0';

        int s = 0;
        while (out[s] == ' ') s++;
        if (s)
        {
            int i = 0;
            while (out[s + i]) { out[i] = out[s + i]; i++; }
            out[i] = '\0';
        }

        int len = 0;
        while (out[len]) len++;
        while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
    }
    else
    {
        nwf_cpuid(0, &a, &b, &c, &d);
        memcpy(out,     &b, 4);
        memcpy(out + 4, &d, 4);
        memcpy(out + 8, &c, 4);
        out[12] = '\0';
    }
}

unsigned int nwf_ram_mb(void)
{
    unsigned int above16 = (unsigned int)rtc_read(0x34) |
                           ((unsigned int)rtc_read(0x35) << 8);

    if (above16)
        return 16 + above16 / 16;

    unsigned int ext = (unsigned int)rtc_read(0x30) |
                       ((unsigned int)rtc_read(0x31) << 8);

    return ext / 1024 + 1;
}

static void nwf_outl(unsigned short port, unsigned int v)
{
    __asm__ volatile ("outl %0, %1" : : "a"(v), "Nd"(port));
}

static unsigned int nwf_inl(unsigned short port)
{
    unsigned int r;
    __asm__ volatile ("inl %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static unsigned int nwf_pci_read(int bus, int slot, int func, int off)
{
    nwf_outl(0xCF8, 0x80000000u |
                    ((unsigned int)bus  << 16) |
                    ((unsigned int)slot << 11) |
                    ((unsigned int)func << 8)  |
                    (unsigned int)(off & 0xFC));
    return nwf_inl(0xCFC);
}

static int nwf_find_gpu(unsigned short* ven, unsigned short* dev)
{
    for (int bus = 0; bus < 256; bus++)
    {
        for (int slot = 0; slot < 32; slot++)
        {
            unsigned int id = nwf_pci_read(bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) continue;

            unsigned int hdr = (nwf_pci_read(bus, slot, 0, 0x0C) >> 16) & 0xFF;
            int funcs = (hdr & 0x80) ? 8 : 1;

            for (int func = 0; func < funcs; func++)
            {
                id = nwf_pci_read(bus, slot, func, 0x00);
                if ((id & 0xFFFF) == 0xFFFF) continue;

                unsigned int cls = nwf_pci_read(bus, slot, func, 0x08) >> 24;
                if (cls == 0x03)
                {
                    *ven = (unsigned short)(id & 0xFFFF);
                    *dev = (unsigned short)(id >> 16);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void nwf_print_hex16(unsigned short v)
{
    print_hex_byte((unsigned char)(v >> 8));
    print_hex_byte((unsigned char)(v & 0xFF));
}

static void nwf_print_gpu(void)
{
    unsigned short ven, dev;

    if (!nwf_find_gpu(&ven, &dev))
    {
        print("not detected");
        return;
    }

    if (ven == 0x1234 && dev == 0x1111) { print("QEMU Standard VGA"); return; }
    if (ven == 0x1B36 && dev == 0x0100) { print("QEMU QXL");          return; }
    if (ven == 0x1AF4 && dev == 0x1050) { print("Virtio GPU");        return; }
    if (ven == 0x15AD && dev == 0x0405) { print("VMware SVGA II");    return; }
    if (ven == 0x80EE && dev == 0xBEEF) { print("VirtualBox Graphics"); return; }

    switch (ven)
    {
        case 0x8086: print("Intel");  break;
        case 0x10DE: print("NVIDIA"); break;
        case 0x1002: print("AMD");    break;
        default:     print("Unknown"); break;
    }

    print(" GPU [");
    nwf_print_hex16(ven);
    putchar_os(':');
    nwf_print_hex16(dev);
    putchar_os(']');
}

void nwfetch(void)
{
    int hour, min, sec;
    int day, month, year;
    get_time(&hour, &min, &sec);
    get_date(&day, &month, &year);

    unsigned long long total_sec = timer_ms() / 1000;
    unsigned long long up_m = total_sec / 60;
    unsigned long long up_h = up_m / 60;
    up_m %= 60;
    total_sec %= 60;

    char cpu_name[49];
    nwf_cpu_name(cpu_name);

    unsigned int ram_mb = nwf_ram_mb();

    /* Logo: 14 rows, each exactly 20 characters wide */
    static const char* logo[14] =
    {
        "         /\\         ",
        "        /  \\        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "        |  |        ",
        "   ================ ",
        "        |  |        ",
        "        |  |        ",
        "        (  )        "
    };

    static const unsigned char logo_color[14] =
    {
        COLOR_WHITE, COLOR_WHITE, COLOR_LIGHT_CYAN, COLOR_LIGHT_CYAN,
        COLOR_LIGHT_CYAN, COLOR_LIGHT_CYAN, COLOR_LIGHT_CYAN, COLOR_LIGHT_CYAN,
        COLOR_LIGHT_CYAN, COLOR_LIGHT_CYAN, COLOR_YELLOW, COLOR_BROWN,
        COLOR_BROWN, COLOR_YELLOW
    };

    static const unsigned char palette[8] =
    {
        COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
        COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
    };

    print("\n");

    for (int row = 0; row < 14; row++)
    {
        set_color(logo_color[row]);
        print(logo[row]);
        print("  ");

        switch (row)
        {
            case 0:
                set_color(COLOR_CYAN);  print(cfg_user);
                set_color(COLOR_WHITE); print("@");
                set_color(COLOR_CYAN);  print(cfg_hostname);
                break;

            case 1:
                set_color(COLOR_LIGHT_GRAY);
                print("----------------------");
                break;

            case 2:
                set_color(COLOR_CYAN);  print("OS: ");
                set_color(COLOR_WHITE); print(cfg_os); print(" "); print(OS_VERSION_STR); print(" x86_64");
                break;

            case 3:
                set_color(COLOR_CYAN);  print("Kernel: ");
                set_color(COLOR_WHITE); print(OS_NAME " 64-bit");
                break;

            case 4:
                set_color(COLOR_CYAN);  print("Uptime: ");
                set_color(COLOR_WHITE);
                if (up_h > 0) { print_int((int)up_h); print("h "); }
                if (up_m > 0) { print_int((int)up_m); print("m "); }
                print_int((int)total_sec); print("s");
                break;

            case 5:
                set_color(COLOR_CYAN);  print("Shell: ");
                set_color(COLOR_WHITE); print("nwsh");
                break;

            case 6:
                set_color(COLOR_CYAN);  print("Display: ");
                set_color(COLOR_WHITE); print("VGA Text 80x50");
                break;

            case 7:
                set_color(COLOR_CYAN);  print("GPU: ");
                set_color(COLOR_WHITE); nwf_print_gpu();
                break;

            case 8:
                set_color(COLOR_CYAN);  print("Terminal: ");
                set_color(COLOR_WHITE); print(OS_NAME " Console");
                break;

            case 9:
                set_color(COLOR_CYAN);  print("CPU: ");
                set_color(COLOR_WHITE); print(cpu_name);
                break;

            case 10:
                set_color(COLOR_CYAN);  print("Memory: ");
                set_color(COLOR_WHITE);
                print_int((int)ram_mb); print(" MiB");
                break;

            case 11:
                set_color(COLOR_CYAN);  print("Network: ");
                set_color(COLOR_WHITE); print("e1000");
                break;

            case 12:
                set_color(COLOR_CYAN);  print("IP: ");
                set_color(COLOR_WHITE); print("10.0.2.15");
                break;

            case 13:
                set_color(COLOR_CYAN);  print("Date: ");
                set_color(COLOR_WHITE);
                print_two_digits(day);   putchar_os('.');
                print_two_digits(month); putchar_os('.');
                print_int(year);         putchar_os(' ');
                print_two_digits(hour);  putchar_os(':');
                print_two_digits(min);   putchar_os(':');
                print_two_digits(sec);
                break;
        }

        putchar_os('\n');
    }

    print("                      ");
    for (int i = 0; i < 8; i++)
    {
        set_color(palette[i]);
        print("##");
    }
    putchar_os('\n');

    set_color(base_color);
    print("\n");
}
void reboot(void)
{
    unsigned char status;

    do
    {
        __asm__ volatile (
            "inb %%dx, %%al"
            : "=a"(status)
            : "d"((unsigned short)0x64)
        );
    }
    while (status & 0x02);

    __asm__ volatile (
        "outb %%al, %%dx"
        :
        : "a"((unsigned char)0xFE),
          "d"((unsigned short)0x64)
    );

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

void poweroff(void)
{
    __asm__ volatile ("cli");

    port_outw(0x604, 0x2000);
    port_outw(0x604, 0x3400);
    port_outw(0xB004, 0x2000);
    port_outw(0x4004, 0x3400);

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

static DirEntry directory[MAX_FILES];

static int fs_current_drive = 0;
static int fs_quiet = 0;

/*
 * Sector access abstraction.
 *
 * fs_part_lba is the offset of the NwFS area inside the selected disk
 * (0 for the legacy whole-disk layout, 2048 for automatic MBR layouts).
 * In live mode all reads and writes go to a RAM-backed copy instead of
 * touching any real disk.
 */
static int fs_ram_mode = 0;
static unsigned int fs_part_lba = 0;
static int fs_live_build = 0;

#define FS_RAM_LBAS 8192
static unsigned char fs_ram_disk[FS_RAM_LBAS * 512];

static void fs_bytes_zero(unsigned char* p, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++) p[i] = 0;
}

static int fs_read_lba(unsigned int lba, unsigned char* buf)
{
    if (buf == 0) return -1;

    if (fs_ram_mode)
    {
        unsigned long long abs = (unsigned long long)fs_part_lba + lba;

        if (abs >= FS_RAM_LBAS)
        {
            fs_bytes_zero(buf, 512);
            return -1;
        }

        unsigned char* src = fs_ram_disk + (unsigned int)abs * 512u;

        for (unsigned int i = 0; i < 512; i++) buf[i] = src[i];

        return 0;
    }

    if (ata_drive_count() == 0) return -1;
    if (fs_current_drive < 0 || fs_current_drive >= ata_drive_count()) return -1;

    return ata_read_sector_d(fs_current_drive, fs_part_lba + lba, buf);
}

static int fs_write_lba(unsigned int lba, const unsigned char* buf)
{
    if (buf == 0) return -1;

    if (fs_ram_mode)
    {
        unsigned long long abs = (unsigned long long)fs_part_lba + lba;

        if (abs >= FS_RAM_LBAS) return -1;

        unsigned char* dst = fs_ram_disk + (unsigned int)abs * 512u;

        for (unsigned int i = 0; i < 512; i++) dst[i] = buf[i];

        return 0;
    }

    if (ata_drive_count() == 0) return -1;
    if (fs_current_drive < 0 || fs_current_drive >= ata_drive_count()) return -1;

    return ata_write_sector_d(fs_current_drive, fs_part_lba + lba, buf);
}

static int fs_drive_has_fs_at(int idx, unsigned int base)
{
    unsigned char sector[512];

    if (idx < 0 || idx >= ata_drive_count()) return 0;
    if (ata_read_sector_d(idx, base + FS_MAGIC_LBA, sector) != 0) return 0;

    return *(unsigned int*)sector == FS_MAGIC_VALUE &&
           *(unsigned int*)(sector + FS_VER_OFFSET) == FS_FORMAT_VER;
}

/*
 * Returns the start LBA of the NwFS area on a disk, or 0xFFFFFFFF if the
 * disk has no NwOS installation. Partitioned disks are checked first.
 */
static unsigned int fs_drive_fs_base(int idx)
{
    unsigned char mbr[512];

    if (idx < 0 || idx >= ata_drive_count()) return 0xFFFFFFFFu;

    if (ata_read_sector_d(idx, 0, mbr) == 0 &&
        mbr[510] == 0x55 && mbr[511] == 0xAA)
    {
        for (int p = 0; p < 4; p++)
        {
            unsigned char* entry = mbr + 446 + p * 16;

            if (entry[4] == 0) continue;

            unsigned int start =
                (unsigned int)entry[8] |
                ((unsigned int)entry[9] << 8) |
                ((unsigned int)entry[10] << 16) |
                ((unsigned int)entry[11] << 24);

            if (start == 0) continue;

            if (fs_drive_has_fs_at(idx, start)) return start;
        }
    }

    if (fs_drive_has_fs_at(idx, 0)) return 0;

    return 0xFFFFFFFFu;
}

void fs_set_drive(int idx)
{
    if (fs_ram_mode)
    {
        fs_current_drive = 0;
        return;
    }

    if (idx < 0 || idx >= ata_drive_count()) return;
    fs_current_drive = idx;
}

int fs_get_drive(void)
{
    return fs_current_drive;
}

void fs_load(void)
{
    unsigned char buffer[FS_DIR_SECTORS * 512];

    for (int i = 0; i < MAX_FILES; i++) {
        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].size = 0;
    }

    if (!fs_ram_mode && ata_drive_count() == 0) return;/* if no disk */

    for (int s = 0; s < FS_DIR_SECTORS; s++) {
        if (fs_read_lba(FS_DIR_LBA + s, buffer + s * 512) != 0) {
            print_error("Warning: FS read failed.\n");
            return;
        }
    }

    DirEntry* disk_entries = (DirEntry*)buffer;
    for (int i = 0; i < MAX_FILES; i++) {
        directory[i] = disk_entries[i];
        if (directory[i].used != 1) {
            directory[i].used = 0;
            directory[i].name[0] = '\0';
            directory[i].size = 0;
        }
    }
}
void fs_save_directory(void)
{
    if (!fs_ram_mode && ata_drive_count() == 0) return;
    unsigned char buffer[FS_DIR_SECTORS * 512];

    for (int k = 0; k < FS_DIR_SECTORS * 512; k++) buffer[k] = 0;

    DirEntry* disk_entries = (DirEntry*)buffer;

    for (int i = 0; i < MAX_FILES; i++)
    {
        disk_entries[i] = directory[i];
    }

    for (int s = 0; s < FS_DIR_SECTORS; s++)
    {
        fs_write_lba(FS_DIR_LBA + s, buffer + s * 512);
    }
}
static int fs_drive_has_fs(int idx)
{
    return fs_drive_fs_base(idx) != 0xFFFFFFFFu;
}

static const char* disk_position_name(ata_drive_t* d)
{
    if (d->channel == 0 && d->master) return "primary master";
    if (d->channel == 0)               return "primary slave";
    if (d->master)                     return "secondary master";

    return "secondary slave";
}

int fs_drive_index_by_letter(char letter)
{
    int n = ata_drive_count();

    if (letter >= 'a' && letter <= 'z')
        letter = (char)(letter - 'a' + 'A');

    for (int i = 0; i < n; i++)
    {
        ata_drive_t* d = ata_get_drive(i);

        if (d && d->letter == letter)
            return i;
    }

    return -1;
}

char fs_get_drive_letter(void)
{
    ata_drive_t* d = ata_get_drive(fs_current_drive);

    if (d) return d->letter;

    return 'C';
}

/*
 * Switches the active disk (the one every file operation works on).
 * The new disk must contain a NwOS filesystem, otherwise nothing changes.
 * Returns 0 on success, -1 on a bad index and -2 if the disk has no filesystem.
 */
int fs_change_drive(int idx)
{
    if (idx < 0 || idx >= ata_drive_count()) return -1;

    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return -2;

    if (idx != fs_current_drive)
    {
        fs_current_drive = idx;
        fs_part_lba = base;
        current_dir = 0;

        fs_load();
    }
    else
    {
        fs_part_lba = base;
    }

    return 0;
}

void change_p_color(void)
{
    int choose_c;

    print("Prompt color:\n");

    set_color(COLOR_CYAN);           print("1. cyan (default)\n");
    set_color(COLOR_RED);            print("2. red\n");
    set_color(COLOR_GREEN);          print("3. green\n");
    set_color(COLOR_BLUE);           print("4. blue\n");
    set_color(COLOR_MAGENTA);        print("5. magenta\n");
    set_color(COLOR_BROWN);          print("6. brown\n");
    set_color(COLOR_LIGHT_GRAY);     print("7. light gray\n");
    set_color(COLOR_DARK_GRAY);      print("8. dark gray\n");
    set_color(COLOR_LIGHT_BLUE);     print("9. light blue\n");
    set_color(COLOR_LIGHT_GREEN);    print("10. light green\n");
    set_color(COLOR_LIGHT_CYAN);     print("11. light cyan\n");
    set_color(COLOR_LIGHT_RED);      print("12. light red\n");
    set_color(COLOR_LIGHT_MAGENTA);  print("13. light magenta\n");
    set_color(COLOR_YELLOW);         print("14. yellow\n");
    set_color(COLOR_WHITE);          print("15. white\n");
    set_color(COLOR_BLACK);          print("16. black\n");

    set_color(base_color);
    print("Color: ");
    choose_c = read_int();

    if (choose_c < 1 || choose_c > 16)
    {
        print_error("Invalid choose!\n");
        return;
    }

    prompt_color = (unsigned char)(choose_c - 1);

    settings_save();

    print_success("Prompt color set\n");
    print("Preview: ");
    set_color(prompt_color);
    print_shell_p();
    set_color(base_color);
    putchar_os('\n');
}

void print_shell_p(void)
{
    print(shell_prompt_text());
}

void shell(void)
{
    char command[128];

    while (1)
    {
        set_color(prompt_color);
        print_shell_p();
        set_color(base_color);

        console_f7_enabled = 1;
        read_line(command, 128);
        console_f7_enabled = 0;

        shell_execute(command);
    }
}

int fs_change_drive_letter(char letter)
{
    int idx = fs_drive_index_by_letter(letter);

    if (idx < 0) return -1;

    return fs_change_drive(idx);
}

static unsigned char fs_scratch[FS_DIR_SECTORS * 512];

static int fs_read_directory_of(int idx, DirEntry* out)
{
    if (idx < 0 || idx >= ata_drive_count()) return -1;

    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return -1;

    for (int s = 0; s < FS_DIR_SECTORS; s++)
    {
        if (ata_read_sector_d(idx, base + FS_DIR_LBA + s, fs_scratch + s * 512) != 0)
            return -1;
    }

    DirEntry* disk_entries = (DirEntry*)fs_scratch;

    for (int i = 0; i < MAX_FILES; i++)
    {
        out[i] = disk_entries[i];

        if (out[i].used != 1)
        {
            out[i].used = 0;
            out[i].name[0] = '\0';
            out[i].size = 0;
        }
    }

    return 0;
}

static unsigned long long fs_drive_used_bytes(int idx, unsigned int* file_count)
{
    static DirEntry entries[MAX_FILES];

    unsigned long long total = 0;
    unsigned int files = 0;

    if (file_count) *file_count = 0;

    if (fs_read_directory_of(idx, entries) != 0) return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (!entries[i].used) continue;
        if (entries[i].type != FS_TYPE_FILE) continue;

        total += entries[i].size;
        files++;
    }

    if (file_count) *file_count = files;

    return total;
}

static unsigned int fs_drive_entry_count(int idx)
{
    static DirEntry entries[MAX_FILES];

    unsigned int count = 0;

    if (fs_read_directory_of(idx, entries) != 0) return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (entries[i].used) count++;
    }

    return count;
}

unsigned int nwf_disk_used_kb(void)
{
    return (unsigned int)(fs_drive_used_bytes(0, 0) / 1024ULL);
}

unsigned int nwf_disk_files(void)
{
    unsigned int files = 0;
    (void)fs_drive_used_bytes(0, &files);
    return files;
}

int nwf_tz_get(void)
{
    return timezone_offset;
}

void nwf_tz_set(int hours)
{
    if (hours < -12) hours = -12;
    if (hours > 14)  hours = 14;
    timezone_offset = hours;
}

int nwf_cpu_load(void)
{
    static unsigned long long prev_total = 0;
    static unsigned long long prev_idle  = 0;
    unsigned long long total = timer_us();
    unsigned long long idle  = idle_us_accum;
    unsigned long long dt;
    unsigned long long di;
    int load;

    if (prev_total == 0)
    {
        prev_total = total;
        prev_idle  = idle;
        return 0;
    }

    dt = total - prev_total;
    di = idle - prev_idle;
    prev_total = total;
    prev_idle  = idle;

    if (dt == 0)
        return 0;
    if (di > dt)
        di = dt;

    load = (int)(((dt - di) * 100ULL) / dt);
    if (load < 0) load = 0;
    if (load > 100) load = 100;
    return load;
}

int nwf_gpu_load(void)
{
    static unsigned long long prev_total = 0;
    static unsigned long long prev_gpu   = 0;
    unsigned long long total = timer_us();
    unsigned long long gpu   = fb_get_present_us();
    unsigned long long dt;
    unsigned long long dg;
    int load;

    if (prev_total == 0)
    {
        prev_total = total;
        prev_gpu   = gpu;
        return 0;
    }

    dt = total - prev_total;
    dg = gpu - prev_gpu;
    prev_total = total;
    prev_gpu   = gpu;

    if (dt == 0)
        return 0;
    if (dg > dt)
        dg = dt;

    load = (int)((dg * 100ULL) / dt);
    if (load < 0) load = 0;
    if (load > 100) load = 100;
    return load;
}

int nwf_fs_create(const char* name)
{
    if (name == 0 || name[0] == '\0') return 0;
    if (fs_find(name) != -1) return 0;

    fs_quiet = 1;
    fs_create(name);
    fs_quiet = 0;

    return fs_find(name) != -1;
}

int nwf_fs_mkdir(const char* name)
{
    if (name == 0 || name[0] == '\0') return 0;
    if (fs_find(name) != -1) return 0;

    fs_quiet = 1;
    fs_mkdir(name);
    fs_quiet = 0;

    return fs_find(name) != -1;
}

int nwf_fs_delete(const char* name)
{
    if (name == 0 || name[0] == '\0') return 0;
    if (fs_find(name) == -1) return 0;

    fs_quiet = 1;
    fs_delete(name);
    fs_quiet = 0;

    return fs_find(name) == -1;
}

int nwf_fs_write(const char* name, const char* text)
{
    int index;

    if (name == 0 || name[0] == '\0') return 0;
    if (text == 0) text = "";

    index = fs_find(name);
    fs_quiet = 1;

    if (index == -1)
    {
        fs_create(name);
        index = fs_find(name);
    }

    if (index == -1)
    {
        fs_quiet = 0;
        return 0;
    }

    fs_store_data(index, text, "File written.\n");
    fs_quiet = 0;
    return 1;
}

int nwf_fs_read(const char* name, char* buffer, unsigned int max_size)
{
    if (name == 0 || buffer == 0 || max_size == 0) return 0;
    return fs_read_text(name, buffer, max_size);
}

static unsigned int fs_drive_seq(int idx)
{
    unsigned char sector[512];
    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return 0;

    if (ata_read_sector_d(idx, base + FS_MAGIC_LBA, sector) != 0) return 0;

    return *(unsigned int*)(sector + FS_SEQ_OFFSET);
}

static unsigned int fs_max_seq(void)
{
    unsigned int max = 0;

    if (fs_ram_mode)
    {
        unsigned char sector[512];

        if (fs_read_lba(FS_MAGIC_LBA, sector) == 0 &&
            *(unsigned int*)sector == FS_MAGIC_VALUE)
        {
            return *(unsigned int*)(sector + FS_SEQ_OFFSET);
        }

        return 0;
    }

    for (int i = 0; i < ata_drive_count(); i++)
    {
        unsigned int seq = fs_drive_seq(i);

        if (seq > max) max = seq;
    }

    return max;
}

static int fs_find_drive_with_fs(void)
{
    int n = ata_drive_count();
    int best = -1;
    unsigned int best_seq = 0;

    for (int i = 0; i < n; i++)
    {
        if (!fs_drive_has_fs(i)) continue;

        unsigned int seq = fs_drive_seq(i);

        if (best < 0 || seq > best_seq)
        {
            best = i;
            best_seq = seq;
        }
    }

    return best;
}

static int fs_is_formatted(void) {
    unsigned char sector[512];
    if (fs_read_lba(FS_MAGIC_LBA, sector) != 0) return 0;
    return *(unsigned int*)sector == FS_MAGIC_VALUE &&
           *(unsigned int*)(sector + FS_VER_OFFSET) == FS_FORMAT_VER;
}

static void fs_mark_formatted(unsigned int seq) {
    unsigned char sector[512];
    for (int k = 0; k < 512; k++) sector[k] = 0;
    *(unsigned int*)sector = FS_MAGIC_VALUE;
    *(unsigned int*)(sector + FS_SEQ_OFFSET) = seq;
    *(unsigned int*)(sector + FS_VER_OFFSET) = FS_FORMAT_VER;
    fs_write_lba(FS_MAGIC_LBA, sector);
}

static void fs_wipe_magic(void) {
    unsigned char sector[512];
    for (int k = 0; k < 512; k++) sector[k] = 0;
    *(unsigned int*)sector = 0xDEADBEEF;
    fs_write_lba(FS_MAGIC_LBA, sector);
}

int fs_load_status(void) {
    if (ata_drive_count() == 0) return 0;

    int drive = fs_find_drive_with_fs();

    if (drive < 0) return 0;

    fs_set_drive(drive);
    fs_part_lba = fs_drive_fs_base(drive);

    fs_load();
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == 0 &&
            strcmp(directory[i].name, "death.bsod") == 0) {
            return 2;
        }
    }
    return 1;
}
static int fs_prepare_rootfs_path(
    const char *path,
    char leaf[MAX_FILENAME]
)
{
    if (path == 0 || path[0] == '\0' || leaf == 0)
        return -1;

    const char *last_slash = 0;

    for (const char *q = path; *q != '\0'; q++)
    {
        if (*q == '/')
            last_slash = q;
    }

    const char *leaf_src =
        last_slash ? last_slash + 1 : path;

    int leaf_len = 0;

    while (leaf_src[leaf_len] != '\0')
    {
        if (leaf_len >= MAX_FILENAME - 1)
            return -1;

        leaf[leaf_len] = leaf_src[leaf_len];
        leaf_len++;
    }

    leaf[leaf_len] = '\0';

    if (leaf_len == 0)
        return -1;

    const char *p = path;
    const char *end = last_slash ? last_slash : path;

    int parent = 0;

    while (p < end)
    {
        while (p < end && *p == '/')
            p++;

        if (p >= end)
            break;

        char part[MAX_FILENAME];
        int len = 0;

        while (p < end && *p != '/')
        {
            if (len >= MAX_FILENAME - 1)
                return -1;

            part[len++] = *p++;
        }

        part[len] = '\0';

        int child = -1;

        for (int i = 1; i < MAX_FILES; i++)
        {
            if (directory[i].used &&
                directory[i].parent == (uint16_t)parent &&
                strcmp(directory[i].name, part) == 0)
            {
                if (directory[i].type != FS_TYPE_DIR)
                    return -1;

                child = i;
                break;
            }
        }

        if (child == -1)
        {
            for (int i = 1; i < MAX_FILES; i++)
            {
                if (!directory[i].used)
                {
                    child = i;
                    break;
                }
            }

            if (child == -1)
                return -1;

            directory[child].used = 1;
            directory[child].type = FS_TYPE_DIR;
            directory[child].parent = (uint16_t)parent;
            directory[child].size = 0;

            for (int i = 0; i <= len; i++)
                directory[child].name[i] = part[i];
        }

        parent = child;
    }

    return parent;
}
static int fs_build_home_path(char* out, int capacity)
{
    const char* prefix = "/home/";
    int pos = 0;

    if (out == 0 || capacity < 1)
        return 0;

    while (*prefix)
    {
        if (pos >= capacity - 1)
            return 0;

        out[pos++] = *prefix++;
    }

    for (int i = 0; cfg_user[i] != '\0'; i++)
    {
        if (pos >= capacity - 1)
            return 0;

        out[pos++] = cfg_user[i];
    }

    out[pos] = '\0';
    return 1;
}

static int fs_ensure_dir_path(const char* path)
{
    char file_path[256];
    char leaf[MAX_FILENAME];
    int len = 0;

    if (path == 0 || path[0] == '\0')
        return -1;

    while (path[len] != '\0')
    {
        if (len >= 220)
            return -1;

        file_path[len] = path[len];
        len++;
    }

    while (len > 1 && file_path[len - 1] == '/')
        len--;

    if (len == 0)
        return -1;

    if (file_path[len - 1] != '/')
        file_path[len++] = '/';

    const char marker[] = ".nwos-dir";

    for (int i = 0; marker[i] != '\0'; i++)
        file_path[len++] = marker[i];

    file_path[len] = '\0';

    int parent = fs_prepare_rootfs_path(file_path, leaf);

    if (parent >= 0)
        fs_save_directory();

    return parent;
}

static int fs_create_user_home(void)
{
    char home[80];

    if (!fs_build_home_path(home, sizeof(home)))
        return -1;

    int home_index = fs_ensure_dir_path(home);

    current_dir = 0;

    return home_index;
}
void fs_format_disk(int drive_idx) {
    unsigned char zero[512];
    for (int k = 0; k < 512; k++) zero[k] = 0;

    fs_set_drive(drive_idx);
    fs_quiet = 1;

    unsigned int seq = fs_max_seq() + 1;

    for (int i = 0; i < MAX_FILES * FS_FILE_SECTORS; i++) {
        fs_write_lba(FS_DATA_LBA + i, zero);
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].size = 0;
    }

    directory[0].used = 1; directory[0].type = FS_TYPE_DIR; directory[0].parent = 0; strcpy(directory[0].name, "/");

    current_dir = 0;

    directory[0].used = 1;
    directory[0].type = FS_TYPE_DIR;
    directory[0].parent = 0;
    strcpy(directory[0].name, "/");
    directory[0].size = 0;

    fs_mkdir("sys");
    fs_mkdir("home");
    fs_mkdir("bin");

    for (int i = 0; i < ROOTFS_FILE_COUNT; i++)
    {
        const char* path = rootfs_files[i].path;
        const unsigned char* data = rootfs_files[i].data;
        unsigned int size = rootfs_files[i].size;

        char file_name[MAX_FILENAME];

        // removing limit

        // if (size > (unsigned int)(FS_FILE_SECTORS * 512U))
        // {
        //     print_error("File exceeds maximum size: ");
        //     print(path);
        //     putchar_os('\n');
        //     continue;
        // }

        int parent = fs_prepare_rootfs_path(path, file_name);

        if (parent < 0)
        {
            print_error("Cannot prepare rootfs path: ");
            print(path);
            putchar_os('\n');
            continue;
        }

        current_dir = (uint16_t)parent;

        if (!fs_live_build)
        {
            print("    [");
            print_int(i + 1);
            print("/");
            print_int(ROOTFS_FILE_COUNT);
            print("] ");
            print(path);
            print("  (");
            print_int((int)size);
            print(" bytes)\n");
        }

        if (fs_find(file_name) != -1)
            continue;

        fs_create(file_name);

        int idx = fs_find(file_name);

        if (idx == -1)
        {
            print_error("Cannot create file: ");
            print(path);
            putchar_os('\n');
            continue;
        }

        unsigned char sector[512];

        for (unsigned int s = 0; s < FS_FILE_SECTORS; s++)
        {
            unsigned int offset = s * 512U;

            if (offset >= size)
                break;

            for (int k = 0; k < 512; k++)
                sector[k] = 0;

            for (unsigned int k = 0;
                k < 512 && offset + k < size;
                k++)
            {
                sector[k] = data[offset + k];
            }

            if (fs_write_lba(
                    FS_DATA_LBA + (unsigned int)idx *
                        FS_FILE_SECTORS + s,
                    sector) != 0)
            {
                print_error("Disk write failed: ");
                print(path);
                putchar_os('\n');
                break;
            }
        }

        directory[idx].size = size;
    }

    current_dir = 0;

    fs_mark_formatted(seq);
    fs_save_directory();

    fs_quiet = 1;

    int home_ok = fs_create_user_home();

    settings_save();

    fs_quiet = 0;
    current_dir = 0;

    if (home_ok < 0)
        print_error("Warning: could not create user home directory.\n");

    if (!fs_live_build)
        print_success("Filesystem formatted. Rootfs installed.\n");
}

void fs_format(void) {
    fs_format_disk(fs_current_drive);
}

#define FS_PART_START_LBA 2048u

static unsigned int fs_layout_sectors(void)
{
    return 1u + FS_DIR_SECTORS + MAX_FILES * FS_FILE_SECTORS;
}

/*
 * Live boot: build the complete NwFS tree inside RAM. No sector of any
 * real disk is written while the system is running in this mode.
 */
static void fs_live_init(void)
{
    fs_ram_mode = 1;
    fs_part_lba = 0;
    fs_current_drive = 0;

    for (unsigned int i = 0; i < (unsigned int)FS_RAM_LBAS * 512u; i++)
        fs_ram_disk[i] = 0;

    cfg_require_login = 0;
    cfg_setup_complete = 1;

    fs_live_build = 1;
    fs_format_disk(0);
    fs_live_build = 0;

    current_dir = 0;
}

/*
 * Write a minimal MBR with one Linux-type partition. The NwFS area is
 * placed at FS_PART_START_LBA, leaving the first megabyte for a future
 * bootloader/ESP without changing the filesystem itself.
 */
static int fs_write_mbr_partition(int drive_idx, unsigned int start_lba,
                                  unsigned int total_sectors)
{
    unsigned char mbr[512];
    fs_bytes_zero(mbr, 512);

    if (total_sectors <= start_lba) return -1;

    unsigned int part_sectors = total_sectors - start_lba;
    unsigned char* e = mbr + 446;

    e[0] = 0x00; /* not marked bootable: the ISO remains the boot media */
    e[1] = 0x00;
    e[2] = 0x00;
    e[3] = 0x00;
    e[4] = 0x83; /* Linux filesystem type */
    e[5] = 0xFE;
    e[6] = 0xFF;
    e[7] = 0xFF;
    e[8]  = (unsigned char)(start_lba & 0xFF);
    e[9]  = (unsigned char)((start_lba >> 8) & 0xFF);
    e[10] = (unsigned char)((start_lba >> 16) & 0xFF);
    e[11] = (unsigned char)((start_lba >> 24) & 0xFF);
    e[12] = (unsigned char)(part_sectors & 0xFF);
    e[13] = (unsigned char)((part_sectors >> 8) & 0xFF);
    e[14] = (unsigned char)((part_sectors >> 16) & 0xFF);
    e[15] = (unsigned char)((part_sectors >> 24) & 0xFF);

    mbr[510] = 0x55;
    mbr[511] = 0xAA;

    return ata_write_sector_d(drive_idx, 0, mbr);
}

int fs_find(const char* name)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].parent == current_dir &&
            strcmp(directory[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int fs_resolve_dir_path(const char* path)
{
    if (path == 0 || path[0] == '\0')
        return -1;

    int dir = (path[0] == '/') ? 0 : current_dir;
    const char* p = path;

    while (*p)
    {
        while (*p == '/')
            p++;

        if (*p == '\0')
            break;

        char part[MAX_FILENAME];
        int len = 0;

        while (*p && *p != '/')
        {
            if (len >= MAX_FILENAME - 1)
                return -1;

            part[len++] = *p++;
        }

        part[len] = '\0';

        if (strcmp(part, ".") == 0)
            continue;

        if (strcmp(part, "..") == 0)
        {
            if (dir != 0)
                dir = directory[dir].parent;

            continue;
        }

        int found = -1;

        for (int i = 1; i < MAX_FILES; i++)
        {
            if (directory[i].used &&
                directory[i].type == FS_TYPE_DIR &&
                directory[i].parent == (uint16_t)dir &&
                strcmp(directory[i].name, part) == 0)
            {
                found = i;
                break;
            }
        }

        if (found == -1)
            return -1;

        dir = found;
    }

    return dir;
}

static void cmd_cd_path(const char* args)
{
    char target[256];
    int pos = 0;

    if (args == 0 || args[0] == '\0')
    {
        print_error("Usage: cd <path|~>\n");
        return;
    }

    if (args[0] == '~')
    {
        if (args[1] != '\0' && args[1] != '/')
        {
            print_error("Invalid home path.\n");
            return;
        }

        if (!fs_build_home_path(target, sizeof(target)))
        {
            print_error("Home path is too long.\n");
            return;
        }

        while (target[pos] != '\0')
            pos++;

        for (int i = 1; args[i] != '\0'; i++)
        {
            if (pos >= (int)sizeof(target) - 1)
            {
                print_error("Path is too long.\n");
                return;
            }

            target[pos++] = args[i];
        }

        target[pos] = '\0';
    }
    else
    {
        int i = 0;

        while (args[i] != '\0')
        {
            if (i >= (int)sizeof(target) - 1)
            {
                print_error("Path is too long.\n");
                return;
            }

            target[i] = args[i];
            i++;
        }

        target[i] = '\0';
    }

    int dir = fs_resolve_dir_path(target);

    if (dir < 0)
    {
        print_error("Directory not found: ");
        print(target);
        putchar_os('\n');
        return;
    }

    current_dir = (uint16_t)dir;
}

static void run_startup_nwo(const char* filename)
{
    uint16_t saved_dir = current_dir;
    int bin_index = -1;

    if (filename == 0 || filename[0] == '\0')
        return;

    current_dir = 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 &&
            strcmp(directory[i].name, "bin") == 0)
        {
            bin_index = i;
            break;
        }
    }

    if (bin_index == -1)
    {
        print_error("[INIT] /bin not found.\n");
        current_dir = saved_dir;
        return;
    }

    current_dir = (uint16_t)bin_index;

    int index = fs_find(filename);

    if (index == -1)
    {
        print_error("[INIT] Application not found: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    if (directory[index].type != FS_TYPE_FILE)
    {
        print_error("[INIT] Not a file: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    print("[INIT] Starting /bin/");
    print(filename);
    putchar_os('\n');

    nwo_run(filename);

    current_dir = saved_dir;
}

static void run_nwo_from_dir(
    const char* dir_path,
    const char* filename
)
{
    if (!dir_path || !filename ||
        dir_path[0] == '\0' || filename[0] == '\0')
    {
        print_error("[INIT] Invalid application path.\n");
        return;
    }

    for (int i = 0; filename[i] != '\0'; i++)
    {
        if (filename[i] == '/')
        {
            print_error("[INIT] Use a filename, not a full path.\n");
            return;
        }
    }

    uint16_t saved_dir = current_dir;

    int dir = fs_resolve_dir_path(dir_path);

    if (dir < 0)
    {
        print_error("[INIT] Directory not found: ");
        print(dir_path);
        putchar_os('\n');
        return;
    }

    current_dir = (uint16_t)dir;

    int index = fs_find(filename);

    if (index < 0)
    {
        print_error("[INIT] Application not found: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    if (directory[index].type != FS_TYPE_FILE)
    {
        print_error("[INIT] Not a file: ");
        print(filename);
        putchar_os('\n');

        current_dir = saved_dir;
        return;
    }

    print("[INIT] Running ");
    print(dir_path);
    print("/");
    print(filename);
    putchar_os('\n');

    nwo_run(filename);

    current_dir = saved_dir;
}

int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size
);

unsigned int fs_get_file_size(const char* name)
{
    int index = fs_find(name);

    if (index == -1)
        return 0;

    return directory[index].size;
}

static void print_hex_byte(unsigned char b)
{
    const char hex[] = "0123456789ABCDEF";
    putchar_os(hex[(b >> 4) & 0x0F]);
    putchar_os(hex[b & 0x0F]);
}
void app_hexview(const char* filename)
{
    static unsigned char buffer[512];

    if (!filename || filename[0] == '\0')
    {
        print_error("Usage: hexview <filename>\n");
        return;
    }

    if (!fs_read_binary(filename, buffer, sizeof(buffer)))
    {
        print_error("File not found or read error.\n");
        return;
    }

    print_success("\n--- HEX DUMP: ");
    print(filename);
    print(" ---\n\n");

    print("OFFSET   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  | ASCII\n");
    print("-------  -----------------------------------------------  | ----------------\n");

    for (int offset = 0; offset < 512; offset += 16)
    {
        print_hex_byte((offset >> 8) & 0xFF);
        print_hex_byte(offset & 0xFF);
        print("     ");
        for (int i = 0; i < 16; i++)
        {
            print_hex_byte(buffer[offset + i]);
            putchar_os(' ');
        }

        print(" | ");
        for (int i = 0; i < 16; i++)
        {
            unsigned char c = buffer[offset + i];
            if (c >= 32 && c <= 126)
            {
                putchar_os((char)c);
            }
            else
            {
                putchar_os('.');
            }
        }

        putchar_os('\n');
    }

    print_success("\n--- END OF DUMP ---\n\n");
}
void app_nwo_info(const char* filename)
{
    static unsigned char buffer[512];

    if (!filename || filename[0] == '\0')
    {
        print_error("Usage: nwocheck <file.nwo>\n");
        return;
    }

    if (!fs_read_binary(filename, buffer, sizeof(buffer)))
    {
        print_error("Failed to read NWO binary.\n");
        return;
    }

    print_success("\n=== NWO EXECUTABLE INSPECTOR ===\n");
    print("File: ");
    print(filename);
    print("\n\nFirst 16 Opcodes / Raw Bytes:\n[ ");

    for (int i = 0; i < 16; i++)
    {
        print_hex_byte(buffer[i]);
        putchar_os(' ');
    }
    print("]\n\n");

    print("Entry Opcode / Byte: 0x");
    print_hex_byte(buffer[0]);
    print("\nStatus: Valid binary image loaded into memory.\n");
    print("===============================\n\n");
}

void generate_num(void) {
    while (1) {
        unsigned int random_num = random_range(1, 100);
        print("Generated random number: ");
        set_color(COLOR_GREEN);
        print_int(random_num);
        set_color(base_color);
        print("\n");
        char ag[5];
        print("Do you want again? y/n\n");
        read_line(ag, 5);
        if (strcmp(ag, "y") == 0) { }
        else if (strcmp(ag, "n") == 0) {
            break;
        }
        else {
            print_error("Invalid command! automaticaly 'y'\n");
        }
    }
}

void sleep(unsigned int ms)
{
    if (ms == 0) return;

    unsigned long long idle_start = timer_us();
    unsigned long long start = timer_ms();
    unsigned long long target = start + ms;

    while (timer_ms() < target)
    {
        __asm__ volatile ("pause");
    }

    idle_us_accum += timer_us() - idle_start;
}

#define MENU_INNER 34

static void menu_border(void)
{
    print("                    +");
    for (int i = 0; i < MENU_INNER; i++) putchar_os('-');
    print("+\n");
}

static void menu_line(const char* text)
{
    print("                    |");
    int n = 0;
    while (text[n]) { putchar_os(text[n]); n++; }
    while (n < MENU_INNER) { putchar_os(' '); n++; }
    print("|\n");
}

void games_menu(void)
{
    char choice[8];

    while (1)
    {
        clear();

        print("\n\n");
        set_color(COLOR_CYAN);
        menu_border();
        menu_line("            NWOS ARCADE");
        menu_border();
        set_color(base_color);
        print("\n");

        menu_border();
        menu_line("           GAMES MENU");
        menu_border();
        menu_line("  [1]  Guess Number");
        menu_line("  [2]  Guess Word");
        menu_line("  [3]  Rock Paper Scissors");
        menu_line("  [4]  Coin Flip");
        menu_line("  [5]  Dice Roll");
        menu_line("  [6]  Higher / Lower");
        menu_line("  [7]  Math Quiz");
        menu_line("  [8]  Hangman");
        menu_line("  [9]  Tic-Tac-Toe");
        menu_line("  [10] Memory Match");
        menu_line("  [11] Slot Machine");
        menu_line("  [12] Snake");
        menu_line("  [13] Guess Hard");
        menu_border();
        menu_line("  [0]  Back to Shell");
        menu_border();
        print("\n");

        print("                      -----> ");

        read_line(choice, 8);

        if (strcmp(choice, "0") == 0)
        {
            clear();
            return;
        }
        else if (strcmp(choice, "1") == 0)
        {
            clear();
            game_guess();
        }
        else if (strcmp(choice, "2") == 0)
        {
            clear();
            game_word();
        }
        else if (strcmp(choice, "3") == 0)
        {
            clear();
            game_rps();
        }
        else if (strcmp(choice, "4") == 0)
        {
            clear();
            game_coin();
        }
        else if (strcmp(choice, "5") == 0)
        {
            clear();
            game_dice();
        }
        else if (strcmp(choice, "6") == 0)
        {
            clear();
            game_higher_lower();
        }
        else if (strcmp(choice, "7") == 0)
        {
            clear();
            game_math();
        }
        else if (strcmp(choice, "8") == 0)
        {
            clear();
            game_hangman();
        }
        else if (strcmp(choice, "9") == 0)
        {
            clear();
            game_tictactoe();
        }
        else if (strcmp(choice, "10") == 0)
        {
            clear();
            game_memory();
        }
        else if (strcmp(choice, "11") == 0)
        {
            clear();
            game_slots();
        }
        else if (strcmp(choice, "12") == 0)
        {
            clear();
            game_snake();
        }
        else if (strcmp(choice, "13") == 0)
        {
            clear();
            game_guessh();
        }
        else
        {
            print_error("\nInvalid choice.\n");
            sleep(1000);
        }
    }
}

static uint16_t fs_removed[MAX_FILES];
static int fs_removed_count = 0;

static void fs_entry_release(int index)
{
    if (fs_removed_count < MAX_FILES)
        fs_removed[fs_removed_count++] = (uint16_t)index;

    directory[index].used = 0;
    directory[index].type = 0;
    directory[index].parent = 0;
    directory[index].name[0] = '\0';
    directory[index].size = 0;
}

static void fs_entry_release_tree(int index, int depth)
{
    if (depth > 32) return;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (i != index && directory[i].used &&
            directory[i].parent == index)
        {
            fs_entry_release_tree(i, depth + 1);
        }
    }

    fs_entry_release(index);
}

static void fs_release_finish(uint16_t fallback)
{
    for (int i = 0; i < fs_removed_count; i++)
    {
        if (current_dir == fs_removed[i])
        {
            current_dir = fallback;
            return;
        }
    }
}

static int fs_name_copy(const char* name, char* buf, int max)
{
    int n = 0;

    while (name[n] != '\0' && n < max - 1) n++;

    while (n > 0 && name[n - 1] == '/') n--;

    for (int i = 0; i < n; i++) buf[i] = name[i];

    buf[n] = '\0';

    return n;
}

static int fs_entry_in_tree(int index, const char* root_name)
{
    int current = index;

    for (int depth = 0;
         depth < MAX_FILES && current > 0;
         depth++)
    {
        if (current >= MAX_FILES || !directory[current].used)
            return 0;

        if (directory[current].parent == 0 &&
            strcmp(directory[current].name, root_name) == 0)
        {
            return 1;
        }

        current = directory[current].parent;
    }

    return 0;
}

static int fs_entry_in_sys_apps_tree(int index)
{
    int sys_index = -1;
    int apps_index = -1;

    if (index < 0 || index >= MAX_FILES || !directory[index].used)
        return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 &&
            strcmp(directory[i].name, "sys") == 0)
        {
            sys_index = i;
            break;
        }
    }

    if (sys_index < 0)
        return 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == (uint16_t)sys_index &&
            strcmp(directory[i].name, "apps") == 0)
        {
            apps_index = i;
            break;
        }
    }

    if (apps_index < 0)
        return 0;

    int current = index;
    for (int depth = 0; depth < MAX_FILES; depth++)
    {
        if (current == apps_index)
            return 1;

        if (current <= 0 || current >= MAX_FILES ||
            !directory[current].used)
            return 0;

        current = directory[current].parent;
    }

    return 0;
}

static int fs_name_ends_with(const char* name, const char* suffix)
{
    int name_len = 0;
    int suffix_len = 0;

    if (name == 0 || suffix == 0)
        return 0;

    while (name[name_len] != '\0') name_len++;
    while (suffix[suffix_len] != '\0') suffix_len++;

    if (suffix_len > name_len)
        return 0;

    return strcmp(name + name_len - suffix_len, suffix) == 0;
}

static int fs_entry_requires_admin(int index)
{
    if (index <= 0 || index >= MAX_FILES ||
        !directory[index].used)
    {
        return 0;
    }

    /* Top-level system directories always remain protected. */
    if (directory[index].parent == 0 &&
        directory[index].type == FS_TYPE_DIR &&
        (strcmp(directory[index].name, "sys") == 0 ||
         strcmp(directory[index].name, "home") == 0 ||
         strcmp(directory[index].name, "bin") == 0))
    {
        return 1;
    }

    /*
     * Compiled NWO programs are normally application files, even when they
     * are stored outside /sys/apps. Keep the known startup GUI binary protected.
     */
    if (directory[index].type == FS_TYPE_FILE &&
        fs_name_ends_with(directory[index].name, ".nwo"))
    {
        if (strcmp(directory[index].name, "gui.nwo") == 0)
            return 1;

        return 0;
    }

    /* Do not prompt for other ordinary application files under /sys/apps. */
    if (directory[index].type == FS_TYPE_FILE &&
        fs_entry_in_sys_apps_tree(index))
    {
        return 0;
    }

    /* Non-binary content in system trees still requires authorization. */
    if (fs_entry_in_tree(index, "sys") ||
        fs_entry_in_tree(index, "bin"))
    {
        return 1;
    }

    return 0;
}

static int authorize_sensitive_action(
    const char* description,
    const char* confirmation
)
{
    print_error("\nWARNING: dangerous operation!\n");
    print(description);
    print("\nThis action may make NwOS unusable.\n");

    if (!cmd_sudo && !authenticate_root_password())
        return 0;

    print("To confirm, type exactly: ");
    print(confirmation);
    print("\n> ");

    char answer[40];
    read_line(answer, sizeof(answer));

    if (strcmp(answer, confirmation) != 0)
    {
        print("Operation cancelled.\n");
        return 0;
    }

    return 1;
}


void fs_rmdir(const char* name)
{
    char buf[MAX_FILENAME + 1];

    if (name == 0 || name[0] == '\0')
    {
        print_error("Usage: rmdir <directory>\n");
        return;
    }

    if (fs_name_copy(name, buf, sizeof(buf)) == 0)
    {
        print_error("Cannot remove the root directory.\n");
        return;
    }

    int index = fs_find(buf);

    if (index == -1)
    {
        print_error("Directory not found.\n");
        return;
    }

    if (index == 0)
    {
        print_error("Cannot remove the root directory.\n");
        return;
    }

    if (directory[index].type != FS_TYPE_DIR)
    {
        print_error("Not a directory. Use 'rm' to remove files.\n");
        return;
    }

    if (fs_entry_requires_admin(index))
    {
        if (!authorize_sensitive_action(
                "Deleting a protected system directory",
                directory[index].name))
        {
            return;
        }
    }

    uint16_t parent = directory[index].parent;

    fs_removed_count = 0;
    fs_entry_release_tree(index, 0);
    fs_release_finish(parent);

    fs_save_directory();

    print_success("Directory removed.\n");
}

static void fs_rm_one(const char* name, int recursive, int force)
{
    int index = fs_find(name);

    if (index == -1)
    {
        if (!force)
            print_error("No such file or directory: ");
        if (!force)
        {
            print(name);
            putchar_os('\n');
        }
        return;
    }

    /* Never let rm recursively erase the filesystem root. */
    if (index == 0)
    {
        print_error("Cannot remove the root directory.\n");
        return;
    }

    if (directory[index].type == FS_TYPE_DIR)
    {
        if (!recursive)
        {
            print_error("Is a directory. Use 'rm -r' or 'rmdir'.\n");
            return;
        }

        /* Ask for password + exact confirmation only for protected paths. */
        if (fs_entry_requires_admin(index))
        {
            if (!authorize_sensitive_action(
                    "Deleting a protected system directory",
                    directory[index].name))
                return;
        }

        /* Preserve the original directory-removal sequence. */
        uint16_t parent = directory[index].parent;
        fs_removed_count = 0;
        fs_entry_release_tree(index, 0);
        fs_release_finish(parent);
        fs_save_directory();

        print_success("Directory removed: ");
        print(name);
        putchar_os('\n');
        return;
    }

    if (fs_entry_requires_admin(index))
    {
        if (!authorize_sensitive_action(
                "Deleting a protected system file",
                directory[index].name))
            return;
    }

    /* Preserve the original file-removal sequence. */
    fs_removed_count = 0;
    fs_entry_release(index);
    fs_save_directory();
    print_success("File removed: ");
    print(name);
    putchar_os('\n');
}

void fs_rm(const char* args)
{
    const char* p = args ? args : "";
    int recursive = 0;
    int force = 0;
    int targets = 0;

    /* Preserve the original rm option parsing. */
    while (*p == ' ' || *p == '\t') p++;

    while (*p == '-')
    {
        p++;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',')
        {
            if (*p == 'r' || *p == 'R') recursive = 1;
            else if (*p == 'f' || *p == 'F') force = 1;
            p++;
        }
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
    }

    /* Every whitespace/comma-separated token is a separate target. */
    while (*p != '\0')
    {
        char name[MAX_FILENAME];
        int n = 0;
        int too_long = 0;

        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == '\0') break;

        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',')
        {
            if (n < MAX_FILENAME - 1)
                name[n++] = *p;
            else
                too_long = 1;
            p++;
        }
        name[n] = '\0';

        while (n > 0 && name[n - 1] == '/')
            name[--n] = '\0';

        if (n == 0)
            continue;

        targets++;

        if (too_long)
        {
            print_error("Filename is too long (maximum 31 characters).\n");
            continue;
        }

        fs_rm_one(name, recursive, force);
    }

    if (targets == 0)
        print_error("Usage: rm [-r] [-f] <file1> [file2 ...]\n");
}

static void cmd_rm_root(void)
{
    if (!authorize_sensitive_action(
            "Deleting everything inside the root directory",
            "/"))
    {
        return;
    }

    current_dir = 0;
    fs_removed_count = 0;

    for (int i = 1; i < MAX_FILES; i++)
    {
        if (directory[i].used &&
            directory[i].parent == 0)
        {
            fs_entry_release_tree(i, 0);
        }
    }

    current_dir = 0;
    fs_save_directory();

    print_success("Root directory contents removed.\n");
}

int fs_read_text(
    const char* name,
    char* buffer,
    unsigned int max_size
)
{
    int index;
    unsigned char sector[512];
    unsigned int size;
    unsigned int copy_size;
    unsigned int copied = 0;

    if (buffer == 0 || max_size == 0)
        return 0;

    buffer[0] = '\0';

    index = fs_find(name);

    if (index == -1)
        return 0;

    size = directory[index].size;
    if (size >= max_size)
        copy_size = max_size - 1;
    else
        copy_size = size;

    while (copied < copy_size)
    {
        unsigned int sector_index = copied / 512;
        unsigned int sector_offset = copied % 512;
        unsigned int chunk = 512 - sector_offset;

        if (chunk > copy_size - copied)
            chunk = copy_size - copied;

        fs_read_lba(
            FS_DATA_LBA + index * FS_FILE_SECTORS + sector_index,
            sector
        );

        for (unsigned int i = 0; i < chunk; i++)
            buffer[copied + i] = (char)sector[sector_offset + i];

        copied += chunk;
    }

    buffer[copy_size] = '\0';

    return 1;
}

void startingNWOS() {
    fb_init();
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("FB\n");
    random_init();
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("RANDOM\n");
    history_load();
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("HISTORY\n");
    ata_init();
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("ATA\n");
    fs_load();
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("FILESYSTEM\n");
    mouse_init();
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("KEYBOARD\n");
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("MOUSE\n");
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("COMPILER\n");
    set_color(COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("SYSTEM\n");
}

int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size)
{
    int index;

    if (name == 0 ||
        buffer == 0 ||
        max_size == 0)
    {
        return 0;
    }

    index = fs_find(name);

    if (index == -1)
        return 0;
    if (max_size > FS_FILE_SECTORS * 512)
        max_size = FS_FILE_SECTORS * 512;

    unsigned int size = directory[index].size;
    unsigned int copy_size = size < max_size ? size : max_size;
    unsigned int copied = 0;

    while (copied < copy_size)
    {
        unsigned char sector[512];
        unsigned int sector_index = copied / 512;
        unsigned int sector_offset = copied % 512;
        unsigned int chunk = 512 - sector_offset;

        if (chunk > copy_size - copied)
            chunk = copy_size - copied;

        fs_read_lba(
            FS_DATA_LBA + (unsigned int)index * FS_FILE_SECTORS + sector_index,
            sector);

        for (unsigned int i = 0; i < chunk; i++)
            buffer[copied + i] = sector[sector_offset + i];

        copied += chunk;
    }

    return 1;
}

void t_colorgreen(void)
{
    set_base_color(COLOR_GREEN);
    settings_save();
    print("setted color to green\n");
}
void t_colorred(void)
{
    set_base_color(COLOR_RED);
    settings_save();
    print("setted color to red\n");
}
void t_colorgray(void)
{
    set_base_color(COLOR_LIGHT_GRAY);
    settings_save();
    print("setted color to gray\n");
}
void t_colorblue(void)
{
    set_base_color(COLOR_BLUE);
    settings_save();
    print("setted color to blue\n");
}
void t_colorcyan(void)
{
    set_base_color(COLOR_CYAN);
    settings_save();
    print("setted color to cyan\n");
}

void t_colorsetter(void)
{
    int chooseT;
    print("1.green\n");
    print("2.red\n");
    print("3.gray (Default)\n");
    print("4.blue\n");
    print("5.cyan\n");
    chooseT = read_int();
    if (chooseT == 1)
    {
        t_colorgreen();
    }
    if (chooseT == 2)
    {
        t_colorred();
    }
    if (chooseT == 3)
    {
        t_colorgray();
    }
    if (chooseT == 4)
    {
        t_colorblue();
    }
    if (chooseT == 5)
    {
        t_colorcyan();
    }
}

// shell

typedef struct
{
    const char* name;
    void (*run)(void);
    void (*run_arg)(const char* args);
    const char* help;
} Command;

#define CMD(name, fn, help)     { name, fn, 0, help }
#define CMD_ARG(name, fn, help) { name, 0, fn, help }

static void cmd_help(void);
static void cmd_about(void);
static void cmd_echo(const char* args);
static void cmd_touch(const char* args);
static void cmd_write(const char* args);
static void cmd_bash(const char* args);
static void cmd_bat(const char* args);
static void cmd_nwc(const char* args);
static void cmd_panic_safe(void);
static void cmd_panic_fatal(void);
static void cmd_mem(void);
static void cmd_cpu(void);
static void cmd_uptime(void);
static void cmd_time_cmd(void);
static void cmd_zones(void);
static void cmd_timezone(const char* args);
static void cmd_date_set(const char* args);
static void cmd_more_help(void);
static void cmd_drive(const char* args);
static void cmd_drive_status(void);
static void cmd_disk_info(const char* args);


static void cmd_disks(void)
{
    int n = ata_drive_count();
    if (n == 0) {
        print_error("No ATA disks detected.\n");
        print("Add one in QEMU:\n");
        print("  -drive file=disk.img,format=raw,if=ide,index=0\n");
        print("(also: index=1, 2, 3 for slave/secondary)\n");
        return;
    }

    print_title("\n=== DISKS ===\n\n");

    for (int i = 0; i < n; i++) {
        ata_drive_t* d = ata_get_drive(i);
        unsigned long long total = (unsigned long long)d->sectors * 512ULL;

        if (i == fs_current_drive) set_color(COLOR_LIGHT_GREEN);
        else set_color(base_color);

        putchar_os(d->letter);
        print(":");

        if (i == fs_current_drive) print(" *");
        else print("  ");

        set_color(base_color);

        print("  ");
        print(disk_position_name(d));
        print("  ");

        print_uint64(total);
        print(" B  ");

        print_size_human(total);

        if (fs_drive_has_fs(i))
        {
            set_color(COLOR_LIGHT_CYAN);
            print("  [");
            print(OS_NAME);
            print("]");
            set_color(base_color);
        }

        putchar_os('\n');
    }

    putchar_os('\n');

    print("  * = active disk\n");
    print("  'drive <letter|number>' switches the active disk\n");
    print("  'disk <letter|number>' shows the detailed size of one disk\n");
    putchar_os('\n');
}

#include "kernel.h"

void fs_store_data(int index, const char* text, const char* success_message)
{
    static unsigned char buffer[FS_FILE_SECTORS * 512];

    int i = 0;

    while (text[i] != '\0' &&
           i < MAX_FILE_SIZE - 1)
    {
        buffer[i] = text[i];
        i++;
    }

    buffer[i] = '\0';

    for (int k = i + 1; k < FS_FILE_SECTORS * 512; k++)
    {
        buffer[k] = 0;
    }

    directory[index].size = (unsigned int)i;

    for (int s = 0; s < FS_FILE_SECTORS; s++)
    {
        fs_write_lba(
            FS_DATA_LBA + index * FS_FILE_SECTORS + s,
            buffer + s * 512);
    }

    fs_save_directory();

    if (text[i] != '\0')
    {
        print_error("Text is too long. Maximum is 20480 characters.\n");
        return;
    }

    if (!fs_quiet) print_success(success_message);
}

void fs_write(const char* name, const char* text)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    fs_store_data(index, text, "File written.\n");
}


/* Write NwC output as binary; unlike fs_store_data(), this preserves zero bytes. */
int fs_write_binary(const char* name, const unsigned char* data, unsigned int size)
{
    static unsigned char sector[512];
    int index;
    int previous_quiet;
    unsigned int i;
    unsigned int sector_index;

    if (name == 0 || data == 0 || size > MAX_FILE_SIZE ||
        size > FS_FILE_SECTORS * 512u)
        return 0;

    index = fs_find(name);
    if (index == -1)
    {
        previous_quiet = fs_quiet;
        fs_quiet = 1;
        fs_create(name);
        fs_quiet = previous_quiet;
        index = fs_find(name);
    }
    if (index < 0 || directory[index].type != FS_TYPE_FILE)
        return 0;

    directory[index].size = size;
    for (sector_index = 0; sector_index < FS_FILE_SECTORS; ++sector_index)
    {
        for (i = 0; i < 512; ++i)
        {
            unsigned int offset = sector_index * 512u + i;
            sector[i] = offset < size ? data[offset] : 0;
        }
        fs_write_lba(
            FS_DATA_LBA + (unsigned int)index * FS_FILE_SECTORS + sector_index,
            sector);
    }
    fs_save_directory();
    return 1;
}

static int fs_split_file_path(const char* path, char* dir_out,
                              int dir_capacity, char* leaf_out)
{
    int last_slash = -1;
    int length = 0;

    if (path == 0 || dir_out == 0 || leaf_out == 0)
        return 0;

    while (path[length] != '\0')
    {
        if (path[length] == '/')
            last_slash = length;
        length++;
    }

    if (last_slash < 0)
        return 0;

    int dir_length = (last_slash == 0) ? 1 : last_slash;

    if (dir_length >= dir_capacity)
        return 0;

    if (last_slash == 0)
    {
        dir_out[0] = '/';
    }
    else
    {
        for (int i = 0; i < dir_length; i++)
            dir_out[i] = path[i];
    }

    dir_out[dir_length] = '\0';

    const char* leaf = path + last_slash + 1;
    int leaf_length = 0;

    while (leaf[leaf_length] != '\0')
    {
        if (leaf_length >= MAX_FILENAME - 1)
            return 0;
        leaf_out[leaf_length] = leaf[leaf_length];
        leaf_length++;
    }

    leaf_out[leaf_length] = '\0';
    return leaf_length > 0;
}

int fs_path_is_file(const char* path)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int index;

    if (!fs_split_file_path(path, dir_path, sizeof(dir_path), leaf))
        return 0;

    parent = fs_resolve_dir_path(dir_path);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = fs_find(leaf);
    current_dir = saved_dir;

    return index >= 0 && directory[index].type == FS_TYPE_FILE;
}

int fs_write_binary_path(const char* path, const unsigned char* data,
                         unsigned int size)
{
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int result;

    if (path == 0)
        return 0;

    parent = fs_prepare_rootfs_path(path, leaf);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    result = fs_write_binary(leaf, data, size);
    current_dir = saved_dir;

    return result;
}

int fs_delete_file_path(const char* path)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int index;

    if (!fs_split_file_path(path, dir_path, sizeof(dir_path), leaf))
        return 0;

    parent = fs_resolve_dir_path(dir_path);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = fs_find(leaf);

    if (index < 0 || directory[index].type != FS_TYPE_FILE)
    {
        current_dir = saved_dir;
        return 0;
    }

    directory[index].used = 0;
    directory[index].name[0] = '\0';
    directory[index].size = 0;
    fs_save_directory();
    current_dir = saved_dir;

    return 1;
}

int fs_run_nwo_path(const char* path)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    uint16_t saved_dir;
    int parent;
    int index;

    if (!fs_split_file_path(path, dir_path, sizeof(dir_path), leaf))
        return 0;

    parent = fs_resolve_dir_path(dir_path);
    if (parent < 0)
        return 0;

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = fs_find(leaf);

    if (index < 0 || directory[index].type != FS_TYPE_FILE)
    {
        current_dir = saved_dir;
        return 0;
    }

    nwo_run(leaf);
    current_dir = saved_dir;

    return 1;
}

void fs_read(const char* name)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    static unsigned char buffer[MAX_FILE_SIZE];

    if (!fs_read_text(directory[index].name, (char*)buffer, sizeof(buffer)))
    {
        print_error("Read error.\n");
        return;
    }

    print_title("\n--- ");
    print(directory[index].name);
    print(" ---\n");

    print((const char*)buffer);

    putchar_os('\n');

    print_title("--- END ---\n\n");
}

void fs_edit(const char* name, const char* text)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    fs_store_data(index, text, "File edited.\n");
}

void fs_delete(const char* name)
{
    int index = fs_find(name);

    if (index == -1)
    {
        print_error("File not found.\n");
        return;
    }

    if (directory[index].type == FS_TYPE_DIR)
    {
        print_error("Is a directory. Use 'rmdir' to remove it.\n");
        return;
    }

    directory[index].used = 0;

    directory[index].name[0] = '\0';
    directory[index].size = 0;

    fs_save_directory();

    if (!fs_quiet) print_success("File deleted.\n");
}

void fs_create(const char* name) {
    int name_len = 0;

    if (name == 0 || name[0] == '\0') {
        print_error("Filename is empty.\n");
        return;
    }

    /* File names are single tokens: spaces and tabs are not allowed. */
    while (name[name_len] != '\0') {
        if (name[name_len] == ' ' || name[name_len] == '\t' ||
            name[name_len] == '\n' || name[name_len] == '\r') {
            print_error("Filename cannot contain whitespace.\n");
            return;
        }

        if (name_len >= MAX_FILENAME - 1) {
            print_error("Filename is too long (maximum 31 characters).\n");
            return;
        }

        name_len++;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == current_dir && strcmp(directory[i].name, name) == 0) {
            print_error("File already exists.\n");
            return;
        }
    }

    for (int i = 1; i < MAX_FILES; i++) {
        if (!directory[i].used) {
            directory[i].used = 1;
            directory[i].type = FS_TYPE_FILE;
            directory[i].parent = current_dir;
            strcpy(directory[i].name, name);
            directory[i].size = 0;

            unsigned char empty[512];
            for (int k = 0; k < 512; k++) empty[k] = 0;
            for (int s = 0; s < FS_FILE_SECTORS; s++)
                fs_write_lba(
                    FS_DATA_LBA + i * FS_FILE_SECTORS + s,
                    empty);

            fs_save_directory();
            if (!fs_quiet) print_success("File created.\n");
            return;
        }
    }
    print_error("No free file slots.\n");
}

void fs_mkdir(const char* name) {
    if (name[0] == '\0') {
        print_error("Directory name is empty.\n");
        return;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == current_dir && strcmp(directory[i].name, name) == 0) {
            print_error("Directory already exists.\n");
            return;
        }
    }

    for (int i = 1; i < MAX_FILES; i++) {
        if (!directory[i].used) {
            directory[i].used = 1;
            directory[i].type = FS_TYPE_DIR;
            directory[i].parent = current_dir;
            strcpy(directory[i].name, name);
            directory[i].size = 0;

            fs_save_directory();
            if (!fs_quiet) print_success("Directory created.\n");
            return;
        }
    }
    print_error("No free file slots.\n");
}

void fs_ls(void) {
    print_title("\n=== DIRECTORY ===\n\n");
    int found = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].parent == current_dir && i != 0) {
            if (directory[i].type == FS_TYPE_DIR) {
                set_color(COLOR_LIGHT_BLUE);
                print("  [DIR]  ");
                print(directory[i].name);
                print("/\n");
                set_color(base_color);
            } else {
                print("  [FILE] ");
                print(directory[i].name);
                print(" (");
                print_int((int)directory[i].size);
                print(" bytes)\n");
            }
            found = 1;
        }
    }
    if (!found) print("  (empty)\n");
    putchar_os('\n');
}

void fs_cd(const char* name) {
    if (strcmp(name, "..") == 0) {
        if (current_dir != 0) current_dir = directory[current_dir].parent;
        return;
    }

    if (name[0] == '/') {
        current_dir = 0;
    }

    const char* search_name = name;
    if (name[0] == '/') search_name = name + 1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == current_dir &&           /* <<< NEW */
            strcmp(directory[i].name, search_name) == 0) {
            current_dir = i;
            return;
        }
    }
}

int fs_dir_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_FILES; i++)
        if (directory[i].used && directory[i].parent == current_dir && i != 0) n++;
    return n;
}

int fs_dir_entry(int index, const char** name, int* type, unsigned int* size)
{
    int n = 0;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].parent == current_dir && i != 0)
        {
            if (n == index)
            {
                if (name) *name = directory[i].name;
                if (type) *type = directory[i].type;
                if (size) *size = directory[i].size;
                return i;
            }
            n++;
        }
    }
    return -1;
}

static char fs_path_buf[256];
const char* fs_current_path(void)
{
    char stack[MAX_FILES][MAX_FILENAME];
    int depth = 0;
    int cur   = current_dir;
    while (cur != 0 && depth < MAX_FILES)
    {
        strcpy(stack[depth], directory[cur].name);
        depth++;
        cur = directory[cur].parent;
    }
    int pos = 0;
    fs_path_buf[pos++] = '/';
    for (int i = depth - 1; i >= 0; i--)
    {
        int len = 0; while (stack[i][len]) len++;
        if (pos + len + 2 >= 256) break;
        for (int j = 0; j < len; j++) fs_path_buf[pos++] = stack[i][j];
        if (i > 0) fs_path_buf[pos++] = '/';
    }
    fs_path_buf[pos] = '\0';
    return fs_path_buf;
}

static int resolve_drive_arg(const char* args)
{
    int n = ata_drive_count();

    if (args == 0 || args[0] == '\0') return fs_current_drive;

    char c = args[0];

    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {
        return fs_drive_index_by_letter(c);
    }

    if (c >= '0' && c <= '9')
    {
        int num = atoi_simple(args);

        if (num >= 1 && num <= n) return num - 1;
    }

    return -1;
}

static void cmd_drive(const char* args)
{
    if (ata_drive_count() == 0)
    {
        print_error("No ATA disks detected.\n");
        return;
    }

    if (args == 0 || args[0] == '\0')
    {
        ata_drive_t* d = ata_get_drive(fs_current_drive);
        unsigned long long total = (unsigned long long)d->sectors * 512ULL;

        print_title("\n=== ACTIVE DISK ===\n\n");
        print("  Disk:   ");
        putchar_os(d->letter);
        print(" (");
        print(disk_position_name(d));
        print(")\n");
        print("  Model:  ");
        print(d->model);
        putchar_os('\n');
        print("  Bytes:  ");
        print_uint64(total);
        print(" B  (");
        print_size_human(total);
        print(")\n\n");

        print("Usage: drive <letter|number>\n");
        print("  drive D  - switch to the disk with the letter D\n");
        print("  drive 2  - switch to the second detected disk\n");
        putchar_os('\n');
        return;
    }

    int idx = resolve_drive_arg(args);

    if (idx < 0)
    {
        print_error("Unknown disk. Type 'disks' to see the list.\n");
        return;
    }

    if (idx == fs_current_drive)
    {
        print_success("Already on that disk (");
        putchar_os(fs_get_drive_letter());
        print(").\n");
        return;
    }

    int res = fs_change_drive(idx);

    if (res == -2)
    {
        print_error("No " OS_NAME " filesystem on that disk (use 'install').\n");
        return;
    }

    if (res != 0)
    {
        print_error("Cannot switch to that disk.\n");
        return;
    }

    ata_drive_t* d = ata_get_drive(fs_current_drive);

    print_success("Switched to ");
    putchar_os(d->letter);
    print(": (");
    print(disk_position_name(d));
    print(")\n");
    print("  New files are now created on ");
    putchar_os(d->letter);
    print(":\n");
}

static void cmd_drive_status(void)
{
    cmd_drive("");
}

static void cmd_passwd(void)
{
    if (root_password_is_set() &&
        !authenticate_root_password())
    {
        return;
    }

    char password[64];
    char confirm[64];

    for (int attempt = 0; attempt < 3; attempt++)
    {
        print("New root password: ");

        int length = read_password(password, sizeof(password));

        print("Confirm new password: ");

        int confirm_length =
            read_password(confirm, sizeof(confirm));

        if (length != confirm_length ||
            strcmp(password, confirm) != 0)
        {
            wipe_secret(password, sizeof(password));
            wipe_secret(confirm, sizeof(confirm));

            print_error("Passwords do not match.\n");
            continue;
        }

        make_root_salt(cfg_root_salt);
        nw_password_hash(password, cfg_root_salt, cfg_root_hash);

        wipe_secret(password, sizeof(password));
        wipe_secret(confirm, sizeof(confirm));

        cfg_setup_complete = 1;

        settings_save();

        print_success("Root password updated.\n");
        return;
    }

    print_error("Password was not changed.\n");
}

static void cmd_disk_info(const char* args)
{
    if (ata_drive_count() == 0)
    {
        print_error("No ATA disks detected.\n");
        return;
    }

    int idx = resolve_drive_arg(args);

    if (idx < 0)
    {
        print_error("Unknown disk. Type 'disks' to see the list.\n");
        return;
    }

    ata_drive_t* d = ata_get_drive(idx);

    if (d == 0)
    {
        print_error("Unknown disk.\n");
        return;
    }

    unsigned long long total = (unsigned long long)d->sectors * 512ULL;

    print_title("\n=== DISK ");
    putchar_os(d->letter);
    print(" ===\n\n");

    print("  Position:   ");
    print(disk_position_name(d));
    putchar_os('\n');

    print("  Model:      ");
    print(d->model);
    putchar_os('\n');

    print("  Sectors:    ");
    print_uint64(d->sectors);
    putchar_os('\n');

    print("  Bytes:      ");
    print_uint64(total);
    print(" B\n");

    print("  Kilobytes:  ");
    print_size_unit(total, SIZE_KB, 2, "KB");
    putchar_os('\n');

    print("  Megabytes:  ");
    print_size_unit(total, SIZE_MB, 2, "MB");
    putchar_os('\n');

    print("  Gigabytes:  ");
    print_size_unit(total, SIZE_GB, 2, "GB");
    putchar_os('\n');

    if (fs_drive_has_fs(idx))
    {
        unsigned int files = 0;
        unsigned long long used = fs_drive_used_bytes(idx, &files);
        unsigned int entries = fs_drive_entry_count(idx);
        unsigned long long overhead =
            (unsigned long long)(1 + FS_DIR_SECTORS + MAX_FILES * FS_FILE_SECTORS) * 512ULL;
        unsigned long long free = (total > overhead) ? (total - overhead) : 0;

        print("  Filesystem: ");
        print(OS_NAME);
        putchar_os('\n');

        print("  Entries:    ");
        print_uint64(entries);
        print(" (");
        print_uint64(files);
        print(" files)\n");

        print("  File data:  ");
        print_uint64(used);
        print(" B  (");
        print_size_unit(used, SIZE_KB, 2, "KB");
        print(")\n");

        print("  Overhead:   ");
        print_uint64(overhead);
        print(" B  (");
        print_size_unit(overhead, SIZE_KB, 2, "KB");
        print(")\n");

        print("  Free space: ");
        print_uint64(free);
        print(" B  (");
        print_size_unit(free, SIZE_MB, 2, "MB");
        print(")\n");
    }
    else
    {
        print("  Filesystem: none (use 'install' to create one)\n");
    }

    putchar_os('\n');
}

static void fs_enter_user_home(void)
{
    char home[80];

    if (!fs_build_home_path(home, sizeof(home)))
    {
        current_dir = 0;
        return;
    }

    int dir = fs_resolve_dir_path(home);

    if (dir < 0)
        dir = fs_ensure_dir_path(home);

    current_dir = (dir >= 0) ? (uint16_t)dir : 0;
}

static void cmd_format(void)
{
    if (!cmd_sudo && !authenticate_root_password())
        return;

    print_error("This will erase the active NwOS filesystem.\n");
    print("Type format to continue: ");

    char answer[16];
    read_line(answer, sizeof(answer));

    if (strcmp(answer, "format") != 0)
    {
        print("Formatting cancelled.\n");
        return;
    }

    fs_format_disk(fs_current_drive);
}


void death_screen(void) {
    if (!authorize_sensitive_action(
            "Deleting everything inside the root directory",
            "/"))
    {
        return;
    }
    print_error("Deleting '/' directory..\n");
    sleep(1000);
    
    for (int i = 1; i < MAX_FILES; i++) {
        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].size = 0;
    }
    
    unsigned char zero[512];
    for (int k = 0; k < 512; k++) zero[k] = 0;
    for (int i = 0; i < MAX_FILES * FS_FILE_SECTORS; i++) {
        fs_write_lba(FS_DATA_LBA + i, zero);
    }
    
    directory[1].used = 1;
    directory[1].type = FS_TYPE_FILE;
    directory[1].parent = 0;
    strcpy(directory[1].name, "death.bsod");
    const char* msg = "Root filesystem destroyed.";
    directory[1].size = 45;
    
    unsigned char sector[512];
    for (int k = 0; k < 512; k++) sector[k] = 0;
    for (int k = 0; msg[k] != '\0' && k < 511; k++) sector[k] = msg[k];
    fs_write_lba(FS_DATA_LBA + 1 * FS_FILE_SECTORS, sector);
    
    fs_save_directory();
    
    KPANIC_FATAL(ERR_DEATH, "Root filesystem destroyed.");
}

void disk_death(void) {
    unknow_panic(DISK_DEATH);
}

static void cmd_pwd(void)
{
    print(fs_current_path());
    putchar_os('\n');
}

void run_calc(void) {
    run_nwo_from_dir("/sys/apps", "calculator.nwo");
}

static const Command commands[] =
{
    CMD    ("help",         cmd_help,        "help   - show commands"),
    CMD_ARG("echo",         cmd_echo,        "echo <text> - print text"),
    CMD    ("clear",        clear,           "clear  - clear screen"),
    CMD    ("about",        cmd_about,       "about  - OS information"),
    CMD    ("nwfetch",      nwfetch,         "nwfetch - system info"),
    CMD    ("games",        games_menu,      "games  - list games"),
    CMD    ("calc",         run_calc,        "calc   - Calculator"),
    CMD    ("color",        t_colorsetter,   "color  - change color"),
    CMD    ("format",       cmd_format,       "format  - erase all files"),
    CMD    ("panic safe",   cmd_panic_safe,  "panic safe|fatal - test kernel panic"),
    CMD    ("panic fatal",  cmd_panic_fatal, 0),
    CMD    ("passwd",       cmd_passwd,       "passwd - set or change the root password"),
    CMD_ARG("touch",       cmd_touch,       "touch <file1> [file2 ...] - create files"),
    CMD_ARG("bash",        cmd_bash,        "bash <file.sh> - run NwSH script"),
    CMD_ARG("bat",         cmd_bat,         "bat <file.bat> - run NwBAT script"),
    CMD_ARG("nwc",         cmd_nwc,        "nwc <file.nw> -o <file.nwo> - compile NwC source"),
    CMD_ARG("nw",          package_handler, "nw list|install|remove|update|run - local package manager"),
    CMD_ARG("cat",         fs_read,         "cat <file> - read file"),
    CMD    ("pwd",         cmd_pwd,           "pwd - print the current directory"),
    CMD_ARG("write",        cmd_write,       "write <file> <text> - write text to file"),
    CMD_ARG("edit",         redactor,        "edit <file> - edit a file"),
    //CMD_ARG("delete",       fs_delete,       "delete <file> - delete file"),
    CMD    ("rm -rf /",     death_screen,    0),
    CMD    ("rm /",         death_screen,    0),
    CMD    ("rm disk/",     disk_death,      0),
    CMD_ARG("rm",           fs_rm,           "rm [-r] <file1> [file2 ...] - remove files (-r = directories)"),
    CMD_ARG("rmdir",        fs_rmdir,        "rmdir <dir> - remove directory with its content"),
    CMD    ("reboot",       reboot,          "reboot - restart " OS_NAME),
    CMD_ARG("run",          nwo_run,         "run <file.nwo> - run NWO application"),
    CMD    ("apps",         apps_list,       "apps - List of apps"),
    CMD    ("generate num", generate_num,    "generate num - generate num"),
    CMD_ARG("hexview",      app_hexview,     "hexview <file> - hex byte map"),
    CMD_ARG("nwocheck",     app_nwo_info,    "nwocheck <file> - check OPcodes"),
    CMD    ("random word",  say,             "random word - generate word"),
    CMD    ("chat",         chat,            "chat - chat with computer"),
    CMD    ("change invite",set_p_shell,     "change invite - Change invite ('" OS_NAME ">')"),
    CMD    ("change color", change_p_color,  "change color - Change prompt color"),
    CMD    ("settings",     cmd_settings,    "settings - show configuration"),
    CMD    ("mem",          cmd_mem,         "mem    - memory info"),
    CMD    ("cpu",          cmd_cpu,         0                        ),
    CMD    ("uptime",       cmd_uptime,      0                        ),
    CMD    ("time",         cmd_time_cmd,    "time   - show time/date"),
    CMD    ("zones",        cmd_zones,       "zones  - list all time zones"),
    CMD_ARG("timezone",    cmd_timezone,    "timezone <num> - set timezone"),
    CMD_ARG("date",        cmd_date_set,    "date   - set custom date"),
    CMD    ("more",         cmd_more_help,   "more   - additional commands"),
    CMD    ("disks",        cmd_disks,       "disks  - list all ATA disks"),
    CMD    ("drive",        cmd_drive_status, 0),
    CMD_ARG("drive",        cmd_drive,       "drive <C|D|n> - switch the active disk"),
    CMD_ARG("chdrive",      cmd_drive,       0),
    CMD_ARG("disk",         cmd_disk_info,   "disk <C|D|n> - detailed size of one disk"),
    CMD    ("install",      cmd_install,      "install - run the installation wizard"),
    CMD    ("ls",           fs_ls,           "ls     - list files in current dir"),
    CMD_ARG("mkdir",        fs_mkdir,        "mkdir <dir> - create directory"),
    CMD_ARG("cd",           cmd_cd_path,               "cd <path> - change directory; supports ~, .. and nested paths"),
    CMD    ("gui",          run_gui,         "gui    - launch graphical shell"),
    CMD    ("word",         word_new,        "word   - generate random word"),

    /* network */
    CMD_ARG("ping",         net_ping,        "ping <host or IP> - e.g. ping www.google.com"),
    CMD    ("net",          net_info,        "net    - network status (MAC, IP, link)"),
    CMD    ("lspci",        net_lspci,       "lspci  - list PCI devices"),

    /* hidden (not shown by 'help') */
    CMD    ("datec",        dateCr,          0),
    CMD    ("games old",    games,           0),
    CMD    ("testH",        testH,           0),
    CMD    ("guessh",       game_guessh,     0),
    CMD    ("game coin",    game_coin,       0),
    CMD    ("game dice",    game_dice,       0),
    CMD    ("game highlow", game_higher_lower, 0),
    CMD    ("game math",    game_math,       0),
    CMD    ("word",         game_word,       0),
    CMD    ("guess",        game_guess,      0),
    CMD    ("rps",          game_rps,        0),
    CMD    ("hangman",      game_hangman,    0),
    CMD    ("tictactoe",    game_tictactoe, 0),
};

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(void)
{
    set_color(COLOR_GREEN);
    print("Commands:\n");
    int lines_on_screen = 1; 

    for (unsigned int i = 0; i < COMMAND_COUNT; i++)
    {
        if (commands[i].help)
        {
            if (lines_on_screen >= 50)
            {
                set_color(COLOR_YELLOW);
                print("--- MORE --- (ENTER: line, SPACE: page, ESC: quit)\n");
                set_color(COLOR_GREEN);

                int advance = 0;
                while (!advance) 
                {
                    int k = keyboard_getkey();
                    
                    if (k == 27 || k == KEY_ESC) 
                    {
                        set_color(base_color);
                        return;
                    }
                    else if (k == KEY_ENTER) 
                    {
                        lines_on_screen = 22;
                        advance = 1;
                    }
                    else if (k == ' ') 
                    {
                        lines_on_screen = 0;
                        advance = 1;
                    }
                }
                
                set_color(COLOR_DARK_GRAY);
                print("--- continued ---\n");
                set_color(COLOR_GREEN);
                lines_on_screen++;
            }

            print("  ");
            print(commands[i].help);
            putchar_os('\n');
            lines_on_screen++;
        }
    }
    set_color(base_color);
    print("\nType 'more' for additional commands.\n");
}

static void cmd_more_help(void)
{
    set_color(COLOR_GREEN);
    print("More commands:\n");
    print("  mem      - show memory info\n");
    print("  cpu      - show CPU info\n");
    print("  uptime   - show uptime\n");
    print("  time     - show current time/date\n");
    print("  zones    - list all time zones\n");
    print("  timezone <num> - set timezone\n");
    print("  date <DD> <MM> <YYYY> - set custom date\n");
    print("  date reset - reset date\n");
    print("  nwfetch  - system info\n");
    print("  reboot   - reboot system\n");
    print("  clear    - clear screen\n");
    print("  disks    - list all ATA disks\n");
    print("  drive <C|D|n> - switch the active disk\n");
    print("  disk <C|D|n>  - detailed size of one disk\n");
    putchar_os('\n');
    set_color(base_color);
}

static void cmd_about(void)
{
    set_color(COLOR_WHITE);
    print("====" OS_NAME " " OS_VERSION_STR "====\n");
    print("Name: " OS_NAME "\n");
    print("Version: v" OS_VERSION_STR "\n");
    print("Arch: x86-64\n");
    print("Display: VGA text mode\n");
    print("PS/2 keyboard\n");
    print("Network: e1000 (ping)\n");
    print("================\n");
    set_color(base_color);
}

static void cmd_echo(const char* args)
{
    print(args);
    putchar_os('\n');
}

static void cmd_touch(const char* args)
{
    const char* p = args ? args : "";
    int files = 0;

    /* A space, tab or comma separates file names. */
    while (*p != '\0')
    {
        char name[MAX_FILENAME];
        int n = 0;
        int too_long = 0;

        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == '\0') break;

        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',')
        {
            if (n < MAX_FILENAME - 1)
                name[n++] = *p;
            else
                too_long = 1;
            p++;
        }
        name[n] = '\0';

        if (n == 0)
            continue;

        files++;

        if (too_long)
        {
            print_error("Filename is too long (maximum 31 characters).\n");
            continue;
        }

        fs_create(name);
    }

    if (files == 0)
        print_error("Usage: touch <file1> [file2 ...]\n");
}

static void cmd_write(const char* args)
{
    char name[128];
    int i = 0;

    while (args[i] != ' ' && args[i] != '\0' && i < 127)
    {
        name[i] = args[i];
        i++;
    }

    name[i] = '\0';

    if (args[i] != ' ')
    {
        print_error("Usage: write <file> <text>\n");
        return;
    }

    fs_write(name, args + i + 1);
}

#define SCRIPT_MAX_VARS       32
#define SCRIPT_VAR_NAME       32
#define SCRIPT_VAR_VALUE      128
#define SCRIPT_LINE_MAX       256
#define SCRIPT_MAX_STEPS      10000
#define SCRIPT_MODE_BASH      0
#define SCRIPT_MODE_BAT       1

static struct
{
    char name[SCRIPT_VAR_NAME];
    char value[SCRIPT_VAR_VALUE];
} script_vars[SCRIPT_MAX_VARS];

static int script_var_count = 0;
static int script_depth = 0;
static int script_last_status = 0;

static int script_command_failed = 0;

static int script_is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int script_is_name_start(char c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_';
}

static int script_is_name_char(char c)
{
    return script_is_name_start(c) || (c >= '0' && c <= '9');
}

static int script_has_prefix(const char* text, const char* prefix)
{
    while (*prefix)
    {
        char a = *text++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static void script_trim(char* text)
{
    int start = 0;
    int end = 0;
    int out = 0;

    if (!text) return;
    while (script_is_space(text[start])) start++;
    while (text[start + end] != '\0') end++;
    while (end > 0 && script_is_space(text[start + end - 1])) end--;

    while (out < end)
    {
        text[out] = text[start + out];
        out++;
    }
    text[out] = '\0';
}

static void script_clear_vars(void)
{
    for (int i = 0; i < SCRIPT_MAX_VARS; i++)
    {
        script_vars[i].name[0] = '\0';
        script_vars[i].value[0] = '\0';
    }
    script_var_count = 0;
}

static const char* script_get_var(const char* name)
{
    for (int i = 0; i < script_var_count; i++)
    {
        if (strcmp(script_vars[i].name, name) == 0)
            return script_vars[i].value;
    }
    return "";
}

static int script_set_var(const char* name, const char* value)
{
    int index = -1;
    int name_len = 0;
    int value_len = 0;

    if (!name || !value || !script_is_name_start(name[0]))
        return 0;

    while (name[name_len] != '\0')
    {
        if (!script_is_name_char(name[name_len]) ||
            name_len >= SCRIPT_VAR_NAME - 1)
            return 0;
        name_len++;
    }

    while (value[value_len] != '\0')
    {
        if (value_len >= SCRIPT_VAR_VALUE - 1)
            return 0;
        value_len++;
    }

    for (int i = 0; i < script_var_count; i++)
    {
        if (strcmp(script_vars[i].name, name) == 0)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        if (script_var_count >= SCRIPT_MAX_VARS)
            return 0;
        index = script_var_count++;
    }

    strcpy(script_vars[index].name, name);
    strcpy(script_vars[index].value, value);
    return 1;
}

static int script_unset_var(const char* name)
{
    for (int i = 0; i < script_var_count; i++)
    {
        if (strcmp(script_vars[i].name, name) == 0)
        {
            for (int j = i; j < script_var_count - 1; j++)
                script_vars[j] = script_vars[j + 1];
            script_var_count--;
            script_vars[script_var_count].name[0] = '\0';
            script_vars[script_var_count].value[0] = '\0';
            return 1;
        }
    }
    return 1;
}

static void script_status_text(char output[4])
{
    output[0] = script_last_status ? '1' : '0';
    output[1] = '\0';
}

static int script_expand_vars(const char* input, char* output,
                              unsigned int output_size, int mode)
{
    unsigned int out = 0;
    unsigned int i = 0;
    int single_quote = 0;
    int double_quote = 0;
    int escaped = 0;

    if (!input || !output || output_size == 0)
        return 0;

    while (input[i] != '\0')
    {
        char var_name[SCRIPT_VAR_NAME];
        unsigned int name_len = 0;
        const char* value = 0;
        int is_variable = 0;
        int can_expand = 1;

        if (mode == SCRIPT_MODE_BASH && escaped)
        {
            escaped = 0;
            can_expand = 0;
        }
        else if (mode == SCRIPT_MODE_BASH && input[i] == '\\' && !single_quote)
        {
            escaped = 1;
        }
        else if (mode == SCRIPT_MODE_BASH && input[i] == '\'' && !double_quote)
        {
            single_quote = !single_quote;
        }
        else if (mode == SCRIPT_MODE_BASH && input[i] == '"' && !single_quote)
        {
            double_quote = !double_quote;
        }

        if (mode == SCRIPT_MODE_BASH && can_expand && !single_quote && input[i] == '$' && input[i + 1] == '?')
        {
            char status_text[4];
            unsigned int k = 0;
            script_status_text(status_text);
            while (status_text[k] != '\0')
            {
                if (out + 1 >= output_size) return 0;
                output[out++] = status_text[k++];
            }
            i += 2;
            continue;
        }
        else if (mode == SCRIPT_MODE_BASH && can_expand && !single_quote && input[i] == '$')
        {
            unsigned int j = i + 1;
            int braced = input[j] == '{';
            if (braced) j++;

            if (script_is_name_start(input[j]))
            {
                while (script_is_name_char(input[j]) &&
                       name_len < SCRIPT_VAR_NAME - 1)
                    var_name[name_len++] = input[j++];

                if (braced)
                {
                    if (input[j] == '}')
                    {
                        j++;
                        is_variable = 1;
                    }
                }
                else
                {
                    is_variable = 1;
                }

                if (is_variable)
                {
                    var_name[name_len] = '\0';
                    value = script_get_var(var_name);
                    i = j;
                }
            }
        }
        else if (mode == SCRIPT_MODE_BAT && input[i] == '%' &&
                 input[i + 1] != '\0')
        {
            unsigned int j = i + 1;
            while (input[j] != '\0' && input[j] != '%' &&
                   name_len < SCRIPT_VAR_NAME - 1)
                var_name[name_len++] = input[j++];

            if (input[j] == '%' && name_len > 0)
            {
                var_name[name_len] = '\0';
                value = script_get_var(var_name);
                i = j + 1;
                is_variable = 1;
            }
        }

        if (is_variable)
        {
            unsigned int k = 0;
            while (value[k] != '\0')
            {
                if (out + 1 >= output_size) return 0;
                output[out++] = value[k++];
            }
            continue;
        }

        if (out + 1 >= output_size) return 0;
        output[out++] = input[i++];
    }

    output[out] = '\0';
    return 1;
}

static void script_strip_bash_comment(char* line)
{
    int single_quote = 0;
    int double_quote = 0;
    int escaped = 0;

    for (int i = 0; line[i] != '\0'; i++)
    {
        char c = line[i];
        if (escaped) { escaped = 0; continue; }
        if (c == '\\' && !single_quote) { escaped = 1; continue; }
        if (c == '\'' && !double_quote) { single_quote = !single_quote; continue; }
        if (c == '"' && !single_quote) { double_quote = !double_quote; continue; }
        if (c == '#' && !single_quote && !double_quote)
        {
            line[i] = '\0';
            return;
        }
    }
}

static int script_parse_assignment(char* line, int mode)
{
    char name[SCRIPT_VAR_NAME];
    char raw_value[SCRIPT_VAR_VALUE];
    char expanded[SCRIPT_VAR_VALUE];
    int name_len = 0;
    int value_len = 0;
    int i = 0;
    int is_bat_set = 0;

    if (mode == SCRIPT_MODE_BASH && script_has_prefix(line, "export "))
        i = 7;
    else if (mode == SCRIPT_MODE_BAT && script_has_prefix(line, "set "))
    {
        i = 4;
        is_bat_set = 1;
    }

    if (is_bat_set && line[i] == '"') i++;

    if (!script_is_name_start(line[i]))
        return is_bat_set ? -1 : 0;

    while (script_is_name_char(line[i]) && name_len < SCRIPT_VAR_NAME - 1)
        name[name_len++] = line[i++];
    name[name_len] = '\0';

    if (line[i] != '=')
        return is_bat_set ? -1 : 0;
    i++;

    while (line[i] != '\0' && value_len < SCRIPT_VAR_VALUE - 1)
        raw_value[value_len++] = line[i++];
    if (line[i] != '\0')
    {
        print_error("Script variable value is too long.\n");
        return -1;
    }
    raw_value[value_len] = '\0';

    if (is_bat_set && value_len > 0 && raw_value[value_len - 1] == '"')
        raw_value[--value_len] = '\0';

    if (value_len >= 2 &&
        ((raw_value[0] == '"' && raw_value[value_len - 1] == '"') ||
         (raw_value[0] == '\'' && raw_value[value_len - 1] == '\'')))
    {
        for (int j = 0; j < value_len - 2; j++)
            raw_value[j] = raw_value[j + 1];
        raw_value[value_len - 2] = '\0';
    }

    if (!script_expand_vars(raw_value, expanded, sizeof(expanded), mode))
    {
        print_error("Expanded script variable is too long.\n");
        return -1;
    }

    if (!script_set_var(name, expanded))
    {
        print_error("Invalid variable or variable table is full.\n");
        return -1;
    }

    return 1;
}

static void script_echo(char* text)
{
    script_trim(text);
    if (text[0] == '\0')
    {
        putchar_os('\n');
        return;
    }

    int len = 0;
    while (text[len] != '\0') len++;
    if (len >= 2 &&
        ((text[0] == '"' && text[len - 1] == '"') ||
         (text[0] == '\'' && text[len - 1] == '\'')))
    {
        text[len - 1] = '\0';
        text++;
    }

    print(text);
    putchar_os('\n');
}

static int script_run(const char* filename, int mode)
{
    static char file_buffer[MAX_FILE_SIZE + 1];
    char line[SCRIPT_LINE_MAX];
    char expanded[SCRIPT_LINE_MAX];
    unsigned int pos = 0;
    int line_number = 0;
    int steps = 0;
    int index;
    int result = 1;
    int errexit = 0;
    int explicit_exit = 0;

    if (!filename || filename[0] == '\0')
    {
        print_error(mode == SCRIPT_MODE_BASH ?
            "Usage: bash <file.sh>\n" : "Usage: bat <file.bat>\n");
        return 0;
    }

    if (script_depth >= 1)
    {
        print_error("Nested scripts are not supported yet.\n");
        return 0;
    }

    index = fs_find(filename);
    if (index < 0 || directory[index].type != FS_TYPE_FILE)
    {
        print_error("Script file not found.\n");
        return 0;
    }

    if (!fs_read_text(filename, file_buffer, sizeof(file_buffer)))
    {
        print_error("Could not read script file.\n");
        return 0;
    }

    script_clear_vars();
    script_last_status = 0;
    script_depth++;

    while (file_buffer[pos] != '\0')
    {
        int n = 0;
        int too_long = 0;

        line_number++;
        while (file_buffer[pos] != '\0' &&
               file_buffer[pos] != '\n')
        {
            char c = file_buffer[pos++];
            if (c == '\r') continue;
            if (n < SCRIPT_LINE_MAX - 1)
                line[n++] = c;
            else
                too_long = 1;
        }
        if (file_buffer[pos] == '\n') pos++;
        line[n] = '\0';

        steps++;
        if (steps > SCRIPT_MAX_STEPS)
        {
            print_error("Script stopped: instruction limit exceeded.\n");
            result = 0;
            break;
        }
        if (too_long)
        {
            print_error("Script line is too long at line ");
            print_int(line_number);
            putchar_os('\n');
            result = 0;
            break;
        }

        script_trim(line);
        if (line[0] == '\0') continue;

        if (mode == SCRIPT_MODE_BASH)
        {
            if (line[0] == '#') continue;
            script_strip_bash_comment(line);
            script_trim(line);
            if (line[0] == '\0') continue;

            if (strcmp(line, "set -e") == 0)
            {
                errexit = 1;
                continue;
            }
            if (strcmp(line, "set +e") == 0)
            {
                errexit = 0;
                continue;
            }
        }
        else
        {
            if (line[0] == '@')
            {
                for (int i = 0; line[i] != '\0'; i++) line[i] = line[i + 1];
                script_trim(line);
            }
            if (line[0] == '\0' || line[0] == ':' ||
                script_has_prefix(line, "rem ") ||
                script_has_prefix(line, "rem\t") ||
                strcmp(line, "rem") == 0)
                continue;
        }

        if (mode == SCRIPT_MODE_BAT &&
            (strcmp(line, "echo off") == 0 || strcmp(line, "echo on") == 0))
            continue;

        if (mode == SCRIPT_MODE_BAT && script_has_prefix(line, "set "))
        {
            int parsed = script_parse_assignment(line, mode);
            if (parsed < 0) { result = 0; break; }
            script_last_status = 0;
            continue;
        }

        if (mode == SCRIPT_MODE_BASH)
        {
            if (script_has_prefix(line, "unset "))
            {
                char* name = line + 6;
                script_trim(name);
                if (name[0] == '\0')
                {
                    print_error("Usage: unset NAME\n");
                    result = 0;
                    break;
                }
                script_unset_var(name);
                script_last_status = 0;
                continue;
            }

            if (script_has_prefix(line, "export "))
            {
                char* assignment = line + 7;
                int has_equal = 0;
                for (int i = 0; assignment[i] != '\0'; i++)
                    if (assignment[i] == '=') { has_equal = 1; break; }
                if (!has_equal)
                {
                    script_last_status = 0;
                    continue;
                }
            }

            int parsed = script_parse_assignment(line, mode);
            if (parsed < 0) { result = 0; break; }
            if (parsed > 0)
            {
                script_last_status = 0;
                continue;
            }
        }

        if (strcmp(line, "true") == 0)
        {
            script_last_status = 0;
            continue;
        }
        if (strcmp(line, "false") == 0)
        {
            script_last_status = 1;
            if (errexit) { result = 0; break; }
            continue;
        }

        if (strcmp(line, "exit") == 0 ||
            strcmp(line, "exit 0") == 0 ||
            strcmp(line, "exit /b") == 0 ||
            strcmp(line, "exit /b 0") == 0)
        {
            result = script_last_status == 0;
            explicit_exit = 1;
            break;
        }

        if (script_has_prefix(line, "exit "))
        {
            const char* code = line + 5;
            while (script_is_space(*code)) code++;
            result = (*code == '0' && code[1] == '\0');
            explicit_exit = 1;
            break;
        }

        if (script_has_prefix(line, "echo") &&
            (line[4] == '\0' || script_is_space(line[4])))
        {
            char* text = line + 4;
            while (script_is_space(*text)) text++;
            if (!script_expand_vars(text, expanded, sizeof(expanded), mode))
            {
                print_error("Expanded echo line is too long.\n");
                result = 0;
                break;
            }
            script_echo(expanded);
            script_last_status = 0;
            continue;
        }

        if (!script_expand_vars(line, expanded, sizeof(expanded), mode))
        {
            print_error("Expanded command is too long at line ");
            print_int(line_number);
            putchar_os('\n');
            result = 0;
            break;
        }

        script_last_status = shell_execute(expanded) ? 0 : 1;
        if (script_last_status != 0 && errexit)
        {
            result = 0;
            break;
        }
    }

    if (!explicit_exit && result)
        result = (script_last_status == 0);

    script_depth--;
    script_clear_vars();
    return result;
}

static void cmd_bash(const char* args)
{
    char filename[MAX_FILENAME];
    int i = 0;
    int n = 0;

    if (!args) args = "";
    while (script_is_space(args[i])) i++;
    while (args[i] != '\0' && !script_is_space(args[i]) && n < MAX_FILENAME - 1)
        filename[n++] = args[i++];
    filename[n] = '\0';
    while (script_is_space(args[i])) i++;

    if (n == 0 || args[i] != '\0' ||
        n < 3 || strcmp(filename + n - 3, ".sh") != 0)
    {
        print_error("Usage: bash <file.sh>\n");
        return;
    }

    if (script_run(filename, SCRIPT_MODE_BASH))
        print_success("Script completed.\n");
    else
        print_error("Script failed.\n");
}

static void cmd_bat(const char* args)
{
    char filename[MAX_FILENAME];
    int i = 0;
    int n = 0;

    if (!args) args = "";
    while (script_is_space(args[i])) i++;
    while (args[i] != '\0' && !script_is_space(args[i]) && n < MAX_FILENAME - 1)
        filename[n++] = args[i++];
    filename[n] = '\0';
    while (script_is_space(args[i])) i++;

    if (n == 0 || args[i] != '\0' ||
        n < 4 || strcmp(filename + n - 4, ".bat") != 0)
    {
        print_error("Usage: bat <file.bat>\n");
        return;
    }

    if (script_run(filename, SCRIPT_MODE_BAT))
        print_success("Batch file completed.\n");
    else
        print_error("Batch file failed.\n");
}

static int nwc_next_token(const char* args, int* position,
                          char* output, int output_capacity)
{
    int i = *position;
    int n = 0;
    char quote = 0;

    while (args[i] == ' ' || args[i] == '\t') i++;
    if (args[i] == '\0')
    {
        *position = i;
        return 0;
    }

    if (args[i] == '\'' || args[i] == '"')
        quote = args[i++];

    while (args[i] != '\0')
    {
        if (quote)
        {
            if (args[i] == quote)
            {
                i++;
                quote = 0;
                break;
            }
        }
        else if (args[i] == ' ' || args[i] == '\t')
        {
            break;
        }

        if (n >= output_capacity - 1)
            return -1;
        output[n++] = args[i++];
    }

    if (quote)
        return -1;

    output[n] = '\0';
    while (args[i] == ' ' || args[i] == '\t') i++;
    *position = i;
    return n > 0 ? 1 : -1;
}

static int nwc_has_suffix(const char* name, const char* suffix)
{
    int name_len = 0;
    int suffix_len = 0;
    while (name[name_len] != '\0') name_len++;
    while (suffix[suffix_len] != '\0') suffix_len++;
    if (name_len < suffix_len) return 0;
    return strcmp(name + name_len - suffix_len, suffix) == 0;
}

static void cmd_nwc(const char* args)
{
    char input_name[MAX_FILENAME];
    char option[8];
    char output_name[MAX_FILENAME];
    int position = 0;
    int input_index;
    int token_result;
    unsigned int source_size;
    unsigned int nwo_size = 0;

    script_command_failed = 1;

    static char source_buffer[NWC_MAX_SOURCE_SIZE + 1];
    static unsigned char nwo_buffer[FS_FILE_SECTORS * 512u];

    if (!args) args = "";

    token_result = nwc_next_token(args, &position,
                                  input_name, sizeof(input_name));
    if (token_result != 1 ||
        nwc_next_token(args, &position, option, sizeof(option)) != 1 ||
        nwc_next_token(args, &position, output_name, sizeof(output_name)) != 1 ||
        args[position] != '\0' || strcmp(option, "-o") != 0)
    {
        print_error("Usage: nwc <file.nw> -o <file.nwo>\n");
        return;
    }

    if (!nwc_has_suffix(input_name, ".nw") ||
        !nwc_has_suffix(output_name, ".nwo"))
    {
        print_error("NwC expects an input .nw file and an output .nwo file.\n");
        return;
    }

    if (strcmp(input_name, output_name) == 0)
    {
        print_error("Input and output filenames must be different.\n");
        return;
    }

    input_index = fs_find(input_name);
    if (input_index < 0 || directory[input_index].type != FS_TYPE_FILE)
    {
        print_error("NwC source file not found.\n");
        return;
    }

    source_size = directory[input_index].size;
    if (source_size > NWC_MAX_SOURCE_SIZE)
    {
        print_error("NwC source exceeds the 16 KiB compiler limit.\n");
        return;
    }

    if (!fs_read_text(input_name, source_buffer, sizeof(source_buffer)))
    {
        print_error("Could not read NwC source file.\n");
        return;
    }

    print("NwC: compiling ");
    print(input_name);
    print("...\n");

    if (!nwc_compile_source(source_buffer, source_size,
                            nwo_buffer, sizeof(nwo_buffer), &nwo_size))
    {
        print_error("NwC compilation failed.\n");
        return;
    }

    if (nwo_size == 0 || nwo_size > sizeof(nwo_buffer))
    {
        print_error("NwC returned an invalid output size.\n");
        return;
    }

    if (!fs_write_binary(output_name, nwo_buffer, nwo_size))
    {
        print_error("Could not save NWO output file. Check filename and disk space.\n");
        return;
    }

    print_success("NwC: created ");
    print(output_name);
    print(" (");
    print_int((int)nwo_size);
    print(" bytes).\n");
    script_command_failed = 0;
}

static void cmd_panic_safe(void)
{
    KPANIC_SAFE(ERR_NONE, "Manual test panic (safe)");
}

static void cmd_panic_fatal(void)
{
    KPANIC_FATAL(ERR_BAD_STATE, "Manual test panic (fatal)");
}

static int command_name_length(const char* s)
{
    int n = 0;

    while (s[n] != '\0')
        n++;

    return n;
}

static void trim_whitespace(char* s)
{
    if (!s)
        return;

    while (*s == ' ' || *s == '\t')
        s++;

    char* p = s;
    while (*p)
        p++;
    while (p > s && (p[-1] == ' ' || p[-1] == '\t'))
        p--;
    *p = '\0';

    /* move back to start */
    if (p == s)
        return;
    char* d = s;
    char* src = s;
    while (*src == ' ' || *src == '\t')
        src++;
    while (*src)
    {
        *d++ = *src++;
    }
    *d = '\0';
}

static int execute_single_command(char* command)
{
    if (command[0] == '\0')
        return 1;

    if (strcmp(command, "sudo") == 0)
    {
        print_error("Usage: sudo <command>\n");
        return 0;
    }

    if (strncmp(command, "sudo ", 5) == 0)
    {
        if (cmd_sudo)
        {
            print_error("Nested sudo is not supported.\n");
            return 0;
        }

        char nested[128];
        const char* src = command + 5;

        while (*src == ' ' || *src == '\t')
            src++;

        int length = 0;

        while (*src && length < (int)sizeof(nested) - 1)
            nested[length++] = *src++;

        while (length > 0 &&
            (nested[length - 1] == ' ' ||
                nested[length - 1] == '\t'))
        {
            length--;
        }

        nested[length] = '\0';

        if (length == 0)
        {
            print_error("Usage: sudo <command>\n");
            return 0;
        }

        if (!authenticate_root_password())
            return 0;

        int previous_sudo = cmd_sudo;

        cmd_sudo = 1;

        int result = execute_single_command(nested);

        cmd_sudo = previous_sudo;

        return result;
    }

    for (unsigned int i = 0; i < COMMAND_COUNT; i++)
    {
        const Command* c = &commands[i];

        if (c->run != 0)
        {
            /* exact-word command */
            if (strcmp(command, c->name) == 0)
            {
                script_command_failed = 0;
                c->run();
                return script_command_failed ? 0 : 1;
            }

            continue;
        }

        /* command that takes text: "<name> <args>" */
        int n = command_name_length(c->name);

        if (strncmp(command, c->name, n) != 0)
            continue;

        if (command[n] == ' ' || command[n] == '\t')
        {
            const char* args = command + n;
            while (*args == ' ' || *args == '\t') args++;
            script_command_failed = 0;
            c->run_arg(args);
            return script_command_failed ? 0 : 1;
        }

        if (command[n] == '\0')
        {
            print_error("Usage: ");
            print_error(c->help ? c->help : c->name);
            putchar_os('\n');
            return 0;
        }
    }

    if (package_run_command(command))
        return 1;

    print_error("Unknown command. Type 'help'.\n");
    return 0;
}

static int shell_execute(const char* command)
{
    char segment[256];
    int length = 0;
    int single_quote = 0;
    int double_quote = 0;
    int escaped = 0;
    int run_next = 1;
    int last_status = 1;

    if (command == 0 || command[0] == '\0')
        return 1;

    for (int i = 0; ; i++)
    {
        char c = command[i];
        int at_end = (c == '\0');
        int separator = 0;
        int is_and = 0;

        if (!at_end && escaped)
        {
            escaped = 0;
        }
        else if (!at_end && c == '\\' && !single_quote)
        {
            escaped = 1;
        }
        else if (!at_end && c == '\'' && !double_quote)
        {
            single_quote = !single_quote;
        }
        else if (!at_end && c == '"' && !single_quote)
        {
            double_quote = !double_quote;
        }
        else if (!single_quote && !double_quote)
        {
            if (at_end || c == ';')
                separator = 1;
            else if (c == '&' && command[i + 1] == '&')
            {
                separator = 1;
                is_and = 1;
            }
        }

        if (separator)
        {
            segment[length] = '\0';
            script_trim(segment);

            if (segment[0] != '\0' && run_next)
            {
                last_status = execute_single_command(segment);
            }

            if (at_end)
                break;

            if (is_and)
            {
                if (!last_status)
                    run_next = 0;
                i++; /* Skip the second '&'. */
            }
            else
            {
                run_next = 1;
            }

            length = 0;
            continue;
        }

        if (!at_end)
        {
            if (length >= (int)sizeof(segment) - 1)
            {
                print_error("Command is too long (maximum 255 characters).\n");
                return 0;
            }
            segment[length++] = c;
        }
        else
        {
            if (single_quote || double_quote)
            {
                print_error("Unclosed quote in command.\n");
                return 0;
            }
            break;
        }
    }

    return last_status;
}

void set_p_shell(void)
{
    int choose_p = 0;
    print("1.Default (" OS_NAME "> )\n2.D disk (D:/>)\n3.C disk (C:/>)\n4.Custom\n5.Current disk (\n6.Unix prompt");
    putchar_os(fs_get_drive_letter());
    print(":/>)\n");
    print("|\n");
    print("----> ");
    choose_p = read_int();
    if (choose_p == 1) {
        print_success("Set to 'Default'\n");
        shell_p = 0;
        cfg_strcpy(invite_user, OS_NAME "> ", 28);
    } else if (choose_p == 2) {
        print_success("Set to 'D disk'\n");
        shell_p = 1;
        cfg_strcpy(invite_user, "D:/> ", 28);
    } else if (choose_p == 3) {
        print_success("Set to 'C disk'\n");
        shell_p = 2;
        cfg_strcpy(invite_user, "C:/> ", 28);
    } else if (choose_p == 4) {
        print("Invite: ");
        read_line(invite_user, 28);
        shell_p = 9;
        print_success("Set to custom invite\n");
    } else if (choose_p == 5) {
        print_success("Set to 'Current disk'\n");
        shell_p = 3;
        cfg_strcpy(invite_user, "drive", 28);
    } else if (choose_p == 6) {
        shell_p = 4;
        print_success("Set to 'Unix'\n");
    } else {
        print_error("Invalid choose!\n");
        return;
    }

    settings_save();
}

static const char* const color_names[16] = {
    "black", "blue", "green", "cyan",
    "red", "magenta", "brown", "light_gray",
    "dark_gray", "light_blue", "light_green", "light_cyan",
    "light_red", "light_magenta", "yellow", "white"
};

static unsigned char color_from_name(const char* name)
{
    for (int i = 0; i < 16; i++)
    {
        if (strcmp(name, color_names[i]) == 0)
            return (unsigned char)i;
    }

    return COLOR_LIGHT_GRAY;
}

static int cfg_find_key(const char* buffer, const char* key, char* out, int out_max)
{
    int klen = 0;
    while (key[klen] != '\0') klen++;

    int i = 0;
    while (buffer[i] != '\0')
    {
        int start = i;

        while (buffer[i] != '\0' && buffer[i] != '\n')
            i++;

        if (i - start > klen && buffer[start + klen] == '=')
        {
            int match = 1;

            for (int k = 0; k < klen; k++)
            {
                if (buffer[start + k] != key[k])
                {
                    match = 0;
                    break;
                }
            }

            if (match)
            {
                int j = start + klen + 1;
                int o = 0;

                while (j < i && buffer[j] != '\r' && o < out_max - 1)
                    out[o++] = buffer[j++];

                out[o] = '\0';
                return 1;
            }
        }

        if (buffer[i] == '\n') i++;
    }

    return 0;
}

void settings_load(void)
{
    char buffer[SETTINGS_MAX];
    char value[80];
    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1)
        return;

    current_dir = sys_dir;

    if (fs_read_text(SETTINGS_FILE, buffer, sizeof(buffer)))
    {
        if (cfg_find_key(buffer, "name", value, sizeof(value)))
            cfg_strcpy(cfg_user, value, sizeof(cfg_user));

        if (cfg_find_key(buffer, "OS", value, sizeof(value)))
            cfg_strcpy(cfg_os, value, sizeof(cfg_os));

        if (cfg_find_key(buffer, "hostname", value, sizeof(value)))
            cfg_strcpy(cfg_hostname, value, sizeof(cfg_hostname));

        if (cfg_find_key(buffer, "root_salt", value, sizeof(value)))
            cfg_strcpy(cfg_root_salt, value, sizeof(cfg_root_salt));

        if (cfg_find_key(buffer, "root_hash", value, sizeof(value)))
            cfg_strcpy(cfg_root_hash, value, sizeof(cfg_root_hash));

        if (cfg_find_key(buffer, "require_login", value, sizeof(value)))
            cfg_require_login = (strcmp(value, "1") == 0);

        if (cfg_find_key(buffer, "disk_history", value, sizeof(value)))
                    cfg_disk_history = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);

        if (cfg_find_key(buffer, "setup_complete", value, sizeof(value)))
            cfg_setup_complete = (strcmp(value, "1") == 0);

        if (cfg_find_key(buffer, "prompt", value, sizeof(value)))
        {
            cfg_strcpy(invite_user, value, 28);

            if (strcmp(value, "unix") == 0)
                shell_p = 4;
            else if (strcmp(value, "drive") == 0)
                shell_p = 3;
            else if (strcmp(value, "D:/> ") == 0)
                shell_p = 1;
            else if (strcmp(value, "C:/> ") == 0)
                shell_p = 2;
            else if (strcmp(value, OS_NAME "> ") == 0)
                shell_p = 0;
            else
                shell_p = 9;
        }

        if (cfg_find_key(buffer, "prompt_color", value, sizeof(value)))
            prompt_color = color_from_name(value);

        if (cfg_find_key(buffer, "color", value, sizeof(value)))
            base_color = color_from_name(value);

        if (cfg_find_key(buffer, "help_color", value, sizeof(value)))
            text_color = color_from_name(value);
    }

    current_dir = prev_dir;
}

static void cfg_append(char* out, int* pos, int max, const char* key, const char* value)
{
    int i = 0;

    while (key[i] != '\0' && *pos < max - 1)
        out[(*pos)++] = key[i++];

    if (*pos < max - 1) out[(*pos)++] = '=';

    i = 0;
    while (value[i] != '\0' && *pos < max - 1)
        out[(*pos)++] = value[i++];

    if (*pos < max - 1) out[(*pos)++] = '\n';

    out[*pos] = '\0';
}

void settings_save(void)
{
    char buffer[SETTINGS_MAX];
    int pos = 0;
    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1)
        return;

    current_dir = sys_dir;

    buffer[0] = '\0';

    cfg_append(buffer, &pos, sizeof(buffer),
            "name", cfg_user);

    cfg_append(buffer, &pos, sizeof(buffer),
            "OS", cfg_os);

    cfg_append(buffer, &pos, sizeof(buffer),
            "hostname", cfg_hostname);

    cfg_append(buffer, &pos, sizeof(buffer),
            "root_salt", cfg_root_salt);

    cfg_append(buffer, &pos, sizeof(buffer),
            "root_hash", cfg_root_hash);

    cfg_append(buffer, &pos, sizeof(buffer),
            "require_login",
            cfg_require_login ? "1" : "0");

    cfg_append(buffer, &pos, sizeof(buffer),
                "disk_history",
                cfg_disk_history ? "true" : "false");

    cfg_append(buffer, &pos, sizeof(buffer),
            "version", OS_VERSION_STR);

    if (shell_p == 4)
        cfg_append(buffer, &pos, sizeof(buffer),
                "prompt", "unix");
    else if (shell_p == 3)
        cfg_append(buffer, &pos, sizeof(buffer),
                "prompt", "drive");
    else
        cfg_append(buffer, &pos, sizeof(buffer),
                "prompt", invite_user);

    cfg_append(buffer, &pos, sizeof(buffer),
            "prompt_color",
            color_names[prompt_color & 0x0F]);

    cfg_append(buffer, &pos, sizeof(buffer),
            "color",
            color_names[base_color & 0x0F]);

    cfg_append(buffer, &pos, sizeof(buffer),
            "help_color",
            color_names[text_color & 0x0F]);

    cfg_append(buffer, &pos, sizeof(buffer),
            "setup_complete",
            cfg_setup_complete ? "1" : "0");

    if (fs_find(SETTINGS_FILE) == -1)
        fs_create(SETTINGS_FILE);

    fs_write(SETTINGS_FILE, buffer);

    current_dir = prev_dir;
}

static void prompt_append(
    char* out,
    int capacity,
    int* pos,
    const char* text
)
{
    while (*text && *pos < capacity - 1)
        out[(*pos)++] = *text++;

    out[*pos] = '\0';
}

static const char* shell_prompt_text(void)
{
    static char drive_prompt[8];
    static char unix_prompt[384];

    if (shell_p == 4)
    {
        char home[80];
        const char* cwd = fs_current_path();
        int pos = 0;

        unix_prompt[0] = '\0';

        if (!fs_build_home_path(home, sizeof(home)))
            cfg_strcpy(home, "/home/user", sizeof(home));

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, cfg_user);

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, "@");

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, cfg_hostname);

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, ":");

        int home_len = 0;

        while (home[home_len] != '\0')
            home_len++;

        if (strncmp(cwd, home, home_len) == 0 &&
            (cwd[home_len] == '\0' || cwd[home_len] == '/'))
        {
            prompt_append(unix_prompt, sizeof(unix_prompt),
                          &pos, "~");

            prompt_append(unix_prompt, sizeof(unix_prompt),
                          &pos, cwd + home_len);
        }
        else
        {
            prompt_append(unix_prompt, sizeof(unix_prompt),
                          &pos, cwd);
        }

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, "$ ");

        return unix_prompt;
    }

    if (shell_p == 0)
        return OS_NAME "> ";

    if (shell_p == 1)
        return "D:/> ";

    if (shell_p == 2)
        return "C:/> ";

    if (shell_p == 3)
    {
        drive_prompt[0] = fs_get_drive_letter();
        drive_prompt[1] = ':';
        drive_prompt[2] = '/';
        drive_prompt[3] = '>';
        drive_prompt[4] = ' ';
        drive_prompt[5] = '\0';

        return drive_prompt;
    }

    return invite_user;
}

void cmd_settings(void)
{
    print_title("\n=== SETTINGS ===\n\n");
    print("user        = "); print(cfg_user);        putchar_os('\n');
    print("os          = "); print(cfg_os);          putchar_os('\n');
    print("version     = "); print(OS_VERSION_STR);  putchar_os('\n');
    print("prompt      = "); print(shell_prompt_text()); putchar_os('\n');
    print("prompt_color= "); print(color_names[prompt_color & 0x0F]); putchar_os('\n');
    print("color       = "); print(color_names[base_color & 0x0F]);   putchar_os('\n');
    print("help_color  = "); print(color_names[text_color & 0x0F]);   putchar_os('\n');
    print("disk        = "); putchar_os(fs_get_drive_letter());      putchar_os('\n');
    print("hostname    = "); print(cfg_hostname);    putchar_os('\n'); putchar_os('\n');
}

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

static void print_size_bytes(unsigned long long bytes)
{
    if (bytes >= SIZE_GB)
        print_size_unit(bytes, SIZE_GB, 2, "GB");
    else if (bytes >= SIZE_MB)
        print_size_unit(bytes, SIZE_MB, 2, "MB");
    else if (bytes >= SIZE_KB)
        print_size_unit(bytes, SIZE_KB, 2, "KB");
    else
    {
        print_uint64(bytes);
        print(" bytes");
    }
}

static unsigned long long rootfs_total_size(void)
{
    unsigned long long total = 0;

    for (int i = 0; i < ROOTFS_FILE_COUNT; i++)
    {
        total += rootfs_files[i].size;
    }

    return total;
}

static unsigned long long install_disk_usage(void)
{
    return (unsigned long long)(1 + FS_DIR_SECTORS + MAX_FILES * FS_FILE_SECTORS) * 512ULL;
}

static void installer_print_disks(int selected)
{
    int n = ata_drive_count();

    for (int i = 0; i < n; i++)
    {
        ata_drive_t* d = ata_get_drive(i);

        print("    ");

        if (selected == i) set_color(COLOR_LIGHT_GREEN);
        else set_color(base_color);

        print_int(i + 1);
        print(") ");

        putchar_os(d->letter);
        print(":  ");

        set_color(COLOR_WHITE);
        print(d->model);
        set_color(base_color);

        print("   ");
        print_size_bytes((unsigned long long)d->sectors * 512ULL);

        if (fs_drive_has_fs(i))
        {
            set_color(COLOR_LIGHT_RED);
            print("  [has " OS_NAME "]");
            set_color(base_color);
        }

        if (selected == i) set_color(COLOR_LIGHT_GREEN);
        else set_color(base_color);

        putchar_os('\n');
    }
}

static int installer_collect_setup(void)
{
    char input[64];
    char password[64];
    char confirm[64];

    print_title("\n=== NwOS SETUP ===\n\n");
    print("Configure your new NwOS installation.\n\n");

    while (1)
    {
        print("Hostname [nwos]: ");
        read_line(input, sizeof(input));

        if (input[0] == '\0')
        {
            cfg_strcpy(cfg_hostname, "nwos", sizeof(cfg_hostname));
            break;
        }

        if (normalize_hostname(input))
        {
            cfg_strcpy(cfg_hostname, input, sizeof(cfg_hostname));
            break;
        }

        print_error("Invalid hostname. Use letters, digits and '-'.\n");
    }

    while (1)
    {
        print("Username [user]: ");
        read_line(input, 32);

        if (input[0] == '\0')
        {
            cfg_strcpy(cfg_user, "user", sizeof(cfg_user));
            break;
        }

        if (normalize_username(input))
        {
            cfg_strcpy(cfg_user, input, sizeof(cfg_user));
            break;
        }

        print_error("Invalid username. Use 1-31 characters; root is reserved.\n");
    }

    while (1)
    {
        print("Root password: ");
        int len = read_password(password, sizeof(password));

        print("Confirm root password: ");
        read_password(confirm, sizeof(confirm));

        if (strcmp(password, confirm) != 0)
        {
            print_error("Passwords do not match. Try again.\n");
            continue;
        }

        break;
    }

    make_root_salt(cfg_root_salt);
    nw_password_hash(password, cfg_root_salt, cfg_root_hash);

    for (int i = 0; i < (int)sizeof(password); i++)
    {
        password[i] = '\0';
        confirm[i] = '\0';
    }

    cfg_setup_complete = 1;
    shell_p = 4;

    print_success("\nSetup configuration collected.\n");
    print("Hostname: ");
    print(cfg_hostname);
    print("\nUsername: ");
    print(cfg_user);
    print("\n\n");

    return 1;
}

static void cmd_install(void)
{
    if (root_password_is_set() &&
        !cmd_sudo &&
        !authenticate_root_password())
    {
        return;
    }
    installer_cancelled = 0;

    installer();

    if (installer_cancelled)
    {
        print("Installation cancelled. The system keeps running.\n");
    }
}

void installer(void) {
    clear();
    fb_init();
    set_color(COLOR_CYAN);
    print("\n");
    print("  +======================================+\n");
    print("  |     " OS_NAME " Installation Wizard " OS_VERSION_STR "   |\n");
    print("  +======================================+\n\n");
    set_color(base_color);
    
    print("  Welcome to the " OS_NAME " installer.\n");
    print("  Detected disks are listed below. Nothing is written until\n");
    print("  you explicitly confirm formatting.\n\n");

    ata_init();

    int n = ata_drive_count();

    if (n == 0)
    {
        print_error("  No ATA disks detected!\n");
        print("  Add one to QEMU: -drive file=disk.img,format=raw,if=ide,index=0\n");
        installer_cancelled = 1;
        return;
    }

    if (!installer_collect_setup())
    {
        installer_cancelled = 1;
        return;
    }

    print_title("\n  Disk configuration:\n");
    print("    Automatic : MBR + one partition starting at LBA 2048\n");
    print("    Legacy    : whole disk, raw NwFS starting at LBA 0\n");
    print("    Nothing is written before the final confirmation.\n\n");

    print_title("  Detected disks:\n");
    installer_print_disks(-1);

    print_title("\n  Installation size:\n");
    print("    Files:       ");
    print_int(ROOTFS_FILE_COUNT);
    print("\n    Payload:     ");
    print_size_bytes(rootfs_total_size());
    print("\n    Disk usage:  ");
    print_size_bytes(install_disk_usage());
    print(" (");
    print_int(1 + FS_DIR_SECTORS + MAX_FILES * FS_FILE_SECTORS);
    print(" sectors)\n");

    char buf[32];
    int sel = -1;

    while (sel < 0)
    {
        print("\n  Select target disk (1-");
        print_int(n);
        print("), default 1: ");

        read_line(buf, 32);

        if (buf[0] == '\0')
        {
            sel = 1;
            continue;
        }

        int value = atoi_simple(buf);

        if (value < 1 || value > n)
        {
            print_error("  Invalid disk number.\n");
            continue;
        }

        sel = value;
    }

    ata_drive_t* target = ata_get_drive(sel - 1);

    if (target == 0)
    {
        print_error("\n  Disk is not available.\n");
        installer_cancelled = 1;
        return;
    }

    print("\n  Target disk:\n");
    installer_print_disks(sel - 1);

    if (fs_drive_has_fs(sel - 1))
    {
        print_error("\n  WARNING: this disk already contains an " OS_NAME " installation.\n");
    }

    print("\n  Target layout:\n");
    print("    1) Automatic MBR partition (recommended)\n");
    print("    2) Whole disk, raw NwFS (legacy)\n");
    print("  Choice [1]: ");

    read_line(buf, 32);

    int use_mbr = !(buf[0] == '2');
    unsigned int start_lba = use_mbr ? FS_PART_START_LBA : 0u;
    unsigned int needed_sectors = start_lba + fs_layout_sectors();

    if (target->sectors < needed_sectors)
    {
        print_error("\n  Disk is too small for " OS_NAME ".\n");
        print("  Required: ");
        print_size_bytes((unsigned long long)needed_sectors * 512ULL);
        print(", available: ");
        print_size_bytes((unsigned long long)target->sectors * 512ULL);
        print("\n");
        installer_cancelled = 1;
        return;
    }

    print("Require a password at startup? [Y/n]: ");

    char answer[8];
    read_line(answer, sizeof(answer));

    if (answer[0] == 'n' || answer[0] == 'N')
        cfg_require_login = 0;
    else
        cfg_require_login = 1;

    cfg_setup_complete = 1;

    print("\n  This will FORMAT disk ");
    putchar_os(target->letter);
    print(": and erase every partition and file on it.\n");
    print("  Layout: ");
    print(use_mbr ? "MBR + partition at LBA 2048" : "raw NwFS from LBA 0");
    print("\n  Type 'yes' to format and install: ");

    read_line(buf, 32);

    if (strcmp(buf, "yes") != 0)
    {
        print_error("\n  Cancelled. Nothing was changed.\n");
        installer_cancelled = 1;
        return;
    }

    print_success("\n\n  Installing to ");
    putchar_os(target->letter);
    print(": ...\n");
    sleep(500);

    fs_ram_mode = 0;
    fs_part_lba = 0;

    if (use_mbr)
    {
        print("  Writing MBR partition table...\n");

        if (fs_write_mbr_partition(sel - 1, FS_PART_START_LBA,
                                   target->sectors) != 0)
        {
            print_error("  Could not write the partition table.\n");
            installer_cancelled = 1;
            return;
        }

        fs_part_lba = FS_PART_START_LBA;
    }

    fs_set_drive(sel - 1);
    fs_format_disk(sel - 1);
    
    print_success("\n  Installation complete!\n");
    print("  " OS_NAME " was installed on ");
    putchar_os(target->letter);
    print(":\n");
    print("    Files:       ");
    print_int(ROOTFS_FILE_COUNT);
    print("\n    Payload:     ");
    print_size_bytes(rootfs_total_size());
    print("\n    Disk usage:  ");
    print_size_bytes(install_disk_usage());
    print("\n");
    print("  Boot from the " OS_NAME " ISO/USB again; the installed disk\n");
    print("  is detected automatically and appears in the boot menu.\n\n");
    for (int s = 3; s > 0; s--) {
        print("Reboot in ");
        print_int(s);
        print(" seconds...\n");
        sleep(1000);
    }
    reboot();
}

static int login_prompt(void)
{
    if (!cfg_require_login)
        return 1;

    if (!root_password_is_set())
    {
        print_error("Login is enabled but no root password is configured.\n");
        return 0;
    }

    while (1)
    {
        char password[64];
        int tries;

        print("\nNwOS login: ");
        print(cfg_user);
        print("\nPassword: ");

        read_password(password, sizeof(password));

        int valid = verify_root_password(password);
        wipe_secret(password, sizeof(password));

        if (valid)
        {
            print_success("Login successful.\n");
            return 1;
        }

        print_error("Incorrect password. Try again.\n");
        tries++;

        sleep(1000);
    }
}

#define BOOT_ACTION_SYSTEM 0
#define BOOT_ACTION_LIVE   1
#define BOOT_ACTION_SETUP  2

static int boot_menu(int has_installed)
{
    char input[16];

    clear();
    fb_init();

    set_color(COLOR_CYAN);
    print("\n");
    print("  +==========================================+\n");
    print("  |          " OS_NAME " " OS_VERSION_STR " Boot Menu          |\n");
    print("  +==========================================+\n\n");
    set_color(base_color);

    if (has_installed)
    {
        print("  1) Boot installed " OS_NAME "\n");
        print("  2) Live mode (runs in RAM, disks are not modified)\n");
        print("  3) Installer (detect, partition and format a disk)\n\n");
        print("  Selection [1]: ");
    }
    else
    {
        print("  No installed " OS_NAME " filesystem was found.\n\n");
        print("  1) Live mode (runs in RAM, disks are not modified)\n");
        print("  2) Installer (detect, partition and format a disk)\n\n");
        print("  Selection [1]: ");
    }

    read_line(input, sizeof(input));

    int value = (input[0] == '\0') ? 1 : atoi_simple(input);

    if (has_installed)
    {
        if (value == 2) return BOOT_ACTION_LIVE;
        if (value == 3) return BOOT_ACTION_SETUP;
        return BOOT_ACTION_SYSTEM;
    }

    if (value == 2) return BOOT_ACTION_SETUP;

    return BOOT_ACTION_LIVE;
}

static void boot_system(void)
{
    print("CLEARING...\n");
    startingNWOS();
    settings_load();
    print("Starting " OS_NAME "...\n");
    sleep(500);
    print_success("Kernel Started!\n");
    print("Clearing..\n");

    sleep(1500);

    clear();

    fb_draw_image(((int)fb_get_width() - LOGO_NWOS_WIDTH) / 2, ((int)fb_get_height() - LOGO_NWOS_HEIGHT) / 2, LogoNwOS, LOGO_NWOS_WIDTH, LOGO_NWOS_HEIGHT);

    sleep(1500);

    print("================================\n");
    print("        Welcome to " OS_NAME " " OS_VERSION_STR "\n");
    print("================================\n\n");
    set_color(COLOR_LIGHT_GREEN);
    print("copyright (c) User0140155 2026\n\n");
    set_color(COLOR_LIGHT_GRAY);
    print("Github: https://github.com/User0140155/NwOS"); print("\n");
    print("Type 'help' for commands.\n\n");
    set_color(COLOR_GREEN);
    print("================");
    set_color(COLOR_BLUE);
    print("================\n");
    set_color(COLOR_LIGHT_GRAY);

    fs_enter_user_home();
    if (!login_prompt())
    {
        print_error("error\n");
        while (1)
            __asm__ volatile ("hlt");
    }
    shell();

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}

void kernel_main(void) {
    fb_init();
    
    if (!fb_is_available()) {
        volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
        
        for (int i = 0; i < 80 * 25; i++) vga[i] = 0x4F20;

        const char* msg1 = " FATAL ERROR: Framebuffer not found! ";
        for (int i = 0; msg1[i]; i++) vga[2 * 80 + 10 + i] = (uint16_t)msg1[i] | (0x4F << 8);

        const char* msg2 = " Check limine framebuffer configuration. ";
        for (int i = 0; msg2[i]; i++) vga[4 * 80 + 5 + i] = (uint16_t)msg2[i] | (0x4F << 8);
        
        const char* msg3 = " If booting from HDD, try NwOS.iso instead. ";
        for (int i = 0; msg3[i]; i++) vga[6 * 80 + 5 + i] = (uint16_t)msg3[i] | (0x4F << 8);

        while (1) __asm__ volatile ("hlt");
    }

    ata_init();

    int status = fs_load_status();

    if (status == 2)
        dead_boot();

    int has_installed = (status == 1);

    while (1)
    {
        int action = boot_menu(has_installed);

        if (action == BOOT_ACTION_LIVE)
        {
            clear();
            print("Preparing " OS_NAME " live environment in RAM...\n");
            fs_live_init();
            boot_system();
        }

        if (action == BOOT_ACTION_SETUP)
        {
            installer_cancelled = 0;
            installer();

            if (installer_cancelled)
            {
                print("\nInstaller cancelled. Returning to boot menu...\n");
                sleep(1500);
            }

            continue;
        }

        ata_init();

        int status = fs_load_status();

        if (status == 2)
            dead_boot();

        if (status == 1)
        {
            has_installed = 1;
            boot_system();
        }

        has_installed = 0;
        print_error("No installed " OS_NAME " found. Choose live mode or the installer.\n");
        sleep(2000);
    }
}
