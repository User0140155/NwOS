#include "nwos_internal.h"

static int nwos_safe_load = 0;

void init_print(const char *str)
{
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print(str);
}

void pre_boot_init() {

    nw_entropy_init();
    print("[BOOT] Initializing keyboard (PS/2 probe)...\n");
    print_error("[WARN] PS/2 keyboard detection may take several seconds or minutes; please wait.\n");
    keyboard_init();

    print("[PS/2] Controller/channel responded during init: ");
    print_int(keyboard_ps2_init_responded());
    putchar_os('\n');
    print("[PS/2] Key scan data received: ");
    print_int(keyboard_ps2_seen());
    putchar_os('\n');
    print("[XHCI] ");
    print(xhci_status_string());
    putchar_os('\n');
    print("[XHCI] BIOS released semaphore: ");
    print_int(xhci_bios_released());
    putchar_os('\n');
    print("Loading boot menu..\n");
    sleep(150);
}

void startingNWOS() {
    clear();
    print("Cleared!\n");
    fb_init();
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("FB\n");
    random_init();
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("RANDOM\n");
    history_load();
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("HISTORY\n");
    ata_init();
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("ATA\n");
    fs_load();
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("FILESYSTEM\n");
    sleep(100);
    mouse_init();
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("KEYBOARD\n");
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("MOUSE\n");
    if (mouse_is_touchpad())
    {
        set_color(TERM_COLOR_GREEN);
        print("[OK] ");
        set_color(base_color);
        print("TOUCHPAD (");
        print(mouse_device_name());
        print(")\n");
    }
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("COMPILER\n");
    set_color(TERM_COLOR_GREEN);
    print("[OK] ");
    set_color(base_color);
    print("SYSTEM\n");
}
