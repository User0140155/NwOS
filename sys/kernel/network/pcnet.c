#include "pcnet.h"
#include "pci.h"
#include "timer.h"
#include "../memory/pmm.h"

#define RX_RING_LEN 8
#define TX_RING_LEN 4
#define RX_BUF_SIZE 1548
#define TX_BUF_SIZE 1548

typedef struct {
    volatile unsigned int addr;
    volatile unsigned int status; /* bits: BCNT(0-11, neg), ENP/STP flags in hi? simplified */
    volatile unsigned int length; /* BCNT */
    volatile unsigned int user;
} pcnet_rde_t; /* 16 bytes */

typedef struct {
    volatile unsigned int addr;
    volatile unsigned int status;
    volatile unsigned int length;
    volatile unsigned int user;
} pcnet_tde_t;

struct pcnet_init_block {
    volatile unsigned short mode;
    volatile unsigned char  rlen;  /* encoded: log2(rx) << 4 */
    volatile unsigned char  tlen;  /* encoded: log2(tx) << 4 */
    volatile unsigned char  padr[6];
    volatile unsigned short reserved;
    volatile unsigned char  ladr[8];
    volatile unsigned int  rdra;
    volatile unsigned int  tdra;
} __attribute__((packed, aligned(4)));

static unsigned short io;
static unsigned char mac[6];
static int ready;
static int link_up_state;

static struct pcnet_init_block *initblk;
static pcnet_rde_t *rx_ring;
static pcnet_tde_t *tx_ring;
static unsigned char *rx_bufs;
static unsigned char *tx_bufs;
static unsigned int rx_idx;
static unsigned int tx_idx;

static inline void outdw(unsigned short reg, unsigned int v) { pci_outl((unsigned short)(io + reg), v); }
static inline unsigned int indw(unsigned short reg) { return pci_inl((unsigned short)(io + reg)); }
static inline void outw(unsigned short reg, unsigned short v) { pci_outw((unsigned short)(io + reg), v); }
static inline unsigned short inw(unsigned short reg) { return pci_inw((unsigned short)(io + reg)); }

static void csr_write(unsigned int csr, unsigned int val)
{
    /* DWIO: RDP at +0x10, RAP at +0x14 */
    outdw(0x14, csr);
    outdw(0x10, val);
}

static unsigned int csr_read(unsigned int csr)
{
    outdw(0x14, csr);
    return indw(0x10);
}

static void bcr_write(unsigned int bcr, unsigned int val)
{
    outdw(0x14, bcr);
    outdw(0x1C, val);
}

static unsigned int bcr_read(unsigned int bcr)
{
    outdw(0x14, bcr);
    return indw(0x1C);
}

static void delay_ms(unsigned int ms)
{
    unsigned long long t = timer_ms();
    while (timer_ms() - t < ms) __asm__ volatile("pause");
}

int pcnet_probe(void)
{
    pci_dev_t dev;
    uint64_t phys = 0;

    if (ready) return 0;
    timer_init();

    if (!pci_find(0x1022, 0x2000, &dev) && !pci_find(0x1022, 0x2001, &dev))
        return -1;

    if (!pci_enable_io_busmaster(&dev))
        return -2;
    if (!pci_get_io_bar(&dev, 0, &io) || io == 0)
        return -3;
    (void)indw(0x18);
    delay_ms(5);
    bcr_write(20, 0x0002);

    for (int i = 0; i < 6; i++) mac[i] = pci_inb((unsigned short)(io + i));

    size_t total = sizeof(struct pcnet_init_block) + RX_RING_LEN * sizeof(pcnet_rde_t) +
                   TX_RING_LEN * sizeof(pcnet_tde_t) + RX_RING_LEN * RX_BUF_SIZE +
                   TX_RING_LEN * TX_BUF_SIZE + 256;
    void *mem = pmm_alloc_dma32(total, 16, &phys);
    if (!mem) return -4;
    unsigned char *p = (unsigned char *)mem;
    initblk = (struct pcnet_init_block *)p; p += sizeof(struct pcnet_init_block);
    rx_ring = (pcnet_rde_t *)p; p += RX_RING_LEN * sizeof(pcnet_rde_t);
    tx_ring = (pcnet_tde_t *)p; p += TX_RING_LEN * sizeof(pcnet_tde_t);
    p = (unsigned char *)(((uintptr_t)p + 15) & ~(uintptr_t)15);
    rx_bufs = p; p += RX_RING_LEN * RX_BUF_SIZE;
    tx_bufs = p;

    for (unsigned int i = 0; i < RX_RING_LEN; i++) {
        uint64_t b = (uint64_t)(uintptr_t)(rx_bufs + i * RX_BUF_SIZE);
        rx_ring[i].addr = (unsigned int)(b & 0xFFFFFFFFu);
        /* OWN=1 (bit31), BCNT is two's complement of size in low 12 bits, ONES in 12-15 */
        rx_ring[i].length = (unsigned int)(0xF000 | ((~(RX_BUF_SIZE - 1) + 1) & 0x0FFF));
        rx_ring[i].status = 0x80000000u; /* OWN */
        rx_ring[i].user = 0;
    }
    for (unsigned int i = 0; i < TX_RING_LEN; i++) {
        uint64_t b = (uint64_t)(uintptr_t)(tx_bufs + i * TX_BUF_SIZE);
        tx_ring[i].addr = (unsigned int)(b & 0xFFFFFFFFu);
        tx_ring[i].status = 0;
        tx_ring[i].length = 0;
        tx_ring[i].user = 0;
    }
    rx_idx = 0;
    tx_idx = 0;

    initblk->mode = 0x0000; /* promiscuous off, normal */
    /* rlen/tlen encoded: value k means 2^k entries, stored in bits 7:4 */
    {
        unsigned char rlog = 3, tlog = 2; /* 8,4 */
        initblk->rlen = (unsigned char)(rlog << 4);
        initblk->tlen = (unsigned char)(tlog << 4);
    }
    for (int i = 0; i < 6; i++) initblk->padr[i] = mac[i];
    for (int i = 0; i < 8; i++) initblk->ladr[i] = 0;
    initblk->rdra = (unsigned int)((uint64_t)(uintptr_t)rx_ring & 0xFFFFFFFFu);
    initblk->tdra = (unsigned int)((uint64_t)(uintptr_t)tx_ring & 0xFFFFFFFFu);

    unsigned int init_addr = (unsigned int)((uint64_t)(uintptr_t)initblk & 0xFFFFFFFFu);
    csr_write(1, init_addr & 0xFFFFu);
    csr_write(2, (init_addr >> 16) & 0xFFFFu);

    /* INIT */
    csr_write(0, 0x0001);
    for (int i = 0; i < 100000; i++) {
        if (csr_read(0) & 0x0100) break; /* IDON */
        __asm__ volatile("pause");
    }
    /* clear IDON, start */
    csr_write(0, 0x0102); /* IDON clear + STRT */

    ready = 1;
    link_up_state = 1; /* assume up; BCR4/LNKST can be checked for detail */
    return 0;
}

int pcnet_link_up(void)
{
    if (!ready) return 0;
    /* BCR4 bit 15 is LEDOUT in some modes, LNKST in others; keep simple */
    return link_up_state;
}

const unsigned char *pcnet_mac(void)
{
    return mac;
}

int pcnet_send(const void *frame, unsigned int len)
{
    if (!ready || !frame || len == 0) return -1;
    if (len > TX_BUF_SIZE) len = TX_BUF_SIZE;
    pcnet_tde_t *d = &tx_ring[tx_idx];
    if (d->status & 0x80000000u) return -2; /* still owned by controller */

    unsigned char *dst = tx_bufs + tx_idx * TX_BUF_SIZE;
    const unsigned char *src = (const unsigned char *)frame;
    for (unsigned int i = 0; i < len; i++) dst[i] = src[i];
    if (len < 60) {
        for (unsigned int i = len; i < 60; i++) dst[i] = 0;
        len = 60;
    }

    d->length = (unsigned int)(0xF000 | ((~(len - 1) + 1) & 0x0FFF));
    d->status = 0x8300F000u; /* OWN + STP + ENP, clear errs */
    /* trigger transmit demand */
    csr_write(0, 0x0008); /* TDMD */

    tx_idx = (tx_idx + 1) % TX_RING_LEN;
    return 0;
}

int pcnet_recv(void *buf, unsigned int max)
{
    if (!ready || !buf) return 0;
    pcnet_rde_t *d = &rx_ring[rx_idx];
    if (d->status & 0x80000000u) return 0; /* owned by controller, no packet */

    int ret = 0;
    if (!(d->status & 0x4000u)) { /* ERR bit */
        unsigned int mcnt = d->length & 0x0FFFu; /* message byte count in low 12 when received */
        if (mcnt >= 4) {
            unsigned int copy_len = mcnt - 4; /* CRC */
            if (copy_len > max) copy_len = max;
            unsigned char *src = rx_bufs + rx_idx * RX_BUF_SIZE;
            unsigned char *dst = (unsigned char *)buf;
            for (unsigned int i = 0; i < copy_len; i++) dst[i] = src[i];
            ret = (int)copy_len;
        }
    }

    d->length = (unsigned int)(0xF000 | ((~(RX_BUF_SIZE - 1) + 1) & 0x0FFF));
    d->status = 0x80000000u; /* give back */
    rx_idx = (rx_idx + 1) % RX_RING_LEN;
    return ret;
}

const nic_driver_t pcnet_driver = {
    .name = "AMD PCnet-PCI II",
    .probe = pcnet_probe,
    .link_up = pcnet_link_up,
    .mac = pcnet_mac,
    .send = pcnet_send,
    .recv = pcnet_recv,
};
