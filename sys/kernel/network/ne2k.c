#include "ne2k.h"
#include "pci.h"
#include "timer.h"

#define NE2K_PSTART 0x46
#define NE2K_PSTOP  0x80
#define NE2K_TXP0   0x40
#define NE2K_TXP1   0x42
#define NE2K_TXPAGES 6

static unsigned short io;
static unsigned char mac[6];
static int ready;
static unsigned char next_pkt;

static inline void ne_outb(unsigned short reg, unsigned char v)
{
    pci_outb((unsigned short)(io + reg), v);
}

static inline unsigned char ne_inb(unsigned short reg)
{
    return pci_inb((unsigned short)(io + reg));
}

static inline void ne_outw(unsigned short reg, unsigned short v)
{
    pci_outw((unsigned short)(io + reg), v);
}

static inline unsigned short ne_inw(unsigned short reg)
{
    return pci_inw((unsigned short)(io + reg));
}

static void ne_reset(void)
{
    unsigned char v = ne_inb(0x1F);
    ne_outb(0x1F, v);
    for (volatile int i = 0; i < 10000; i++) __asm__ volatile("pause");
    /* wait for reset interrupt indication in ISR (bit 7) */
    for (int i = 0; i < 100000; i++) {
        if (ne_inb(0x07) & 0x80)
            break;
        __asm__ volatile("pause");
    }
    ne_outb(0x07, 0xFF);
}

static void ne_rdma_read(unsigned short addr, unsigned char *dst, unsigned short len)
{
    ne_outb(0x00, 0x22); /* page0, stop, remote DMA */
    ne_outb(0x0A, (unsigned char)(len & 0xFF));
    ne_outb(0x0B, (unsigned char)(len >> 8));
    ne_outb(0x08, (unsigned char)(addr & 0xFF));
    ne_outb(0x09, (unsigned char)(addr >> 8));
    ne_outb(0x00, 0x0A); /* remote read, start */
    for (unsigned short i = 0; i < len; i += 2) {
        unsigned short w = ne_inw(0x10);
        dst[i] = (unsigned char)(w & 0xFF);
        if (i + 1 < len) dst[i + 1] = (unsigned char)(w >> 8);
    }
}

static void ne_rdma_write(unsigned short addr, const unsigned char *src, unsigned short len)
{
    ne_outb(0x00, 0x22); /* page0, stop */
    ne_outb(0x0A, (unsigned char)(len & 0xFF));
    ne_outb(0x0B, (unsigned char)(len >> 8));
    ne_outb(0x08, (unsigned char)(addr & 0xFF));
    ne_outb(0x09, (unsigned char)(addr >> 8));
    ne_outb(0x00, 0x12); /* remote write, start */
    for (unsigned short i = 0; i < len; i += 2) {
        unsigned short w = src[i];
        if (i + 1 < len) w |= (unsigned short)src[i + 1] << 8;
        ne_outw(0x10, w);
    }
    for (int i = 0; i < 100000; i++) {
        if (ne_inb(0x07) & 0x40)
            break;
        __asm__ volatile("pause");
    }
    ne_outb(0x07, 0x40);
}

int ne2k_probe(void)
{
    pci_dev_t dev;

    if (ready) return 0;

    if (!pci_find(0x10EC, 0x8029, &dev))
        return -1;

    if (!pci_enable_io_busmaster(&dev))
        return -2;
    if (!pci_get_io_bar(&dev, 0, &io) || io == 0)
        return -3;

    ne_reset();

    /* stop, page0, abort remote DMA */
    ne_outb(0x00, 0x21);
    ne_outb(0x0E, 0x49); /* DCR: word, FIFO 8 bytes, normal */
    ne_outb(0x0A, 0x00);
    ne_outb(0x0B, 0x00);
    ne_outb(0x0C, 0x20); /* RCR: monitor (disable rx) for setup */
    ne_outb(0x0D, 0x02); /* TCR: loopback */
    ne_outb(0x01, NE2K_PSTART); /* PSTART */
    ne_outb(0x02, NE2K_PSTOP);  /* PSTOP */
    ne_outb(0x03, NE2K_PSTART); /* BNRY */
    ne_outb(0x04, 0x00);
    ne_outb(0x05, 0x00);
    ne_outb(0x07, 0xFF); /* clear ISR */
    ne_outb(0x0F, 0x00); /* IMR off */

    {
        unsigned char prom[32];
        ne_rdma_read(0x0000, prom, 32);
        for (int i = 0; i < 6; i++) mac[i] = prom[i * 2];
    }

    ne_outb(0x00, 0x61); /* page1, stop */
    for (int i = 0; i < 6; i++) ne_outb((unsigned short)(0x01 + i), mac[i]);
    for (int i = 0; i < 8; i++) ne_outb((unsigned short)(0x08 + i), 0xFF); /* multicast all for simplicity */
    ne_outb(0x07, NE2K_PSTART + 1); /* CURR */
    next_pkt = NE2K_PSTART + 1;

    ne_outb(0x00, 0x22); /* page0, stop */
    ne_outb(0x07, 0xFF);
    ne_outb(0x0C, 0x0E); /* RCR: AB, AM, pro? use AB+AM+promiscuous-ish? 0x1E accepts all; keep moderate */
    ne_outb(0x0C, 0x1F); /* accept broadcast/multicast/physical/promisc for robustness */
    ne_outb(0x0D, 0x00); /* TCR: normal */
    ne_outb(0x00, 0x22); /* start */
    ne_outb(0x00, 0x22);

    ready = 1;
    return 0;
}

int ne2k_link_up(void)
{
    return ready ? 1 : 0;
}

const unsigned char *ne2k_mac(void)
{
    return mac;
}

int ne2k_send(const void *frame, unsigned int len)
{
    if (!ready || len == 0 || len > 1514) return -1;
    if (len < 60) len = 60;

    /* use tx buffer at NE2K_TXP0 */
    ne_rdma_write((unsigned short)(NE2K_TXP0 << 8), (const unsigned char *)frame, (unsigned short)len);

    /* wait previous transmit complete (optional) */
    for (int i = 0; i < 100000; i++) {
        unsigned char cmd = ne_inb(0x00);
        if (!(cmd & 0x04)) break; /* TXP clear */
        __asm__ volatile("pause");
    }

    ne_outb(0x04, NE2K_TXP0); /* TPSR */
    ne_outb(0x05, (unsigned char)(len & 0xFF));
    ne_outb(0x06, (unsigned char)(len >> 8));
    ne_outb(0x00, 0x26); /* start + transmit */

    /* wait for TX complete or error */
    for (int i = 0; i < 100000; i++) {
        unsigned char isr = ne_inb(0x07);
        if (isr & 0x0A) { /* PTX or TXE */
            ne_outb(0x07, 0x0A);
            break;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

int ne2k_recv(void *buf, unsigned int max)
{
    if (!ready) return 0;

    unsigned char isr = ne_inb(0x07);
    if (isr & 0x01) ne_outb(0x07, 0x01); /* PRX ack */

    /* read CURR from page1 */
    ne_outb(0x00, 0x62);
    unsigned char curr = ne_inb(0x07);
    ne_outb(0x00, 0x22);

    if (next_pkt == curr)
        return 0;

    unsigned short pkt_addr = (unsigned short)(next_pkt << 8);
    unsigned char hdr[4];
    ne_rdma_read(pkt_addr, hdr, 4);

    unsigned char status = hdr[0];
    unsigned char next = hdr[1];
    unsigned short pkt_len = (unsigned short)hdr[2] | ((unsigned short)hdr[3] << 8);

    if (!(status & 0x01) || pkt_len < 4 || pkt_len > 1518 || next < NE2K_PSTART || next >= NE2K_PSTOP) {
        /* ring corrupted: reset BNRY/CURR */
        ne_outb(0x03, NE2K_PSTART);
        ne_outb(0x00, 0x62);
        ne_outb(0x07, NE2K_PSTART + 1);
        ne_outb(0x00, 0x22);
        next_pkt = NE2K_PSTART + 1;
        return 0;
    }

    unsigned short data_len = pkt_len - 4; /* includes CRC usually; strip later not critical */
    if (data_len > 4) data_len -= 4;
    if (data_len > max) data_len = (unsigned short)max;

    /* data starts at pkt_addr+4, may wrap ring */
    unsigned short start = pkt_addr + 4;
    unsigned short ring_bytes = (unsigned short)((NE2K_PSTOP - NE2K_PSTART) << 8);
    unsigned short ring_start = (unsigned short)(NE2K_PSTART << 8);
    unsigned char *dst = (unsigned char *)buf;

    for (unsigned short i = 0; i < data_len; i++) {
        unsigned short off = start + i;
        if (off >= (unsigned short)(NE2K_PSTOP << 8))
            off = (unsigned short)(ring_start + (off - (unsigned short)(NE2K_PSTOP << 8)));
        unsigned char b[2];
        ne_rdma_read(off & ~1u, b, 2);
        dst[i] = (off & 1u) ? b[1] : b[0];
    }

    next_pkt = next;
    unsigned char bnry = (next_pkt == NE2K_PSTART) ? (NE2K_PSTOP - 1) : (unsigned char)(next_pkt - 1);
    ne_outb(0x03, bnry);

    return (int)data_len;
}

const nic_driver_t ne2k_driver = {
    .name = "NE2000 PCI (RTL8029)",
    .probe = ne2k_probe,
    .link_up = ne2k_link_up,
    .mac = ne2k_mac,
    .send = ne2k_send,
    .recv = ne2k_recv,
};
