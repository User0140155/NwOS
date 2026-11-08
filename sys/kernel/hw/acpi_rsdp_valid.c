#include "nwos_internal.h"

void port_outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void port_outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
unsigned char port_inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
unsigned short port_inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

unsigned int acpi_u32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

unsigned long long acpi_u64(const unsigned char *p)
{
    return (unsigned long long)acpi_u32(p) |
           ((unsigned long long)acpi_u32(p + 4) << 32);
}

int acpi_checksum_valid(const unsigned char *p, unsigned int length)
{
    unsigned char sum = 0;
    if (!p || !length || length > 0x100000u)
        return 0;
    for (unsigned int i = 0; i < length; ++i)
        sum = (unsigned char)(sum + p[i]);
    return sum == 0;
}

int acpi_rsdp_valid(const unsigned char *rsdp, unsigned int size)
{
    if (!rsdp || size < 20u ||
        rsdp[0] != 'R' || rsdp[1] != 'S' || rsdp[2] != 'D' ||
        rsdp[3] != ' ' || rsdp[4] != 'P' || rsdp[5] != 'T' ||
        rsdp[6] != 'R' || rsdp[7] != ' ' ||
        !acpi_checksum_valid(rsdp, 20u))
        return 0;

    if (rsdp[15] >= 2u) {
        unsigned int extended_length;
        if (size < 36u)
            return 0;
        extended_length = acpi_u32(rsdp + 20u);
        if (extended_length < 36u || extended_length > size ||
            !acpi_checksum_valid(rsdp, extended_length))
            return 0;
    }
    return 1;
}

unsigned char *acpi_phys_ptr(unsigned long long address,
                                    unsigned int minimum_length)
{
    if (!address || address >= 0x100000000ull ||
        (unsigned long long)minimum_length > 0x100000000ull - address)
        return 0;

    return (unsigned char *)(unsigned long)address;
}
