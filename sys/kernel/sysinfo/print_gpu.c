#include "nwos_internal.h"

static void nwf_outl(unsigned short port, unsigned int v);
static unsigned int nwf_inl(unsigned short port);
static unsigned int nwf_pci_read(int bus, int slot, int func, int off);
static int nwf_find_gpu(unsigned short* ven, unsigned short* dev);
static void nwf_print_hex16(unsigned short v);

unsigned int nwf_ram_mb(void)
{
    if (pmm_is_ready()) {
        uint64_t bytes = pmm_total_bytes();
        if (bytes >= 1024ULL * 1024ULL)
            return (unsigned int)(bytes / (1024ULL * 1024ULL));
    }

    unsigned int above16 = (unsigned int)rtc_read(0x34) |
                           ((unsigned int)rtc_read(0x35) << 8);

    if (above16)
        return 16 + above16 / 16;

    unsigned int ext = (unsigned int)rtc_read(0x30) |
                       ((unsigned int)rtc_read(0x31) << 8);

    return ext / 1024 + 1;
}

static void nwf_outl(unsigned short port, unsigned int v)
{
    __asm__ volatile ("outl %0, %1" : : "a"(v), "Nd"(port));
}

static unsigned int nwf_inl(unsigned short port)
{
    unsigned int r;
    __asm__ volatile ("inl %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static unsigned int nwf_pci_read(int bus, int slot, int func, int off)
{
    nwf_outl(0xCF8, 0x80000000u |
                    ((unsigned int)bus  << 16) |
                    ((unsigned int)slot << 11) |
                    ((unsigned int)func << 8)  |
                    (unsigned int)(off & 0xFC));
    return nwf_inl(0xCFC);
}

static int nwf_find_gpu(unsigned short* ven, unsigned short* dev)
{
    for (int bus = 0; bus < 256; bus++)
    {
        for (int slot = 0; slot < 32; slot++)
        {
            unsigned int id = nwf_pci_read(bus, slot, 0, 0x00);
            if ((id & 0xFFFF) == 0xFFFF) continue;

            unsigned int hdr = (nwf_pci_read(bus, slot, 0, 0x0C) >> 16) & 0xFF;
            int funcs = (hdr & 0x80) ? 8 : 1;

            for (int func = 0; func < funcs; func++)
            {
                id = nwf_pci_read(bus, slot, func, 0x00);
                if ((id & 0xFFFF) == 0xFFFF) continue;

                unsigned int cls = nwf_pci_read(bus, slot, func, 0x08) >> 24;
                if (cls == 0x03)
                {
                    *ven = (unsigned short)(id & 0xFFFF);
                    *dev = (unsigned short)(id >> 16);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void nwf_print_hex16(unsigned short v)
{
    print_hex_byte((unsigned char)(v >> 8));
    print_hex_byte((unsigned char)(v & 0xFF));
}

void nwf_print_gpu(void)
{
    unsigned short ven, dev;

    if (!nwf_find_gpu(&ven, &dev))
    {
        print("not detected");
        return;
    }

    if (ven == 0x1234 && dev == 0x1111) { print("QEMU Standard VGA"); return; }
    if (ven == 0x1B36 && dev == 0x0100) { print("QEMU QXL");          return; }
    if (ven == 0x1AF4 && dev == 0x1050) { print("Virtio GPU");        return; }
    if (ven == 0x15AD && dev == 0x0405) { print("VMware SVGA II");    return; }
    if (ven == 0x80EE && dev == 0xBEEF) { print("VirtualBox Graphics"); return; }

    switch (ven)
    {
        case 0x8086: print("Intel");  break;
        case 0x10DE: print("NVIDIA"); break;
        case 0x1002: print("AMD");    break;
        default:     print("Unknown"); break;
    }

    print(" GPU [");
    nwf_print_hex16(ven);
    putchar_os(':');
    nwf_print_hex16(dev);
    putchar_os(']');
}
