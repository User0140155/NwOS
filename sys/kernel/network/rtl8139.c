#include "rtl8139.h"
#include "pci.h"
#include "timer.h"
#include "../memory/mmu.h"

#define REG_MAC0        0x00
#define REG_MAR0        0x08
#define REG_RBSTART     0x30
#define REG_CMD         0x37
#define REG_CAPR        0x38
#define REG_CBR         0x3A
#define REG_IMR         0x3C
#define REG_ISR         0x3E
#define REG_TCR         0x40
#define REG_RCR         0x44
#define REG_CONFIG1     0x52

#define CMD_RST         0x10
#define CMD_RE          0x08
#define CMD_TE          0x04
#define CMD_BUFE        0x01

#define RCR_AAP         (1u << 0)  /* accept all physical */
#define RCR_APM         (1u << 1)  /* accept physical match */
#define RCR_AM          (1u << 2)  /* accept multicast */
#define RCR_AB          (1u << 3)  /* accept broadcast */
#define RCR_WRAP        (1u << 7)
#define RCR_RBLEN_8K    (0u << 11)

#define RX_BUF_SIZE     8192
#define RX_RING_SIZE    (RX_BUF_SIZE + 16 + 1500) /* header slack + max packet */
#define TX_BUF_SIZE     1600
#define TX_SLOTS        4

static unsigned short io_base = 0;
static unsigned char mac[6];
static int ready = 0;

static unsigned char rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static unsigned char tx_bufs[TX_SLOTS][TX_BUF_SIZE] __attribute__((aligned(16)));
static unsigned int tx_slot = 0;
static unsigned int rx_offset = 0;

static inline void outb8(unsigned short reg, unsigned char v)
{
    pci_outb(io_base + reg, v);
}

static inline void outw16(unsigned short reg, unsigned short v)
{
    pci_outw(io_base + reg, v);
}

static inline void outl32(unsigned short reg, unsigned int v)
{
    pci_outl(io_base + reg, v);
}

static inline unsigned char inb8(unsigned short reg)
{
    return pci_inb(io_base + reg);
}

static inline unsigned short inw16(unsigned short reg)
{
    return pci_inw(io_base + reg);
}

static inline unsigned int inl32(unsigned short reg)
{
    return pci_inl(io_base + reg);
}

static void delay_ms(unsigned int ms)
{
    unsigned long long t = timer_ms();
    while (timer_ms() - t < ms)
        __asm__ volatile ("pause");
}

int rtl8139_probe(void)
{
    pci_dev_t dev;

    if (ready)
        return 0;

    timer_init();

    /* Realtek RTL8139: 10EC:8139 (also accept some common variants) */
    if (!pci_find(0x10EC, 0x8139, &dev) &&
        !pci_find(0x10EC, 0x8138, &dev))
    {
        return -1;
    }

    if (!pci_enable_io_busmaster(&dev))
        return -2;

    if (!pci_get_io_bar(&dev, 0, &io_base) || io_base == 0)
        return -3;

    /* Power on (CONFIG1 = 0) */
    outb8(REG_CONFIG1, 0x00);

    /* Software reset */
    outb8(REG_CMD, CMD_RST);
    unsigned long long t = timer_ms();
    while (inb8(REG_CMD) & CMD_RST)
    {
        if (timer_ms() - t > 100)
            return -4;
        __asm__ volatile ("pause");
    }

    /* Read MAC */
    for (int i = 0; i < 6; i++)
        mac[i] = inb8(REG_MAC0 + (unsigned short)i);

    if ((mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0)
        return -5;

    (void)mmu_mark_mmio_uncached;

    rx_offset = 0;
    tx_slot = 0;

    for (int i = 0; i < TX_SLOTS; i++)
    {
        for (unsigned int k = 0; k < TX_BUF_SIZE; k++)
            tx_bufs[i][k] = 0;
    }
    for (unsigned int k = 0; k < RX_RING_SIZE; k++)
        rx_ring[k] = 0;

    outl32(REG_RBSTART, (unsigned int)(unsigned long)rx_ring);

    /* No interrupts (polling driver) */
    outw16(REG_IMR, 0x0000);
    outw16(REG_ISR, 0xFFFF);

    /* RX config: accept broadcast + physical match, wrap, 8K buffer */
    outl32(REG_RCR, RCR_AB | RCR_APM | RCR_AM | RCR_WRAP | RCR_RBLEN_8K);

    /* TX config: standard DMA thresholds */
    outl32(REG_TCR, 0x03000700u);

    /* Enable RX/TX */
    outb8(REG_CMD, CMD_RE | CMD_TE);

    /* Reset CAPR (read pointer) to 0 - 16 per spec */
    outw16(REG_CAPR, 0xFFF0);

    ready = 1;
    delay_ms(10);
    return 0;
}

int rtl8139_link_up(void)
{
    /* Basic driver: assume up after init; some boards have no simple link bit exposed
       the same way. Packets will flow if cable/link is actually present. */
    return ready ? 1 : 0;
}

const unsigned char *rtl8139_mac(void)
{
    return mac;
}

int rtl8139_send(const void *frame, unsigned int len)
{
    if (!ready || io_base == 0 || len == 0 || len > 1514)
        return -1;

    unsigned int slot = tx_slot;
    unsigned short tsd_reg = (unsigned short)(0x10 + slot * 4);
    unsigned short tsad_reg = (unsigned short)(0x20 + slot * 4);

    /* Wait for ownership (TOK) if previously used, with timeout */
    unsigned long long t = timer_ms();
    while ((inl32(tsd_reg) & (1u << 13)) == 0 && (inl32(tsd_reg) & 0xFFFFu) != 0)
    {
        /* 1<<13 = TOK (transmit OK) appears after completion; first use value may be 0 */
        if (timer_ms() - t > 100)
            return -2;
        __asm__ volatile ("pause");
    }

    const unsigned char *src = (const unsigned char*)frame;
    for (unsigned int i = 0; i < len; i++)
        tx_bufs[slot][i] = src[i];

    unsigned int tx_len = len < 60 ? 60 : len;
    if (tx_len > TX_BUF_SIZE) tx_len = TX_BUF_SIZE;

    outl32(tsad_reg, (unsigned int)(unsigned long)tx_bufs[slot]);
    /* Clear status bits and set length (bits 0-12) to start transmission */
    outl32(tsd_reg, tx_len & 0x1FFFu);

    tx_slot = (slot + 1) % TX_SLOTS;
    return 0;
}

int rtl8139_recv(void *buf, unsigned int max)
{
    if (!ready || io_base == 0)
        return 0;

    /* CMD.BUFE == 1 means RX buffer empty */
    if (inb8(REG_CMD) & CMD_BUFE)
        return 0;

    unsigned int offset = rx_offset % RX_BUF_SIZE;
    /* packet header: [status:16][length:16] */
    unsigned short status = *(volatile unsigned short *)(rx_ring + offset);
    unsigned short pkt_len = *(volatile unsigned short *)(rx_ring + offset + 2);

    if ((status & 0x01) == 0 || pkt_len == 0 || pkt_len == 0xFFFF)
    {
        /* Reset RX pointers if desynced */
        rx_offset = 0;
        outw16(REG_CAPR, 0xFFF0);
        return 0;
    }

    /* pkt_len includes 4-byte CRC */
    unsigned int copy_len = pkt_len;
    if (copy_len > 4) copy_len -= 4; /* strip CRC */
    else copy_len = 0;

    if (copy_len > max) copy_len = max;

    unsigned char *dst = (unsigned char*)buf;
    unsigned int data_off = (offset + 4) % RX_RING_SIZE;
    for (unsigned int i = 0; i < copy_len; i++)
        dst[i] = rx_ring[(data_off + i) % RX_RING_SIZE];

    /* advance rx offset: header + pkt_len, dword aligned */
    rx_offset = (offset + pkt_len + 4 + 3) & ~3u;
    rx_offset %= RX_BUF_SIZE;

    /* CAPR is write-pointer-minus-16 */
    outw16(REG_CAPR, (unsigned short)(rx_offset - 16));

    return (int)copy_len;
}

const nic_driver_t rtl8139_driver = {
    .name = "Realtek RTL8139",
    .probe = rtl8139_probe,
    .link_up = rtl8139_link_up,
    .mac = rtl8139_mac,
    .send = rtl8139_send,
    .recv = rtl8139_recv,
};
