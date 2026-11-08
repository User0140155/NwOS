#include "nwos_internal.h"

int acpi_find_s5(unsigned char *dsdt, unsigned int length,
                        unsigned char *sleep_a, unsigned char *sleep_b)
{
    for (unsigned int i = 2u; i + 7u < length; ++i) {
        unsigned int name_pos;
        unsigned int package_pos;
        unsigned int body_pos;
        unsigned int package_length;
        unsigned int package_end;
        unsigned int follow;
        unsigned char first;
        unsigned long long a, b;

        if (dsdt[i] != '_' || dsdt[i + 1u] != 'S' ||
            dsdt[i + 2u] != '5' || dsdt[i + 3u] != '_')
            continue;
        name_pos = i;
        if (name_pos && dsdt[name_pos - 1u] == 0x5Cu)
            --name_pos;
        if (!name_pos || dsdt[name_pos - 1u] != 0x08u)
            continue;
        package_pos = i + 4u;
        if (package_pos >= length || dsdt[package_pos] != 0x12u)
            continue;
        ++package_pos;
        if (package_pos >= length)
            continue;
        first = dsdt[package_pos];
        follow = (unsigned int)(first >> 6);
        if (follow > 3u || package_pos + 1u + follow > length)
            continue;
        if (!follow) {
            package_length = first & 0x3Fu;
            body_pos = package_pos + 1u;
        } else {
            package_length = first & 0x0Fu;
            for (unsigned int n = 0; n < follow; ++n)
                package_length |= (unsigned int)dsdt[package_pos + 1u + n]
                                  << (4u + 8u * n);
            body_pos = package_pos + 1u + follow;
        }
        package_end = package_pos + package_length;
        if (!package_length || package_end > length ||
            body_pos >= package_end)
            continue;
        if (dsdt[body_pos++] < 2u)
            continue;
        if (!acpi_aml_integer(dsdt, package_end, &body_pos, &a) ||
            !acpi_aml_integer(dsdt, package_end, &body_pos, &b))
            continue;
        *sleep_a = (unsigned char)(a & 7u);
        *sleep_b = (unsigned char)(b & 7u);
        return 1;
    }
    return 0;
}

unsigned short acpi_gas_read16(const unsigned char *gas)
{
    unsigned long long address = acpi_u64(gas + 4u);
    if (gas[2] != 0u || gas[1] < 16u)
        return 0;
    if (gas[0] == 1u && address <= 0xFFFFu)
        return port_inw((unsigned short)address);
    if (gas[0] == 0u && address < 0x100000000ull)
        return *(volatile unsigned short *)(unsigned long)address;
    return 0;
}

int acpi_gas_write16(const unsigned char *gas, unsigned short value)
{
    unsigned long long address = acpi_u64(gas + 4u);
    if (gas[2] != 0u || gas[1] < 16u)
        return 0;
    if (gas[0] == 1u && address <= 0xFFFFu) {
        port_outw((unsigned short)address, value);
        return 1;
    }
    if (gas[0] == 0u && address < 0x100000000ull) {
        *(volatile unsigned short *)(unsigned long)address = value;
        return 1;
    }
    return 0;
}

int acpi_gas_write8(const unsigned char *gas, unsigned char value)
{
    unsigned long long address = acpi_u64(gas + 4u);
    if (gas[2] != 0u || gas[1] < 8u)
        return 0;
    if (gas[0] == 1u && address <= 0xFFFFu) {
        port_outb((unsigned short)address, value);
        return 1;
    }
    if (gas[0] == 0u && address < 0x100000000ull) {
        *(volatile unsigned char *)(unsigned long)address = value;
        return 1;
    }
    return 0;
}
