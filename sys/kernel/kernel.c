// NwOS - Operating System
// Copyright (C) 2026 User0140155

// This program is free software: you can redistribute it
// and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation,
// version 2 only.

// See the LICENSE file for details.


#include "nwos_internal.h"

void kernel_main(void) {
    fb_init();
    print("[BOOT] fb_init() returned.\n");
    fb_set_mode(NWOS_BOOT_WIDTH, NWOS_BOOT_HEIGHT);

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

    print("[BOOT] Initializing physical memory manager...\n");
    int pmm_ready_for_modules = pmm_init();
    if (!pmm_ready_for_modules) {
        print_error(
            "PMM initialization failed; USB DMA unavailable.\n"
        );
    } else {
        print("[BOOT] PMM initialized.\n");
    }

    if (pmm_ready_for_modules &&
        nw_word_module_init_from_multiboot2(multiboot_info_ptr))
    {
        print_success("[BOOT] NwWord dictionary validated: ");
        print_uint64(nw_word_module_count());
        print(" words.\n");
    }
    else
    {
        print_error("[BOOT] NwWord dictionary unavailable: ");
        if (pmm_ready_for_modules)
            print(nw_word_module_error());
        else
            print("PMM is not ready");
        print(". Using the built-in fallback dictionary.\n");
    }

    if (pmm_ready_for_modules &&
        nw_passage_module_init_from_multiboot2(multiboot_info_ptr))
    {
        print_success("[BOOT] NwPassage text loaded: ");
        print_uint64(nw_passage_module_count());
        print(" verses. Shift+F7 is ready.\n");
    }
    else
    {
        print("[BOOT] NwPassage text is not installed; Shift+F7 will show setup instructions.\n");
    }

    pre_boot_init();

    keyboard_input_test();

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
            installer_completed = 0;
            installer();

            if (installer_completed)
            {
                int installed_status = fs_load_status();
                if (installed_status == 2)
                    dead_boot();

                has_installed = (installed_status == 1);
                if (has_installed)
                {
                    print_success("\nInstallation verified. NwOS is now available in this boot menu.\n");
                }
                else
                {
                    print_error("\nInstaller returned success, but disk scan did not find NwOS.\n");
                    print_error("The new partition may not have been written or may not be readable.\n");
                }
                sleep(1500);
            }
            else if (installer_cancelled)
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
