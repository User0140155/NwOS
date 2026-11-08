#include "nwos_internal.h"

int acpi_shutdown_from_tables(void)
{
    unsigned char *fadt = acpi_find_fadt();
    unsigned int fadt_length;
    unsigned int flags;
    unsigned char *dsdt;
    unsigned long long dsdt_address;
    unsigned char sleep_a, sleep_b;

    if (!fadt)
        return 0;
    fadt_length = acpi_u32(fadt + 4u);
    if (fadt_length < 116u)
        return 0;

    dsdt_address = acpi_u32(fadt + 40u);
    if (fadt_length >= 148u && acpi_u64(fadt + 140u))
        dsdt_address = acpi_u64(fadt + 140u);
    dsdt = acpi_phys_ptr(dsdt_address, 36u);
    if (!dsdt)
        return 0;
    {
        unsigned int dsdt_length = acpi_u32(dsdt + 4u);
        if (dsdt_length < 36u || dsdt_length > 0x100000u ||
            !acpi_checksum_valid(dsdt, dsdt_length) ||
            !acpi_find_s5(dsdt, dsdt_length, &sleep_a, &sleep_b))
            return 0;
    }

    flags = acpi_u32(fadt + 112u);
    if (flags & (1u << 20)) {

        if (fadt_length >= 256u && acpi_u64(fadt + 244u)) {
            unsigned char value = (unsigned char)((sleep_a << 2) | (1u << 5));
            return acpi_gas_write8(fadt + 244u, value);
        }
        return 0;
    }

    {
        unsigned int pm1a_legacy = acpi_u32(fadt + 64u);
        unsigned int pm1b_legacy = acpi_u32(fadt + 68u);
        unsigned char *pm1a_gas = (fadt_length >= 184u) ? fadt + 172u : 0;
        unsigned char *pm1b_gas = (fadt_length >= 196u) ? fadt + 184u : 0;
        unsigned short current_a = 0;
        unsigned short current_b = 0;
        unsigned short value_a;
        unsigned short value_b;
        int use_a_gas = pm1a_gas && acpi_u64(pm1a_gas + 4u) != 0 &&
                        (pm1a_gas[0] <= 1u) && pm1a_gas[1] >= 16u &&
                        pm1a_gas[2] == 0u;
        int use_b_gas = pm1b_gas && acpi_u64(pm1b_gas + 4u) != 0 &&
                        (pm1b_gas[0] <= 1u) && pm1b_gas[1] >= 16u &&
                        pm1b_gas[2] == 0u;

        if (use_a_gas)
            current_a = acpi_gas_read16(pm1a_gas);
        else if (pm1a_legacy && pm1a_legacy <= 0xFFFFu)
            current_a = port_inw((unsigned short)pm1a_legacy);
        else
            return 0;

        if (!(current_a & 1u) && acpi_u32(fadt + 48u) <= 0xFFFFu &&
            acpi_u32(fadt + 48u) != 0u && fadt[52u] != 0u) {
            port_outb((unsigned short)acpi_u32(fadt + 48u), fadt[52u]);
            for (unsigned int wait = 0; wait < 1000000u; ++wait) {
                current_a = use_a_gas ? acpi_gas_read16(pm1a_gas) :
                    port_inw((unsigned short)pm1a_legacy);
                if (current_a & 1u)
                    break;
                __asm__ volatile ("pause");
            }
        }
        if (!(current_a & 1u))
            return 0;

        value_a = (unsigned short)((current_a & ~(7u << 10)) |
                                   ((unsigned short)sleep_a << 10) |
                                   (1u << 13));
        if (use_a_gas) {
            if (!acpi_gas_write16(pm1a_gas, value_a))
                return 0;
        } else {
            port_outw((unsigned short)pm1a_legacy, value_a);
        }

        if (use_b_gas) {
            current_b = acpi_gas_read16(pm1b_gas);
            value_b = (unsigned short)((current_b & ~(7u << 10)) |
                                       ((unsigned short)sleep_b << 10) |
                                       (1u << 13));
            (void)acpi_gas_write16(pm1b_gas, value_b);
        } else if (pm1b_legacy && pm1b_legacy <= 0xFFFFu) {
            current_b = port_inw((unsigned short)pm1b_legacy);
            value_b = (unsigned short)((current_b & ~(7u << 10)) |
                                       ((unsigned short)sleep_b << 10) |
                                       (1u << 13));
            port_outw((unsigned short)pm1b_legacy, value_b);
        }
    }
    return 1;
}
