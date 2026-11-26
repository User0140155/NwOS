#include "rtl8822.h"
#include "pci.h"
#include "../memory/mmu.h"
#include "timer.h"

static int probed = 0;
static int present = 0;
static int initialized = 0;
static const char *detected_name = 0;
static volatile unsigned char *mmio = 0;
static uint64_t mmio_base = 0;
static unsigned char mac[6];
static pci_dev_t found_dev;

struct rtl8822_id {
    unsigned short vendor;
    unsigned short device;
    const char *name;
};

static const struct rtl8822_id ids[] = {
    { 0x10EC, 0xB822, "Realtek RTL8822BE 802.11ac PCIe" },
    { 0x10EC, 0xC822, "Realtek RTL8822CE 802.11ac PCIe" },
    { 0x10EC, 0xB82B, "Realtek RTL8822BE (variant)" },
    { 0x10EC, 0xC82F, "Realtek RTL8822CE (variant)" },
    { 0x10EC, 0xC82A, "Realtek RTL8822CE (variant)" },
    { 0, 0, 0 }
};

static void rtl8822_try_read_mac(void)
{
    for (int i = 0; i < 6; i++) mac[i] = 0;
    if (!mmio) return;

    unsigned char m1[6], m2[6];
    for (int i = 0; i < 6; i++) {
        m1[i] = mmio[0x0100 + i];
        m2[i] = mmio[0x0000 + i];
    }
    int v1 = 0, v2 = 0;
    int z1 = 1, z2 = 1, ff1 = 1, ff2 = 1;
    for (int i = 0; i < 6; i++) {
        if (m1[i] != 0x00) z1 = 0;
        if (m2[i] != 0x00) z2 = 0;
        if (m1[i] != 0xFF) ff1 = 0;
        if (m2[i] != 0xFF) ff2 = 0;
    }
    v1 = (!z1 && !ff1 && (m1[0] & 0x01) == 0);
    v2 = (!z2 && !ff2 && (m2[0] & 0x01) == 0);
    if (v1) { for (int i = 0; i < 6; i++) mac[i] = m1[i]; }
    else if (v2) { for (int i = 0; i < 6; i++) mac[i] = m2[i]; }
}

int rtl8822_probe(void)
{
    pci_dev_t dev;
    uint64_t bar = 0;
    int is64 = 0;

    if (probed)
        return present ? (initialized ? 0 : -4) : -1;

    probed = 1;
    present = 0;
    initialized = 0;
    detected_name = 0;
    mmio = 0;
    mmio_base = 0;

    for (int i = 0; ids[i].name; i++) {
        if (pci_find(ids[i].vendor, ids[i].device, &dev)) {
            present = 1;
            detected_name = ids[i].name;
            found_dev = dev;
            break;
        }
    }

    if (!present) {
        pci_dev_t list[64];
        int n = pci_scan(list, 64);
        for (int i = 0; i < n; i++) {
            if (list[i].vendor_id != 0x10EC || list[i].class_code != 0x02) continue;
            unsigned short d = list[i].device_id;
            if (d == 0xB822 || d == 0xC822 ||
                (d & 0xFF00u) == 0xB800u || (d & 0xFF00u) == 0xC800u) {
                present = 1;
                detected_name = "Realtek RTL8822-series PCIe Wi-Fi";
                found_dev = list[i];
                break;
            }
        }
    }

    if (!present)
        return -1;

    /* Bring PCI function up */
    pci_set_power_d0(&found_dev);
    if (!pci_enable_mem_busmaster(&found_dev))
        return -2;

    if (pci_get_mmio_bar(&found_dev, 2, &bar, &is64) && bar != 0) {
        mmio_base = bar;
    } else if (pci_get_mmio_bar(&found_dev, 0, &bar, &is64) && bar != 0) {
        mmio_base = bar;
    }

    if (mmio_base != 0 && mmio_base < 0x100000000ull) {
        mmio = (volatile unsigned char *)(uintptr_t)mmio_base;
        mmu_mark_mmio_uncached(mmio_base, 0x10000);

        volatile unsigned int *mmio32 = (volatile unsigned int *)mmio;
        (void)mmio32[0]; /* read probe */

        for (volatile int i = 0; i < 10000; i++) __asm__ volatile("pause");

        rtl8822_try_read_mac();
        initialized = 1;
        return 0;
    }

    initialized = 0;
    return -3;
}

int rtl8822_present(void)
{
    if (!probed)
        (void)rtl8822_probe();
    return present;
}

int rtl8822_detected(void)
{
    return rtl8822_present();
}

int rtl8822_initialized(void)
{
    if (!probed) (void)rtl8822_probe();
    return initialized;
}

int rtl8822_link_up(void)
{
    return 0;
}

const unsigned char *rtl8822_mac(void)
{
    return mac;
}

int rtl8822_send(const void *frame, unsigned int len)
{
    (void)frame; (void)len;
    return -1; /* requires firmware + 802.11 MAC */
}

int rtl8822_recv(void *buf, unsigned int max)
{
    (void)buf; (void)max;
    return 0;
}

const char *rtl8822_detected_name(void)
{
    (void)rtl8822_present();
    return detected_name ? detected_name : "Realtek RTL8822 PCIe Wi-Fi";
}

const nic_driver_t rtl8822_driver = {
    .name = "Realtek RTL8822 Wi-Fi (PCI init)",
    .probe = rtl8822_probe,
    .link_up = rtl8822_link_up,
    .mac = rtl8822_mac,
    .send = rtl8822_send,
    .recv = rtl8822_recv,
};
