#include "nwos_internal.h"

void keyboard_input_test(void)
{
    unsigned long long end = timer_ms() + 2500;
    unsigned long long next_dot = 0;
    int received = 0;

    print("Testing keyboard availablity\nIf keyboard not working check PS/2 or xhci\n");

    while (timer_ms() < end)
    {
        int key = keyboard_pollkey();
        unsigned long long now = timer_ms();

        if (key != KEY_NONE)
        {
            print("\nKEY = ");
            print_int(key);

            if (key >= 32 && key <= 126)
            {
                print(" ('");
                putchar_os((char)key);
                print("')");
            }

            received = 1;
        }

        if (now >= next_dot)
        {
            putchar_os('.');
            next_dot = now + 500;
        }

        __asm__ volatile ("pause");
    }

    if (received)
        print("\n[INPUT] Keyboard events received!\n");
    else
        print("\n[INPUT] No keyboard events from the first input path.\n");

    if (!received && !xhci_keyboard_ready())
    {
        print("\n[USB] No key events yet; trying native xHCI fallback...\n");
        (void)xhci_init();
        print("[XHCI] Status: ");
        print(xhci_status_string());
        putchar_os('\n');
        print("[XHCI] Keyboard ready: ");
        print_int(xhci_keyboard_ready());
        putchar_os('\n');

        if (xhci_keyboard_ready())
        {
            unsigned long long usb_end = timer_ms() + 5000;
            unsigned long long usb_next_dot = 0;
            int usb_received = 0;
            print("[TEST] Press any key for 5 seconds (USB xHCI): ");
            while (timer_ms() < usb_end)
            {
                int key = keyboard_pollkey();
                unsigned long long now = timer_ms();
                if (key != KEY_NONE)
                {
                    print("\nKEY = ");
                    print_int(key);
                    if (key >= 32 && key <= 126)
                    {
                        print(" ('");
                        putchar_os((char)key);
                        print("')");
                    }
                    usb_received = 1;
                }
                if (now >= usb_next_dot)
                {
                    putchar_os('.');
                    usb_next_dot = now + 500;
                }
                __asm__ volatile ("pause");
            }
            if (usb_received)
            {
                received = 1;
                print("\n[INPUT] USB keyboard events received!\n");
            }
            else
                print("\n[INPUT] xHCI is ready, but no USB key reports arrived.\n");
        }
    }

    print("\n\n========== INPUT DEBUG ==========\n");

    print("[XHCI] Status: ");
    print(xhci_status_string());
    putchar_os('\n');

    print("[XHCI] Keyboard ready: ");
    print_int(xhci_keyboard_ready());
    putchar_os('\n');

    print("[PS2] Controller/channel responded at init: ");
    print_int(keyboard_ps2_init_responded());
    putchar_os('\n');
    print("[PS2] Key scan data received: ");
    print_int(keyboard_ps2_seen());
    putchar_os('\n');

    print("================================\n");

    print_error("If keyboard isnt working, check if connected\nOr check PS/2 / xhci availablity\n");

    print("Getting ready to start NwOS...\n");

    sleep(2500);

    print("Loading installer..\n");
    print("STATUS: ");
    print_success("[GOOD]\n");
    sleep(300);
    print("Warning: If your keyboard isnt working then:\n 1.Turn off PC\n 2.Launch OS inside of the VM\n (If you still want real-pc launch, wait until update of drivers)\n");
    sleep(600);
    print("Details\nNwOS Version: " OS_VERSION_STR "\nMade by: User0140155 (Old: User014015)\ncopyright (c) User0140155 2026\n");
    print("Getting ready to launch boot menu...\n");
    sleep(1200);
}
