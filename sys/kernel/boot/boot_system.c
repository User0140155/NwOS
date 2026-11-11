#define NWOS_INCLUDE_LOGONWOS_H
#include "nwos_internal.h"
#include "audio/audio.h"

static int boot_text_width(const char* text);
static void boot_splash(void);

static int boot_text_width(const char* text)
{
    int length = 0;
    while (text[length] != '\0')
        ++length;
    return length * 8;
}

static void boot_splash(void)
{
    int width = (int)fb_get_width();
    int height = (int)fb_get_height();
    int logo_x = (width - LOGO_NWOS_WIDTH) / 2;
    int logo_y = (height - LOGO_NWOS_HEIGHT) / 2 - 48;
    int bar_width = width / 3;
    int bar_x;
    int bar_y;

    if (bar_width > 420)
        bar_width = 420;
    if (bar_width < 220)
        bar_width = 220;

    fb_clear(COLOR_BLUE); // 0x000A1118
    fb_draw_rect(logo_x - 30, logo_y - 30,
                 LOGO_NWOS_WIDTH + 60, LOGO_NWOS_HEIGHT + 60, COLOR_LIGHT_GRAY);
    fb_draw_frame(logo_x - 30, logo_y - 30,
                  LOGO_NWOS_WIDTH + 60, LOGO_NWOS_HEIGHT + 60, 0x003B5A6B);
    fb_draw_frame(logo_x - 25, logo_y - 25,
                  LOGO_NWOS_WIDTH + 50, LOGO_NWOS_HEIGHT + 50, 0x001C3340);

    fb_draw_image(logo_x, logo_y, LogoNwOS, LOGO_NWOS_WIDTH, LOGO_NWOS_HEIGHT);

    const char* title = OS_NAME " " OS_VERSION_STR;
    const char* subtitle = "Starting the system...";

    fb_draw_text((width - boot_text_width(title)) / 2,
                 logo_y + LOGO_NWOS_HEIGHT + 48, title, COLOR_WHITE);
    fb_draw_text((width - boot_text_width(subtitle)) / 2,
                 logo_y + LOGO_NWOS_HEIGHT + 70, subtitle, COLOR_LIGHT_GRAY);

    startup_sound_start();

    bar_x = (width - bar_width) / 2;
    bar_y = logo_y + LOGO_NWOS_HEIGHT + 108;

    fb_draw_rect(bar_x, bar_y, bar_width, 8, 0x00202B33);
    for (int i = 0; i <= 100; i += 4)
    {
        fb_draw_rect(bar_x, bar_y, (bar_width * i) / 100, 8, 0x003FA9C8);
        audio_service();
        sleep(15);
    }

    fb_draw_text((width - boot_text_width("Copyright (c) User0140155 2026")) / 2,
                 height - 40, "Copyright (c) User0140155 2026", 0x006F8794);
}

void boot_system(void)
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

    boot_splash();

    sleep(1200);
    audio_wait();

    clear();

    print("================================\n");
    print("        Welcome to " OS_NAME " " OS_VERSION_STR "\n");
    print("================================\n\n");
    set_color(TERM_COLOR_LIGHT_GREEN);
    print("copyright (c) User0140155 2026\n\n");
    set_color(TERM_COLOR_LIGHT_GRAY);
    print("Github: https://github.com/User0140155/NwOS"); print("\n");
    print("Alt + R to shutdown\n");
    print("Type 'help' for commands.\n\n");
    set_color(TERM_COLOR_GREEN);
    print("================");
    set_color(TERM_COLOR_BLUE);
    print("================\n");
    set_color(TERM_COLOR_LIGHT_GRAY);

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
