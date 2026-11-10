#include "fb.h"
#include "char_symbol.h"
#include "wallpaper_data.h"

extern uint32_t multiboot_info_ptr;

static uint32_t* fb_addr = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static int fb_bpp = 0;
static uint8_t fb_type = 0;
static uint8_t fb_red_pos = 16, fb_red_size = 8;
static uint8_t fb_green_pos = 8, fb_green_size = 8;
static uint8_t fb_blue_pos = 0, fb_blue_size = 8;

#define FB_BACKBUFFER_WIDTH  1280
#define FB_BACKBUFFER_HEIGHT 720

static uint32_t fb_backbuffer_pixels[FB_BACKBUFFER_WIDTH * FB_BACKBUFFER_HEIGHT];
static uint32_t* fb_physical = 0;
static uint32_t fb_physical_pitch = 0;
static int fb_back_active = 0;
static int fb_back_enabled = 1;
static unsigned long long fb_present_us = 0;

extern unsigned long long timer_us(void);

void fb_backbuffer_end(void);

static const uint8_t font8x8_basic[128][8] = {
    [' ']={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    ['!']={0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00},
    ['"']={0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00},
    ['#']={0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    ['$']={0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    ['%']={0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    ['&']={0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
    ['\'']={0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00},
    ['(']={0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00},
    [')']={0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00},
    ['*']={0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    ['+']={0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    [',']={0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06},
    ['-']={0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    ['.']={0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00},
    ['/']={0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    ['0']={0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    ['1']={0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    ['2']={0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    ['3']={0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    ['4']={0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    ['5']={0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    ['6']={0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    ['7']={0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    ['8']={0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    ['9']={0x1E,0x33,0x33,0x3F,0x30,0x18,0x0E,0x00},
    [':']={0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00},
    [';']={0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06},
    ['<']={0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    ['=']={0x00,0x00,0x3F,0x00,0x3F,0x00,0x00,0x00},
    ['>']={0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    ['?']={0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00},
    ['@']={0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00},
    ['A']={0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    ['B']={0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    ['C']={0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    ['D']={0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    ['E']={0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    ['F']={0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    ['G']={0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    ['H']={0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    ['I']={0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    ['J']={0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    ['K']={0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    ['L']={0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    ['M']={0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    ['N']={0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    ['O']={0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    ['P']={0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    ['Q']={0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    ['R']={0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    ['S']={0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    ['T']={0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    ['U']={0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    ['V']={0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    ['W']={0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    ['X']={0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    ['Y']={0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    ['Z']={0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    ['[']={0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00},
    ['\\']={0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00},
    [']']={0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00},
    ['^']={0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00},
    ['_']={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    ['`']={0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00},
    ['a']={0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00},
    ['b']={0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00},
    ['c']={0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00},
    ['d']={0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00},
    ['e']={0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00},
    ['f']={0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00},
    ['g']={0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    ['h']={0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00},
    ['i']={0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00},
    ['j']={0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E},
    ['k']={0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00},
    ['l']={0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    ['m']={0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00},
    ['n']={0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00},
    ['o']={0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00},
    ['p']={0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    ['q']={0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78},
    ['r']={0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00},
    ['s']={0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00},
    ['t']={0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00},
    ['u']={0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00},
    ['v']={0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00},
    ['w']={0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00},
    ['x']={0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00},
    ['y']={0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F},
    ['z']={0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00},
    ['{']={0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00},
    ['|']={0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00},
    ['}']={0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00},
    ['~']={0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},
};

static const uint8_t* fb_glyph_for(unsigned char c) {
    const uint8_t* symbol = char_symbol_glyph(c);

    if (symbol)
        return symbol;

    return font8x8_basic[c & 0x7F];
}

static uint32_t vga_to_rgb(uint8_t color) {
    static const uint32_t pal[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };
    return pal[color & 0x0F];
}

static int fb_initialized = 0;

static uint32_t fb_pack_rgb(uint32_t rgb)
{
    uint32_t r = (rgb >> 16) & 0xFFu;
    uint32_t g = (rgb >> 8) & 0xFFu;
    uint32_t b = rgb & 0xFFu;

    if (fb_red_size == 8 && fb_green_size == 8 && fb_blue_size == 8)
        return (r << fb_red_pos) | (g << fb_green_pos) | (b << fb_blue_pos);

    {
        uint32_t rmax = (1u << fb_red_size) - 1u;
        uint32_t gmax = (1u << fb_green_size) - 1u;
        uint32_t bmax = (1u << fb_blue_size) - 1u;
        r = (r * rmax + 127u) / 255u;
        g = (g * gmax + 127u) / 255u;
        b = (b * bmax + 127u) / 255u;
    }
    return (r << fb_red_pos) | (g << fb_green_pos) | (b << fb_blue_pos);
}

void fb_init(void)
{
    if (fb_initialized || !multiboot_info_ptr)
        return;

    uint8_t* ptr = (uint8_t*)(uintptr_t)multiboot_info_ptr;
    uint32_t total_size = *(uint32_t*)ptr;
    if (total_size < 16)
        return;

    uint8_t* end = ptr + total_size;
    uint8_t* tag = ptr + 8;

    while (tag + 8 <= end) {
        uint32_t type = *(uint32_t*)tag;
        uint32_t size = *(uint32_t*)(tag + 4);

        if (size < 8 || size > (uint32_t)(end - tag))
            break;
        if (type == 0)
            break;

        if (type == 8 && size >= 38) {
            uint64_t addr = *(uint64_t*)(tag + 8);
            uint32_t pitch = *(uint32_t*)(tag + 16);
            uint32_t width = *(uint32_t*)(tag + 20);
            uint32_t height = *(uint32_t*)(tag + 24);
            uint8_t bpp = tag[28];
            uint8_t kind = tag[29];
            uint8_t red_pos = tag[32], red_size = tag[33];
            uint8_t green_pos = tag[34], green_size = tag[35];
            uint8_t blue_pos = tag[36], blue_size = tag[37];

            uint64_t bytes = (uint64_t)pitch * height;
            uint64_t row_bytes = (uint64_t)width * 4u;

            int address_mapped = addr != 0 && addr < 0x100000000ULL &&
                bytes != 0 && bytes <= 0x100000000ULL - addr;
            int masks_valid = red_size && red_size <= 8 &&
                green_size && green_size <= 8 &&
                blue_size && blue_size <= 8 &&
                red_pos + red_size <= 32 &&
                green_pos + green_size <= 32 &&
                blue_pos + blue_size <= 32;

            if (kind == 1 && bpp == 32 && width && height &&
                pitch >= row_bytes && address_mapped && masks_valid) {
                fb_addr = (uint32_t*)(uintptr_t)addr;
                fb_pitch = pitch;
                fb_width = width;
                fb_height = height;
                fb_bpp = bpp;
                fb_type = kind;
                fb_red_pos = red_pos;
                fb_red_size = red_size;
                fb_green_pos = green_pos;
                fb_green_size = green_size;
                fb_blue_pos = blue_pos;
                fb_blue_size = blue_size;
                fb_initialized = 1;
                return;
            }
        }

        tag += (size + 7u) & ~7u;
    }
}

int fb_set_mode(uint32_t width, uint32_t height)
{
    if (!fb_is_available() || !width || !height)
        return 0;

    return fb_width == width && fb_height == height;
}

int fb_is_available(void)
{
    return fb_addr != 0 && fb_bpp == 32 && fb_type == 1 &&
           fb_width != 0 && fb_height != 0 &&
           fb_pitch >= fb_width * 4u;
}

uint32_t fb_get_width(void)
{
    return fb_width;
}

uint32_t fb_get_height(void)
{
    return fb_height;
}

void fb_clear(uint32_t color) {
    if (!fb_addr) return;

    color = fb_pack_rgb(color);

    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch);
        for (uint32_t x = 0; x < fb_width; x++)
            row[x] = color;
    }
}

void fb_draw_char(int x, int y, char c, uint8_t color) {
    if (!fb_addr) return;
    
    int px_start = x * 8;
    int py_start = y * 8;
    
    uint32_t fg_rgb = fb_pack_rgb(vga_to_rgb(color & 0x0F));
    uint32_t bg_rgb = fb_pack_rgb(vga_to_rgb((color >> 4) & 0x0F));
    
    const uint8_t* glyph = fb_glyph_for((uint8_t)c);
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int px = px_start + col;
            int py = py_start + row;
            if (px >= (int)fb_width || py >= (int)fb_height || px < 0 || py < 0) continue;
            
            uint32_t color_rgb = ((glyph[row] >> col) & 1) ? fg_rgb : bg_rgb;
            uint32_t* pixel = (uint32_t*)((uint8_t*)fb_addr + py * fb_pitch + px * 4);
            *pixel = color_rgb;
        }
    }
}

void fb_draw_char_transparent(int x, int y, char c, uint8_t color) {
    if (!fb_addr) return;

    int px_start = x * 8;
    int py_start = y * 8;

    uint32_t fg_rgb = fb_pack_rgb(vga_to_rgb(color & 0x0F));

    const uint8_t* glyph = fb_glyph_for((uint8_t)c);

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (!((glyph[row] >> col) & 1)) continue;

            int px = px_start + col;
            int py = py_start + row;
            if (px >= (int)fb_width || py >= (int)fb_height || px < 0 || py < 0) continue;

            uint32_t* pixel = (uint32_t*)((uint8_t*)fb_addr + py * fb_pitch + px * 4);
            *pixel = fg_rgb;
        }
    }
}

static void fb_fill_rect(int x0, int y0, int x1, int y1, uint32_t rgb) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)fb_width) x1 = (int)fb_width;
    if (y1 > (int)fb_height) y1 = (int)fb_height;

    rgb = fb_pack_rgb(rgb);

    for (int y = y0; y < y1; y++) {
        uint32_t* row = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch);
        for (int x = x0; x < x1; x++)
            row[x] = rgb;
    }
}
static void fb_draw_char_box_fast_impl(
    int x0, int y0, int x1, int y1,
    char c, uint8_t color, int transparent)
{
    if (!fb_addr || x1 <= x0 || y1 <= y0)
        return;

    const int w = x1 - x0;
    const int h = y1 - y0;

    int clip_x0 = x0 < 0 ? 0 : x0;
    int clip_y0 = y0 < 0 ? 0 : y0;
    int clip_x1 = x1 > (int)fb_width ? (int)fb_width : x1;
    int clip_y1 = y1 > (int)fb_height ? (int)fb_height : y1;

    if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1)
        return;

    const uint32_t fg = fb_pack_rgb(vga_to_rgb(color & 0x0F));
    const uint32_t bg = fb_pack_rgb(vga_to_rgb((color >> 4) & 0x0F));
    const uint8_t *glyph = fb_glyph_for((uint8_t)c);

    /* Start the source-x scaler at the first visible destination pixel. */
    uint64_t x_acc_start = (uint64_t)(clip_x0 - x0) * 8u;
    unsigned int sx_start = (unsigned int)(x_acc_start / (unsigned int)w);
    x_acc_start %= (unsigned int)w;

    for (int y = clip_y0; y < clip_y1; ++y) {
        const unsigned int sy =
            (unsigned int)(((uint64_t)(y - y0) * 8u) / (unsigned int)h);
        const uint8_t glyph_row = glyph[sy < 8u ? sy : 7u];
        uint32_t *dst = (uint32_t *)((uint8_t *)fb_addr + (uint64_t)y * fb_pitch);

        unsigned int sx = sx_start;
        uint64_t x_acc = x_acc_start;

        for (int x = clip_x0; x < clip_x1; ++x) {
            const int set = (glyph_row >> (sx < 8u ? sx : 7u)) & 1u;
            if (set)
                dst[x] = fg;
            else if (!transparent)
                dst[x] = bg;

            x_acc += 8u;
            while (x_acc >= (unsigned int)w) {
                x_acc -= (unsigned int)w;
                if (sx < 7u)
                    ++sx;
            }
        }
    }
}

void fb_draw_char_box(int x0, int y0, int x1, int y1, char c, uint8_t color)
{
    fb_draw_char_box_fast_impl(x0, y0, x1, y1, c, color, 0);
}
void fb_draw_char_box_transparent(int x0, int y0, int x1, int y1, char c, uint8_t color)
{
    fb_draw_char_box_fast_impl(x0, y0, x1, y1, c, color, 1);
}

void fb_draw_image(int x, int y, const uint16_t* data, int width, int height) {
    if (!fb_addr || !data || width <= 0 || height <= 0)
        return;

    for (int j = 0; j < height; ++j) {
        int py = y + j;
        if (py < 0 || py >= (int)fb_height)
            continue;

        const uint16_t *src_row = data + (uint64_t)(unsigned int)j * (unsigned int)width;
        uint32_t *dst_row = (uint32_t *)((uint8_t *)fb_addr + (uint64_t)(unsigned int)py * fb_pitch);

        for (int i = 0; i < width; ++i) {
            int px = x + i;
            if (px < 0 || px >= (int)fb_width)
                continue;

            uint16_t p = src_row[i];
            uint32_t r = (p >> 11) & 0x1Fu;
            uint32_t g = (p >> 5) & 0x3Fu;
            uint32_t b = p & 0x1Fu;
            uint32_t rgb = (((r << 3) | (r >> 2)) << 16) |
                           (((g << 2) | (g >> 4)) << 8) |
                           ((b << 3) | (b >> 2));
            dst_row[px] = fb_pack_rgb(rgb);
        }
    }
}

void fb_draw_wallpaper(void) {
#if WALLPAPER_PRESENT
    if (!fb_addr || !fb_width || !fb_height || !WALLPAPER_WIDTH || !WALLPAPER_HEIGHT)
        return;

    if (WALLPAPER_WIDTH == (int)fb_width && WALLPAPER_HEIGHT == (int)fb_height) {
        fb_draw_image(0, 0, wallpaper_pixels, WALLPAPER_WIDTH, WALLPAPER_HEIGHT);
        return;
    }

    uint32_t sy = 0;
    uint64_t y_rem = 0;

    for (uint32_t y = 0; y < fb_height; ++y) {
        uint32_t *dst_row = (uint32_t *)((uint8_t *)fb_addr + (uint64_t)y * fb_pitch);
        const uint16_t *src_row = wallpaper_pixels + (uint64_t)sy * WALLPAPER_WIDTH;
        uint32_t sx = 0;
        uint64_t x_rem = 0;

        for (uint32_t x = 0; x < fb_width; ++x) {
            uint16_t p = src_row[sx];
            uint32_t r = (p >> 11) & 0x1Fu;
            uint32_t g = (p >> 5) & 0x3Fu;
            uint32_t b = p & 0x1Fu;
            uint32_t rgb = (((r << 3) | (r >> 2)) << 16) |
                           (((g << 2) | (g >> 4)) << 8) |
                           ((b << 3) | (b >> 2));
            dst_row[x] = fb_pack_rgb(rgb);

            x_rem += WALLPAPER_WIDTH;
            while (x_rem >= fb_width) {
                x_rem -= fb_width;
                ++sx;
            }
        }

        y_rem += WALLPAPER_HEIGHT;
        while (y_rem >= fb_height) {
            y_rem -= fb_height;
            ++sy;
        }
    }
#endif
}


void fb_set_backbuffer_enabled(int enabled)
{
    fb_back_enabled = enabled;
}

void fb_backbuffer_begin(void) {
    if (!fb_back_enabled || fb_back_active || !fb_addr)
        return;

    if (fb_width > FB_BACKBUFFER_WIDTH ||
        fb_height > FB_BACKBUFFER_HEIGHT)
        return;

    fb_physical = fb_addr;
    fb_physical_pitch = fb_pitch;
    fb_addr = fb_backbuffer_pixels;
    fb_pitch = fb_width * 4;
    fb_back_active = 1;
}

void fb_backbuffer_end(void) {
    if (!fb_back_active)
        return;

    fb_addr = fb_physical;
    fb_pitch = fb_physical_pitch;
    fb_back_active = 0;
}

unsigned long long fb_get_present_us(void) {
    return fb_present_us;
}

void fb_present(void) {
    if (!fb_back_active || !fb_physical)
        return;

    unsigned long long start = timer_us();

    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t* dst = (uint32_t*)((uint8_t*)fb_physical + y * fb_physical_pitch);
        uint32_t* src = (uint32_t*)((uint8_t*)fb_backbuffer_pixels + y * fb_pitch);

        for (uint32_t x = 0; x < fb_width; x++)
            dst[x] = src[x];
    }

    fb_present_us += timer_us() - start;
}

static const unsigned short cursor_rows[19] = {
    0x800, 0xC00, 0xE00, 0xF00, 0xF80, 0xFC0, 0xFE0,
    0xFF0, 0xFF8, 0xFFC, 0xFFE, 0xFE0, 0xCE0, 0x0E0,
    0x0E0, 0x070, 0x070, 0x030, 0x030
};

void fb_draw_cursor(int x, int y) {
    if (!fb_addr) return;

    for (int pass = 0; pass < 2; pass++) {
        uint32_t color = (pass == 0) ? 0x00000000 : 0x00FFFFFF;

        for (int row = 0; row < 19; row++) {
            unsigned short bits = cursor_rows[row];
            int col = 0;

            while (col < 12) {
                while (col < 12 && !(bits & (0x800 >> col)))
                    col++;
                if (col >= 12)
                    break;

                int start = col;
                while (col < 12 && (bits & (0x800 >> col)))
                    col++;
                int end = col - 1;

                if (pass == 0)
                    fb_draw_rect(x + start - 1, y + row,
                                 end - start + 3, 1, color);
                else
                    fb_draw_rect(x + start, y + row,
                                 end - start + 1, 1, color);
            }
        }
    }
}

void fb_draw_int(int x, int y, int value, uint32_t color) {
    char buffer[16];
    int length = 0;
    unsigned int magnitude;

    if (value < 0) {
        magnitude = (unsigned int)(-(value + 1)) + 1u;
        buffer[length++] = '-';
    } else {
        magnitude = (unsigned int)value;
    }

    char digits[12];
    int digit_count = 0;

    if (magnitude == 0) {
        digits[digit_count++] = '0';
    } else {
        while (magnitude > 0) {
            digits[digit_count++] = (char)('0' + (magnitude % 10));
            magnitude /= 10;
        }
    }

    while (digit_count > 0)
        buffer[length++] = digits[--digit_count];

    buffer[length] = '\0';

    fb_draw_text(x, y, buffer, color);
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb_addr) return;
    color = fb_pack_rgb(color);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int px = x + i;
            int py = y + j;
            if (px >= (int)fb_width || py >= (int)fb_height || px < 0 || py < 0) continue;
            uint32_t* pixel = (uint32_t*)((uint8_t*)fb_addr + py * fb_pitch + px * 4);
            *pixel = color;
        }
    }
}

static void fb_put_pixel(int x, int y, uint32_t color) {
    if (!fb_addr) return;
    if (x < 0 || y < 0 || x >= (int)fb_width || y >= (int)fb_height) return;
    uint32_t* pixel = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch + x * 4);
    *pixel = fb_pack_rgb(color);
}

static int fb_abs(int v) {
    return v < 0 ? -v : v;
}

void fb_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = fb_abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -fb_abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void fb_draw_frame(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    int x2 = x + w - 1;
    int y2 = y + h - 1;

    fb_draw_line(x, y, x2, y, color);
    fb_draw_line(x, y2, x2, y2, color);
    fb_draw_line(x, y, x, y2, color);
    fb_draw_line(x2, y, x2, y2, color);
}

void fb_draw_cube(int x, int y, int size, uint32_t color) {
    if (size <= 0) return;

    int d = size / 4;

    int ax = x,           ay = y;
    int bx = x + size,    by = y;
    int cx = x + size,    cy = y + size;
    int dx = x,           dy = y + size;

    int ex = x + d,       ey = y - d;
    int fx = x + size + d, fy = y - d;
    int gx = x + size + d, gy = y + size - d;
    int hx = x + d,       hy = y + size - d;

    fb_draw_line(ax, ay, bx, by, color);
    fb_draw_line(bx, by, cx, cy, color);
    fb_draw_line(cx, cy, dx, dy, color);
    fb_draw_line(dx, dy, ax, ay, color);

    fb_draw_line(ex, ey, fx, fy, color);
    fb_draw_line(fx, fy, gx, gy, color);
    fb_draw_line(gx, gy, hx, hy, color);
    fb_draw_line(hx, hy, ex, ey, color);

    fb_draw_line(ax, ay, ex, ey, color);
    fb_draw_line(bx, by, fx, fy, color);
    fb_draw_line(cx, cy, gx, gy, color);
    fb_draw_line(dx, dy, hx, hy, color);
}

void fb_draw_cube_default(uint32_t color) {
    int size = (int)(fb_width / 4);
    if (fb_height / 4 < size) size = (int)(fb_height / 4);
    if (size < 32) size = 32;

    int x = (int)(fb_width / 2) - size / 2;
    int y = (int)(fb_height / 2) - size / 2 + size / 8;

    fb_draw_cube(x, y, size, color);
}
void fb_draw_text(int x, int y, const char* text, uint32_t color) {
    if (text == 0) return;

    color = fb_pack_rgb(color);

    int cx = x;
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            cx = x;
            y += 8;
            continue;
        }

        unsigned char ch = (unsigned char)text[i];
        const uint8_t* glyph = fb_glyph_for(ch);

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (!((glyph[row] >> col) & 1)) continue;

                int px = cx + col;
                int py = y + row;
                if (px < 0 || py < 0 || px >= (int)fb_width || py >= (int)fb_height)
                    continue;

                uint32_t* pixel = (uint32_t*)((uint8_t*)fb_addr + py * fb_pitch + px * 4);
                *pixel = color;
            }
        }

        cx += 8;
    }
}

void fb_draw_window(int x, int y, int w, int h, uint32_t color, const char* title) {
    if (w < 24 || h < 24) return;

    const uint32_t face = 0x00C0C0C0;
    const uint32_t light = 0x00FFFFFF;
    const uint32_t dark = 0x00404040;
    const uint32_t shadow = 0x00808080;

    fb_draw_rect(x, y, w, h, face);

    fb_draw_line(x, y, x + w - 1, y, light);
    fb_draw_line(x, y, x, y + h - 1, light);
    fb_draw_line(x, y + h - 1, x + w - 1, y + h - 1, dark);
    fb_draw_line(x + w - 1, y, x + w - 1, y + h - 1, dark);

    fb_draw_line(x + 2, y + 2, x + w - 3, y + 2, shadow);
    fb_draw_line(x + 2, y + 2, x + 2, y + h - 3, shadow);

    int title_y = y + 3;
    int title_h = 20;
    int title_x = x + 3;
    int title_w = w - 6;

    if (title_w > 0) {
        fb_draw_rect(title_x, title_y, title_w, title_h, color);

        fb_draw_line(title_x, title_y, title_x + title_w - 1, title_y, light);
        fb_draw_line(title_x, title_y, title_x, title_y + title_h - 1, light);
        fb_draw_line(title_x, title_y + title_h - 1,
                     title_x + title_w - 1, title_y + title_h - 1, dark);

        if (title != 0 && title[0] != '\0')
            fb_draw_text(title_x + 5, title_y + 6, title, 0x00FFFFFF);

        int bx = x + w - 20;
        int by = y + 7;

        fb_draw_rect(bx, by, 14, 14, face);
        fb_draw_line(bx, by, bx + 13, by, light);
        fb_draw_line(bx, by, bx, by + 13, light);
        fb_draw_line(bx, by + 13, bx + 13, by + 13, dark);
        fb_draw_line(bx + 13, by, bx + 13, by + 13, dark);

        fb_draw_line(bx + 4, by + 4, bx + 9, by + 9, 0x00000000);
        fb_draw_line(bx + 9, by + 4, bx + 4, by + 9, 0x00000000);
    }
}

/* ======================================================================
 * Frutiger Aero primitives: gradients, rounded corners, glass, glossy orbs,
 * ellipses and bubbles.
 * NwC: nw::gradient nw::rgrad nw::glass nw::orb nw::ellipse nw::bubble
 * All colors are 0x00RRGGBB, alpha is 0..255.
 * ====================================================================== */

static uint32_t aero_isqrt(uint32_t n)
{
    uint32_t res = 0;
    uint32_t bit = 1u << 30;

    while (bit > n)
        bit >>= 2;

    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }

    return res;
}

static int aero_fast_format(void)
{
    return fb_red_size == 8 && fb_green_size == 8 && fb_blue_size == 8;
}

static uint32_t aero_unpack(uint32_t pixel)
{
    uint32_t r = (pixel >> fb_red_pos)   & ((1u << fb_red_size)   - 1u);
    uint32_t g = (pixel >> fb_green_pos) & ((1u << fb_green_size) - 1u);
    uint32_t b = (pixel >> fb_blue_pos)  & ((1u << fb_blue_size)  - 1u);

    if (fb_red_size != 8)   r = (r * 255u) / ((1u << fb_red_size)   - 1u);
    if (fb_green_size != 8) g = (g * 255u) / ((1u << fb_green_size) - 1u);
    if (fb_blue_size != 8)  b = (b * 255u) / ((1u << fb_blue_size)  - 1u);

    return (r << 16) | (g << 8) | b;
}

/* a + (b - a) * num / den, per channel (used per row, not per pixel). */
static uint32_t aero_lerp(uint32_t a, uint32_t b, int num, int den)
{
    uint32_t out = 0;
    int shift;

    if (den <= 0)
        return a;
    if (num < 0) num = 0;
    if (num > den) num = den;

    for (shift = 16; shift >= 0; shift -= 8) {
        int ca = (int)((a >> shift) & 0xFFu);
        int cb = (int)((b >> shift) & 0xFFu);
        int c  = ca + ((cb - ca) * num) / den;
        if (c < 0) c = 0;
        if (c > 255) c = 255;
        out |= ((uint32_t)c) << shift;
    }

    return out;
}

/* Division-free mix: al = 0..256 */
static uint32_t aero_mix(uint32_t dst, uint32_t src, int al)
{
    int ia = 256 - al;
    uint32_t r = ((((dst >> 16) & 0xFFu) * ia) + (((src >> 16) & 0xFFu) * al)) >> 8;
    uint32_t g = ((((dst >> 8)  & 0xFFu) * ia) + (((src >> 8)  & 0xFFu) * al)) >> 8;
    uint32_t b = (((dst & 0xFFu) * ia) + ((src & 0xFFu) * al)) >> 8;
    return (r << 16) | (g << 8) | b;
}

/* Blend rgb over pixel (x,y), alpha 0..255. Clipped to the screen. */
static void aero_blend(int x, int y, uint32_t rgb, int alpha)
{
    uint32_t* p;

    if (alpha <= 0)
        return;
    if (x < 0 || y < 0 || x >= (int)fb_width || y >= (int)fb_height)
        return;

    p = (uint32_t*)((uint8_t*)fb_addr + (uint32_t)y * fb_pitch + (uint32_t)x * 4);

    if (alpha >= 255) {
        *p = fb_pack_rgb(rgb);
        return;
    }

    *p = fb_pack_rgb(aero_mix(aero_unpack(*p), rgb, alpha + (alpha >> 7)));
}

/* One horizontal span from xl256 to xr256 (1/256 px units), soft edges. */
static void aero_span(int py, int xl256, int xr256, uint32_t rgb, int alpha)
{
    int xl, xr, i;
    uint32_t* row;

    if (py < 0 || py >= (int)fb_height || xr256 <= xl256 || alpha <= 0)
        return;

    xl = xl256 >> 8;
    xr = (xr256 - 1) >> 8;

    if (xl == xr) {
        aero_blend(xl, py, rgb, (alpha * (xr256 - xl256)) >> 8);
        return;
    }

    aero_blend(xl, py, rgb, (alpha * (256 - (xl256 & 255))) >> 8);
    aero_blend(xr, py, rgb, (alpha * (((xr256 - 1) & 255) + 1)) >> 8);

    xl += 1;
    xr -= 1;
    if (xl < 0) xl = 0;
    if (xr >= (int)fb_width) xr = (int)fb_width - 1;
    if (xl > xr)
        return;

    row = (uint32_t*)((uint8_t*)fb_addr + (uint32_t)py * fb_pitch);

    if (alpha >= 255) {
        uint32_t packed = fb_pack_rgb(rgb);
        for (i = xl; i <= xr; i++)
            row[i] = packed;
    } else if (aero_fast_format()) {
        int a = alpha + (alpha >> 7);
        int ia = 256 - a;
        uint32_t sr = ((rgb >> 16) & 0xFFu) * (uint32_t)a;
        uint32_t sg = ((rgb >> 8)  & 0xFFu) * (uint32_t)a;
        uint32_t sb = (rgb & 0xFFu) * (uint32_t)a;
        uint32_t rp = fb_red_pos, gp = fb_green_pos, bp = fb_blue_pos;

        for (i = xl; i <= xr; i++) {
            uint32_t p = row[i];
            uint32_t r = ((((p >> rp) & 0xFFu) * (uint32_t)ia) + sr) >> 8;
            uint32_t g = ((((p >> gp) & 0xFFu) * (uint32_t)ia) + sg) >> 8;
            uint32_t b = ((((p >> bp) & 0xFFu) * (uint32_t)ia) + sb) >> 8;
            row[i] = (r << rp) | (g << gp) | (b << bp);
        }
    } else {
        for (i = xl; i <= xr; i++)
            aero_blend(i, py, rgb, alpha);
    }
}

/* Corner inset (1/256 px) for row j of a box h tall with corner radius r. */
static int aero_corner_inset(int r, int j, int h)
{
    int k;
    int yc;
    uint32_t v;

    if (r <= 0)
        return 0;

    if (j < r)
        k = j;
    else if (j >= h - r)
        k = h - 1 - j;
    else
        return 0;

    yc = 2 * (r - k) - 1;
    v = (uint32_t)(4 * r * r - yc * yc);
    return r * 256 - (int)aero_isqrt(v * 16384u);
}

/* Rounded box with vertical gradient c1 -> c2 and uniform alpha. */
static void aero_rbox(int x, int y, int w, int h, int r,
                      uint32_t c1, uint32_t c2, int alpha)
{
    int j;

    if (!fb_addr || w <= 0 || h <= 0)
        return;

    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r > 40) r = 40;
    if (r < 0) r = 0;

    for (j = 0; j < h; j++) {
        uint32_t rgb;
        int inset;

        if (y + j < 0 || y + j >= (int)fb_height)
            continue;

        rgb = (c1 == c2) ? c1 : aero_lerp(c1, c2, j, h > 1 ? h - 1 : 1);
        inset = aero_corner_inset(r, j, h);
        aero_span(y + j, x * 256 + inset, (x + w) * 256 - inset, rgb, alpha);
    }
}

void fb_draw_gradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
    aero_rbox(x, y, w, h, 0, top, bottom, 255);
}

void fb_draw_rgrad(int x, int y, int w, int h, int r, uint32_t top, uint32_t bottom)
{
    aero_rbox(x, y, w, h, r, top, bottom, 255);
}

void fb_draw_glass(int x, int y, int w, int h, int r, uint32_t color, int alpha)
{
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    aero_rbox(x, y, w, h, r, color, color, alpha);
}

/* Ellipse inside box (x,y,w,h), vertical gradient, uniform alpha. */
void fb_draw_ellipse(int x, int y, int w, int h, uint32_t c1, uint32_t c2, int alpha)
{
    int j;

    if (!fb_addr || w <= 0 || h <= 0)
        return;
    if (w > 4000 || h > 4000)
        return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    for (j = 0; j < h; j++) {
        int dy = 2 * j + 1 - h;
        uint32_t v = (uint32_t)(h * h - dy * dy);
        uint32_t s = aero_isqrt(v << 8) * 16u;        /* sqrt(v) * 256, no overflow for h <= 4000 */
        uint32_t half = (uint32_t)(((unsigned long long)w * s) / (unsigned long long)(2 * h)); /* 1/256 px */
        int cx256 = x * 256 + (w * 256) / 2;
        uint32_t rgb;

        if (y + j < 0 || y + j >= (int)fb_height)
            continue;

        rgb = (c1 == c2) ? c1 : aero_lerp(c1, c2, j, h > 1 ? h - 1 : 1);
        aero_span(y + j, cx256 - (int)half, cx256 + (int)half, rgb, alpha);
    }
}

/* Glossy sphere: radial shading, specular highlight, soft edge. */
void fb_draw_orb(int cx, int cy, int r, uint32_t color)
{
    int dx, dy;
    int r2;
    uint32_t light, dark, white;

    if (!fb_addr || r <= 0)
        return;

    r2    = r * r;
    white = 0xFFFFFF;
    light = aero_lerp(color, white, 150, 255);
    dark  = aero_lerp(color, 0x000000, 90, 255);

    for (dy = -r; dy <= r; dy++) {
        for (dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            int hx, hy, hd2, lim2;
            int a = 255;
            uint32_t rgb;

            if (d2 > r2 + r)
                continue;

            if (d2 > r2 - r) {
                a = ((r2 + r - d2) * 255) / (2 * r);
                if (a < 0) a = 0;
                if (a > 255) a = 255;
            }

            hx = dx + r / 3;
            hy = dy + r / 3;
            hd2 = hx * hx + hy * hy;
            lim2 = (r * r * 9) / 4;
            if (hd2 > lim2) hd2 = lim2;

            if (hd2 < lim2 / 2)
                rgb = aero_lerp(light, color, hd2, lim2 / 2);
            else
                rgb = aero_lerp(color, dark, hd2 - lim2 / 2, lim2 / 2);

            if (dy > r / 2) {
                int t = dy - r / 2;
                rgb = aero_lerp(rgb, light, t * t, (r / 2 + 1) * (r / 2 + 1) * 2);
            }

            aero_blend(cx + dx, cy + dy, rgb, a);
        }
    }

    {
        int gw = r * 3 / 2;
        int gh = r * 2 / 3;
        int gx = cx - gw / 2;
        int gy = cy - r + r / 6;
        int yy, xx;

        if (gw < 2 || gh < 2)
            return;

        for (yy = 0; yy < gh; yy++) {
            for (xx = 0; xx < gw; xx++) {
                int ex = xx * 2 - gw;
                int ey = yy * 2 - gh;
                int e2 = (ex * ex * 4096) / (gw * gw) + (ey * ey * 4096) / (gh * gh);
                int ga;

                if (e2 > 4096)
                    continue;

                ga = 150 - (yy * 150) / gh;
                aero_blend(gx + xx, gy + yy, white, ga * (4096 - e2) / 4096 + 20);
            }
        }
    }
}

/* Translucent soap bubble: clear body, bright rim, tiny highlights. */
void fb_draw_bubble(int cx, int cy, int r, int alpha)
{
    int dx, dy, r2;

    if (!fb_addr || r < 3)
        return;
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    r2 = r * r;

    for (dy = -r; dy <= r; dy++) {
        for (dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            int t, a, edge;
            uint32_t rgb = 0xFFFFFF;

            if (d2 > r2 + r)
                continue;

            t = (d2 * 256) / r2;
            if (t > 256) t = 256;

            /* body: nearly clear in the middle, denser near the rim */
            a = 10 + (t * t * 60) / 65536;

            /* bottom-right inner glow (aqua), top-left cooler tint */
            if (dx + dy > r / 2 && d2 > r2 / 3)
                { rgb = 0xC8F4FF; a += 28; }

            /* crisp rim */
            edge = r2 - d2;
            if (edge < r) {
                a += 90;
                if (edge < 0)
                    a = (a * (r + edge)) / r;
            }

            a = (a * alpha) / 255;
            aero_blend(cx + dx, cy + dy, rgb, a);
        }
    }

    /* specular dot */
    {
        int hr = r / 4;
        int hx = cx - r / 2;
        int hy = cy - r / 2;
        int yy, xx;

        if (hr < 1) hr = 1;
        for (yy = -hr; yy <= hr; yy++)
            for (xx = -hr; xx <= hr; xx++) {
                int e = xx * xx + yy * yy;
                if (e <= hr * hr)
                    aero_blend(hx + xx, hy + yy, 0xFFFFFF,
                               (((hr * hr - e) * 200) / (hr * hr + 1) + 40) * alpha / 255);
            }
    }
}
