#include "nwos_internal.h"

unsigned char rtc_read(unsigned char reg) {
    port_outb(0x70, reg);
    return port_inb(0x71);
}

unsigned char bcd_to_bin(unsigned char value) {
    return (value & 0x0F) + ((value / 16) * 10);
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

    if (acpi_shutdown_from_tables()) {
        for (volatile unsigned int delay = 0; delay < 2000000u; ++delay)
            __asm__ volatile ("pause");
    }

    port_outw(0x604, 0x2000);
    port_outw(0x604, 0x3400);
    port_outw(0xB004, 0x2000);
    port_outw(0x4004, 0x3400);

    while (1)
    {
        __asm__ volatile ("hlt");
    }
}
