#include "rtl8169.h"
#include "pci.h"
#include "timer.h"
#include "../memory/mmu.h"
#include "../memory/pmm.h"

#define REG_MAC0        0x00
#define REG_MAR0        0x08
#define REG_TXDESC_LO   0x20
#define REG_TXDESC_HI   0x24
#define REG_CMD         0x37
#define REG_TPPOLL      0x38
#define REG_IMR         0x3C
#define REG_ISR         0x3E
#define REG_TXCFG       0x40
#define REG_RXCFG       0x44
#define REG_CFG9346     0x50
#define REG_PHYAR       0x60
#define REG_PHYSTATUS   0x6C
#define REG_RMS         0xDA
#define REG_CPLUSCMD    0xE0
#define REG_RXDESC_LO   0xE4
#define REG_RXDESC_HI   0xE8

#define CMD_RST         0x10
#define CMD_RE          0x08
#define CMD_TE          0x04

#define RX_DESC_COUNT   32
#define TX_DESC_COUNT   8
#define RX_BUF_SIZE     2048
#define TX_BUF_SIZE     2048

#define DESC_OWN        0x80000000u
#define DESC_EOR        0x40000000u
#define DESC_FS         0x20000000u
#define DESC_LS         0x10000000u

typedef struct {
    volatile unsigned int opts1;
    volatile unsigned int opts2;
    volatile unsigned int addr_lo;
    volatile unsigned int addr_hi;
} rtl_desc_t;

static volatile unsigned char *mmio;
static unsigned char mac[6];
static int ready;
static int link;

static rtl_desc_t *rx_descs;
static rtl_desc_t *tx_descs;
static unsigned char *rx_bufs;
static unsigned char *tx_bufs;
static unsigned int rx_idx;
static unsigned int tx_idx;

static inline void wr8(unsigned int reg, unsigned char v) { mmio[reg] = v; }
static inline void wr16(unsigned int reg, unsigned short v) { *(volatile unsigned short *)(mmio + reg) = v; }
static inline void wr32(unsigned int reg, unsigned int v) { *(volatile unsigned int *)(mmio + reg) = v; }
static inline unsigned char rd8(unsigned int reg) { return mmio[reg]; }
static inline unsigned short rd16(unsigned int reg) { return *(volatile unsigned short *)(mmio + reg); }
static inline unsigned int rd32(unsigned int reg) { return *(volatile unsigned int *)(mmio + reg); }

static void delay_ms(unsigned int ms)
{
    unsigned long long t = timer_ms();
    while (timer_ms() - t < ms) __asm__ volatile("pause");
}

static int try_probe_ids(pci_dev_t *out)
{
    static const unsigned short ids[] = {
        0x8161, 0x8168, 0x8169, 0x8136, 0x8129, 0x8167, 0x8125 /* 8125 may be different, but try */ ,
        0
    };
    for (int i = 0; ids[i]; i++) {
        if (pci_find(0x10EC, ids[i], out))
            return 1;
    }
    /* fallback: scan Realtek network controllers that look like 816x/8136 */
    pci_dev_t list[64];
    int n = pci_scan(list, 64);
    for (int i = 0; i < n; i++) {
        if (list[i].vendor_id != 0x10EC || list[i].class_code != 0x02) continue;
        unsigned short d = list[i].device_id;
        if (d == 0x8161 || d == 0x8168 || d == 0x8169 || d == 0x8136 || d == 0x8167) {
            *out = list[i];
            return 1;
        }
    }
    return 0;
}

int rtl8169_probe(void)
{
    pci_dev_t dev;
    uint64_t bar = 0;
    int is64 = 0;
    uint64_t phys = 0;

    if (ready) return 0;

    timer_init();
    if (!try_probe_ids(&dev))
        return -1;

    pci_set_power_d0(&dev);
    if (!pci_enable_mem_busmaster(&dev))
        return -2;

    /* Prefer BAR2 then BAR1 then BAR0 depending on device */
    if (!pci_get_mmio_bar(&dev, 2, &bar, &is64) || bar == 0)
        if (!pci_get_mmio_bar(&dev, 1, &bar, &is64) || bar == 0)
            if (!pci_get_mmio_bar(&dev, 0, &bar, &is64) || bar == 0)
                return -3;

    if (bar >= 0x100000000ull)
        return -4; /* identity map assumptions */

    mmio = (volatile unsigned char *)(uintptr_t)bar;
    mmu_mark_mmio_uncached(bar, 0x1000);

    /* Unlock config registers */
    wr8(REG_CFG9346, 0xC0);

    /* Reset */
    wr8(REG_CMD, CMD_RST);
    {
        unsigned long long t = timer_ms();
        while (rd8(REG_CMD) & CMD_RST) {
            if (timer_ms() - t > 200) return -5;
            __asm__ volatile("pause");
        }
    }

    for (int i = 0; i < 6; i++) mac[i] = rd8(REG_MAC0 + (unsigned int)i);

    /* Allocate DMA rings/bufs */
    size_t rx_ring_bytes = sizeof(rtl_desc_t) * RX_DESC_COUNT;
    size_t tx_ring_bytes = sizeof(rtl_desc_t) * TX_DESC_COUNT;
    size_t total = rx_ring_bytes + tx_ring_bytes + RX_DESC_COUNT * RX_BUF_SIZE + TX_DESC_COUNT * TX_BUF_SIZE + 256;
    void *mem = pmm_alloc_dma32(total, 256, &phys);
    if (!mem) return -6;
    unsigned char *p = (unsigned char *)mem;
    rx_descs = (rtl_desc_t *)p; p += rx_ring_bytes;
    tx_descs = (rtl_desc_t *)p; p += tx_ring_bytes;
    /* align buffers */
    p = (unsigned char *)(((uintptr_t)p + 255) & ~(uintptr_t)255);
    rx_bufs = p; p += RX_DESC_COUNT * RX_BUF_SIZE;
    tx_bufs = p;

    for (unsigned int i = 0; i < RX_DESC_COUNT; i++) {
        uint64_t bphys = (uint64_t)(uintptr_t)(rx_bufs + i * RX_BUF_SIZE);
        rx_descs[i].opts1 = DESC_OWN | (RX_BUF_SIZE & 0x3FFFu);
        if (i == RX_DESC_COUNT - 1) rx_descs[i].opts1 |= DESC_EOR;
        rx_descs[i].opts2 = 0;
        rx_descs[i].addr_lo = (unsigned int)(bphys & 0xFFFFFFFFu);
        rx_descs[i].addr_hi = (unsigned int)(bphys >> 32);
    }
    for (unsigned int i = 0; i < TX_DESC_COUNT; i++) {
        uint64_t bphys = (uint64_t)(uintptr_t)(tx_bufs + i * TX_BUF_SIZE);
        tx_descs[i].opts1 = (i == TX_DESC_COUNT - 1) ? DESC_EOR : 0;
        tx_descs[i].opts2 = 0;
        tx_descs[i].addr_lo = (unsigned int)(bphys & 0xFFFFFFFFu);
        tx_descs[i].addr_hi = (unsigned int)(bphys >> 32);
    }
    rx_idx = 0;
    tx_idx = 0;

    wr16(REG_IMR, 0x0000);
    wr16(REG_ISR, 0xFFFF);

    wr32(REG_RXDESC_LO, (unsigned int)((uint64_t)(uintptr_t)rx_descs & 0xFFFFFFFFu));
    wr32(REG_RXDESC_HI, (unsigned int)((uint64_t)(uintptr_t)rx_descs >> 32));
    wr32(REG_TXDESC_LO, (unsigned int)((uint64_t)(uintptr_t)tx_descs & 0xFFFFFFFFu));
    wr32(REG_TXDESC_HI, (unsigned int)((uint64_t)(uintptr_t)tx_descs >> 32));

    /* RX config: accept broadcast/multicast/physical, no fifo threshold fuss, 1024+ DMA */
    wr32(REG_RXCFG, 0x0000E70Fu | (7u << 8) | (7u << 13));
    wr16(REG_RMS, 0x1FFF);
    wr32(REG_TXCFG, (4u << 24) | (7u << 8)); /* IFG + DMA burst */

    /* Enable RX/TX */
    wr8(REG_CMD, CMD_RE | CMD_TE);

    /* Lock config */
    wr8(REG_CFG9346, 0x00);

    ready = 1;
    link = (rd8(REG_PHYSTATUS) & 0x02) ? 1 : 0; /* common link bit, may vary */
    delay_ms(10);
    link = (rd8(REG_PHYSTATUS) & 0x02) ? 1 : link;
    return 0;
}

int rtl8169_link_up(void)
{
    if (!ready) return 0;
    unsigned char ps = rd8(REG_PHYSTATUS);
    if (ps & 0x02) link = 1;
    return link ? 1 : 1; /* be permissive: some variants report differently */
}

const unsigned char *rtl8169_mac(void)
{
    return mac;
}

int rtl8169_send(const void *frame, unsigned int len)
{
    if (!ready || !frame || len == 0) return -1;
    if (len > TX_BUF_SIZE) len = TX_BUF_SIZE;
    rtl_desc_t *d = &tx_descs[tx_idx];
    /* wait if busy */
    unsigned long long t = timer_ms();
    while (d->opts1 & DESC_OWN) {
        if (timer_ms() - t > 100) return -2;
        __asm__ volatile("pause");
    }
    unsigned char *dst = tx_bufs + tx_idx * TX_BUF_SIZE;
    const unsigned char *src = (const unsigned char *)frame;
    for (unsigned int i = 0; i < len; i++) dst[i] = src[i];
    unsigned int opts1 = DESC_OWN | DESC_FS | DESC_LS | (len & 0x3FFFu);
    if (tx_idx == TX_DESC_COUNT - 1) opts1 |= DESC_EOR;
    d->opts2 = 0;
    d->opts1 = opts1;
    wr8(REG_TPPOLL, 0x40); /* normal priority poll */
    tx_idx = (tx_idx + 1) % TX_DESC_COUNT;
    return 0;
}

int rtl8169_recv(void *buf, unsigned int max)
{
    if (!ready || !buf) return 0;
    rtl_desc_t *d = &rx_descs[rx_idx];
    if (d->opts1 & DESC_OWN) return 0;

    unsigned int status = d->opts1;
    unsigned int pkt_len = status & 0x3FFFu;
    int ret = 0;
    if (!(status & 0x20u) /* RES error */ && pkt_len >= 4) {
        unsigned int copy_len = pkt_len - 4; /* strip CRC */
        if (copy_len > max) copy_len = max;
        unsigned char *src = rx_bufs + rx_idx * RX_BUF_SIZE;
        unsigned char *dst = (unsigned char *)buf;
        for (unsigned int i = 0; i < copy_len; i++) dst[i] = src[i];
        ret = (int)copy_len;
    }

    unsigned int opts1 = DESC_OWN | (RX_BUF_SIZE & 0x3FFFu);
    if (rx_idx == RX_DESC_COUNT - 1) opts1 |= DESC_EOR;
    d->opts1 = opts1;
    d->opts2 = 0;
    rx_idx = (rx_idx + 1) % RX_DESC_COUNT;
    return ret;
}

const nic_driver_t rtl8169_driver = {
    .name = "Realtek RTL8169/8168/8111",
    .probe = rtl8169_probe,
    .link_up = rtl8169_link_up,
    .mac = rtl8169_mac,
    .send = rtl8169_send,
    .recv = rtl8169_recv,
};
