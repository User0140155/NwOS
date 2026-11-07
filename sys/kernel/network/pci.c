#include "pci.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if !defined(_MSC_VER) || defined(__clang__)
static inline uint32_t pci_io_inl(uint16_t port)
{
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void pci_io_outl(uint16_t port, uint32_t val)
{
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t pci_io_inw(uint16_t port)
{
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void pci_io_outw(uint16_t port, uint16_t val)
{
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
#endif
#endif

#ifndef TEST_MOCK_HARDWARE
#endif

static uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFCu);
    pci_io_outl(0xCF8u, address);
    return pci_io_inl(0xCFCu);
}

static void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFCu);
    pci_io_outl(0xCF8u, address);
    pci_io_outl(0xCFCu, value);
}

static uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFCu);
    pci_io_outl(0xCF8u, address);
    return (uint16_t)((pci_io_inl(0xCFCu) >> ((offset & 2u) * 8u)) & 0xFFFFu);
}

static void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFCu);
    pci_io_outl(0xCF8u, address);
    pci_io_outw((uint16_t)(0xCFCu + (offset & 2u)), value);
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    return pci_read_dword(bus, slot, func, offset);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value)
{
    pci_write_dword(bus, slot, func, offset, value);
}

static void pci_fill_device(pci_dev_t *out, uint8_t bus, uint8_t slot, uint8_t func,
                            uint32_t id, uint32_t class_dw)
{
    out->bus = bus;
    out->slot = slot;
    out->func = func;
    out->vendor_id = (uint16_t)(id & 0xFFFFu);
    out->device_id = (uint16_t)(id >> 16);
    out->class_code = (uint8_t)(class_dw >> 24);
    out->subclass = (uint8_t)((class_dw >> 16) & 0xFFu);
    out->prog_if = (uint8_t)((class_dw >> 8) & 0xFFu);
}

int pci_scan(pci_dev_t *list, int max_devices)
{
    int count = 0;

    if (!list || max_devices <= 0) return 0;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint32_t id = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFFu);
            if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

            uint32_t hdr_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x0C);
            uint8_t header_type = (uint8_t)((hdr_dw >> 16) & 0xFFu);
            int num_funcs = (header_type & 0x80u) ? 8 : 1;

            for (uint32_t func = 0; func < (uint32_t)num_funcs; ++func) {
                id = pci_read_dword((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                vendor = (uint16_t)(id & 0xFFFFu);
                if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

                uint32_t class_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot,
                                                   (uint8_t)func, 0x08);
                pci_fill_device(&list[count], (uint8_t)bus, (uint8_t)slot,
                                (uint8_t)func, id, class_dw);
                if (++count >= max_devices) return count;
            }
        }
    }

    return count;
}

int pci_find(uint16_t vendor_id, uint16_t device_id, pci_dev_t *out)
{
    if (!out) return 0;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint32_t id = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFFu);
            if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

            uint32_t hdr_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x0C);
            uint8_t header_type = (uint8_t)((hdr_dw >> 16) & 0xFFu);
            int num_funcs = (header_type & 0x80u) ? 8 : 1;

            for (uint32_t func = 0; func < (uint32_t)num_funcs; ++func) {
                id = pci_read_dword((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                if ((uint16_t)(id & 0xFFFFu) != vendor_id ||
                    (uint16_t)(id >> 16) != device_id)
                    continue;

                uint32_t class_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot,
                                                   (uint8_t)func, 0x08);
                pci_fill_device(out, (uint8_t)bus, (uint8_t)slot, (uint8_t)func,
                                id, class_dw);
                return 1;
            }
        }
    }

    return 0;
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if,
                   pci_dev_t *out)
{
    if (!out) return 0;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint32_t id = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFFu);
            if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

            uint32_t hdr_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x0C);
            uint8_t header_type = (uint8_t)((hdr_dw >> 16) & 0xFFu);
            int num_funcs = (header_type & 0x80u) ? 8 : 1;

            for (uint32_t func = 0; func < (uint32_t)num_funcs; ++func) {
                id = pci_read_dword((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                if ((uint16_t)(id & 0xFFFFu) == 0xFFFFu ||
                    (uint16_t)(id & 0xFFFFu) == 0x0000u)
                    continue;

                uint32_t class_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot,
                                                   (uint8_t)func, 0x08);
                uint8_t cc = (uint8_t)(class_dw >> 24);
                uint8_t sc = (uint8_t)((class_dw >> 16) & 0xFFu);
                uint8_t pi = (uint8_t)((class_dw >> 8) & 0xFFu);

                if (cc != class_code || sc != subclass ||
                    (prog_if != 0xFFu && pi != prog_if))
                    continue;

                pci_fill_device(out, (uint8_t)bus, (uint8_t)slot, (uint8_t)func,
                                id, class_dw);
                return 1;
            }
        }
    }

    return 0;
}

int pci_scan_xhci(pci_dev_t *controllers, int max_controllers)
{
    int count = 0;
    if (!controllers || max_controllers <= 0) return 0;

    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint32_t id = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFFu);
            if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

            uint32_t hdr_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot, 0, 0x0C);
            uint8_t header_type = (uint8_t)((hdr_dw >> 16) & 0xFFu);
            int num_funcs = (header_type & 0x80u) ? 8 : 1;

            for (uint32_t func = 0; func < (uint32_t)num_funcs; ++func) {
                id = pci_read_dword((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x00);
                vendor = (uint16_t)(id & 0xFFFFu);
                if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

                uint32_t class_dw = pci_read_dword((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x08);
                uint8_t class_code = (uint8_t)(class_dw >> 24);
                uint8_t subclass = (uint8_t)((class_dw >> 16) & 0xFFu);
                uint8_t prog_if = (uint8_t)((class_dw >> 8) & 0xFFu);

                /* Class 0x0C = Serial Bus, Subclass 0x03 = USB, Prog-IF 0x30 = xHCI */
                if (class_code == 0x0Cu && subclass == 0x03u && prog_if == 0x30u) {
                    controllers[count].bus = (uint8_t)bus;
                    controllers[count].slot = (uint8_t)slot;
                    controllers[count].func = (uint8_t)func;
                    controllers[count].vendor_id = vendor;
                    controllers[count].device_id = (uint16_t)(id >> 16);
                    controllers[count].class_code = class_code;
                    controllers[count].subclass = subclass;
                    controllers[count].prog_if = prog_if;
                    if (++count >= max_controllers) return count;
                }
            }
        }
    }
    return count;
}

int pci_set_power_d0(const pci_dev_t *dev)
{
    if (!dev) return 0;
    uint16_t status = pci_read_word(dev->bus, dev->slot, dev->func, 0x06);
    if (!(status & 0x10u)) return 1;

    uint8_t cap = (uint8_t)(pci_read_word(dev->bus, dev->slot, dev->func, 0x34) & 0xFCu);
    for (int i = 0; i < 64 && cap >= 0x40; ++i) {
        uint8_t id = (uint8_t)(pci_read_word(dev->bus, dev->slot, dev->func, cap) & 0xFFu);
        if (id == 0x01u) {
            uint16_t pmcsr = pci_read_word(dev->bus, dev->slot, dev->func, cap + 4u);
            if ((pmcsr & 0x3u) == 0u) return 1;
            pmcsr &= ~0x3u;
            pci_write_word(dev->bus, dev->slot, dev->func, cap + 4u, pmcsr);
            for (int j = 0; j < 200000; ++j) {
                pmcsr = pci_read_word(dev->bus, dev->slot, dev->func, cap + 4u);
                if ((pmcsr & 0x3u) == 0u) return 1;
                __asm__ volatile("pause");
            }
            return 0;
        }
        cap = (uint8_t)(pci_read_word(dev->bus, dev->slot, dev->func, cap + 1u) & 0xFCu);
    }
    return 1;
}

int pci_enable_mem_busmaster(const pci_dev_t *dev)
{
    if (!dev) return 0;
    uint16_t cmd = pci_read_word(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= (1u << 1) | (1u << 2);
    pci_write_word(dev->bus, dev->slot, dev->func, 0x04, cmd);
    uint16_t after = pci_read_word(dev->bus, dev->slot, dev->func, 0x04);
    return (after & 0x6u) == 0x6u;
}

int pci_get_mmio_bar(const pci_dev_t *dev, unsigned int bar_idx, uint64_t *bar_out, int *is_64_out)
{
    if (!dev || bar_idx >= 6u || !bar_out || !is_64_out) return 0;
    uint8_t offset = (uint8_t)(0x10u + bar_idx * 4u);
    uint32_t bar_low = pci_read_dword(dev->bus, dev->slot, dev->func, offset);

    if (bar_low & 1u) return 0;

    uint8_t bar_type = (uint8_t)((bar_low >> 1) & 3u);
    if (bar_type == 0x02u || bar_type == 0x03u) {
        if (bar_idx >= 5u) return 0;
        uint32_t bar_high = pci_read_dword(dev->bus, dev->slot, dev->func, offset + 4u);
        *bar_out = ((uint64_t)bar_high << 32) | (bar_low & ~0x0Fu);
        *is_64_out = 1;
        return 1;
    }
    if (bar_type == 0x00u || bar_type == 0x01u) {
        *bar_out = (uint64_t)(bar_low & ~0x0Fu);
        *is_64_out = 0;
        return 1;
    }
    return 0;
}