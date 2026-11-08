#include "nwos_internal.h"

unsigned char *acpi_find_fadt(void)
{
    unsigned char *mbi;
    unsigned int total_size;
    unsigned int offset;
    unsigned char *rsdp = 0;
    unsigned int rsdp_size = 0;
    unsigned char *rsdp_old = 0;
    unsigned int rsdp_old_size = 0;
    unsigned char *root;
    unsigned long long root_address = 0;

    if (!multiboot_info_ptr)
        return 0;
    mbi = (unsigned char *)(unsigned long)multiboot_info_ptr;
    total_size = acpi_u32(mbi);
    if (total_size < 16u || total_size > 16u * 1024u * 1024u)
        return 0;

    for (offset = 8u; offset + 8u <= total_size; ) {
        unsigned int type = acpi_u32(mbi + offset);
        unsigned int size = acpi_u32(mbi + offset + 4u);
        unsigned int data_size;
        if (size < 8u || size > total_size - offset)
            break;
        data_size = size - 8u;
        if (type == 15u && data_size >= 36u) {
            rsdp = mbi + offset + 8u;
            rsdp_size = data_size;
        } else if (type == 14u && data_size >= 20u) {
            rsdp_old = mbi + offset + 8u;
            rsdp_old_size = data_size;
        }
        offset += (size + 7u) & ~7u;
    }

    if (!acpi_rsdp_valid(rsdp, rsdp_size)) {
        rsdp = rsdp_old;
        rsdp_size = rsdp_old_size;
    }
    if (!acpi_rsdp_valid(rsdp, rsdp_size))
        return 0;

    if (rsdp[15] >= 2u && rsdp_size >= 36u) {
        unsigned int extended_length = acpi_u32(rsdp + 20u);
        if (extended_length >= 36u && extended_length <= rsdp_size &&
            acpi_checksum_valid(rsdp, extended_length))
            root_address = acpi_u64(rsdp + 24u);
    }
    if (root_address) {
        root = acpi_phys_ptr(root_address, 36u);
        if (root && root[0] == 'X' && root[1] == 'S' &&
            root[2] == 'D' && root[3] == 'T') {
            unsigned int length = acpi_u32(root + 4u);
            if (length >= 36u && length <= 0x100000u &&
                acpi_checksum_valid(root, length)) {
                for (unsigned int pos = 36u; pos + 8u <= length; pos += 8u) {
                    unsigned long long address = acpi_u64(root + pos);
                    unsigned char *table = acpi_phys_ptr(address, 36u);
                    if (table && table[0] == 'F' && table[1] == 'A' &&
                        table[2] == 'C' && table[3] == 'P') {
                        unsigned int table_length = acpi_u32(table + 4u);
                        if (table_length >= 116u && table_length <= 0x100000u &&
                            acpi_checksum_valid(table, table_length))
                            return table;
                    }
                }
            }
        }
    }

    root_address = acpi_u32(rsdp + 16u);
    root = acpi_phys_ptr(root_address, 36u);
    if (!root || root[0] != 'R' || root[1] != 'S' ||
        root[2] != 'D' || root[3] != 'T')
        return 0;
    {
        unsigned int length = acpi_u32(root + 4u);
        if (length < 36u || length > 0x100000u ||
            !acpi_checksum_valid(root, length))
            return 0;
        for (unsigned int pos = 36u; pos + 4u <= length; pos += 4u) {
            unsigned long long address = acpi_u32(root + pos);
            unsigned char *table = acpi_phys_ptr(address, 36u);
            if (table && table[0] == 'F' && table[1] == 'A' &&
                table[2] == 'C' && table[3] == 'P') {
                unsigned int table_length = acpi_u32(table + 4u);
                if (table_length >= 116u && table_length <= 0x100000u &&
                    acpi_checksum_valid(table, table_length))
                    return table;
            }
        }
    }
    return 0;
}

int acpi_aml_integer(const unsigned char *aml, unsigned int limit,
                            unsigned int *position, unsigned long long *value)
{
    unsigned char opcode;
    unsigned int pos = *position;
    if (pos >= limit)
        return 0;
    opcode = aml[pos++];
    if (opcode == 0x00u) {
        *value = 0;
    } else if (opcode == 0x01u) {
        *value = 1;
    } else if (opcode == 0xFFu) {
        *value = ~0ull;
    } else if (opcode == 0x0Au) {
        if (pos + 1u > limit) return 0;
        *value = aml[pos++];
    } else if (opcode == 0x0Bu) {
        if (pos + 2u > limit) return 0;
        *value = (unsigned long long)aml[pos] |
                 ((unsigned long long)aml[pos + 1u] << 8);
        pos += 2u;
    } else if (opcode == 0x0Cu) {
        if (pos + 4u > limit) return 0;
        *value = acpi_u32(aml + pos);
        pos += 4u;
    } else if (opcode == 0x0Eu) {
        if (pos + 8u > limit) return 0;
        *value = acpi_u64(aml + pos);
        pos += 8u;
    } else {
        return 0;
    }
    *position = pos;
    return 1;
}
