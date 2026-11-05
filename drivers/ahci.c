#include "ahci.h"
#include "../sys/kernel/network/pci.h"

#define AHCI_MAX_DRIVES 4

#define HBA_CAP        0x00
#define HBA_GHC        0x04
#define HBA_IS         0x08
#define HBA_PI         0x0C
#define HBA_VS         0x10

#define GHC_HR         (1u << 0)
#define GHC_IE         (1u << 1)
#define GHC_AE         (1u << 31)

#define HBA_PORT_BASE  0x100
#define HBA_PORT_SIZE  0x80

#define PORT_CLB       0x00
#define PORT_CLBU      0x04
#define PORT_FB        0x08
#define PORT_FBU       0x0C
#define PORT_IS        0x10
#define PORT_CMD       0x18
#define PORT_TFD       0x20
#define PORT_SIG       0x24
#define PORT_SSTS      0x28
#define PORT_SCTL      0x2C
#define PORT_SERR      0x30
#define PORT_SACT      0x34
#define PORT_CI        0x38

#define CMD_ST         (1u << 0)
#define CMD_SUD        (1u << 1)
#define CMD_POD        (1u << 2)
#define CMD_FRE        (1u << 4)
#define CMD_FR         (1u << 14)
#define CMD_CR         (1u << 15)

#define TFD_BSY        (1u << 7)
#define TFD_DRQ        (1u << 3)
#define TFD_ERR        (1u << 0)

#define IS_TFES        (1u << 30)
#define IS_HBDS        (1u << 29)
#define IS_IF          (1u << 27)

#define AHCI_SIG_SATA  0x00000101u

#define ATA_CMD_READ      0x25
#define ATA_CMD_WRITE     0x35
#define ATA_CMD_FLUSH     0xEA
#define ATA_CMD_IDENTIFY  0xEC

static ahci_drive_t drives[AHCI_MAX_DRIVES];
static volatile unsigned char* abars[AHCI_MAX_DRIVES];
static int info_count = 0;

static unsigned char clb[AHCI_MAX_DRIVES][1024] __attribute__((aligned(1024)));
static unsigned char fis[AHCI_MAX_DRIVES][256] __attribute__((aligned(256)));
static unsigned char ct[AHCI_MAX_DRIVES][256] __attribute__((aligned(128)));
static unsigned short idbuf[AHCI_MAX_DRIVES][256] __attribute__((aligned(512)));
static unsigned char bounce[512] __attribute__((aligned(512)));

static void memzero(void* dst, unsigned int n)
{
    unsigned char* d = (unsigned char*)dst;

    for (unsigned int i = 0; i < n; i++)
        d[i] = 0;
}

static void memcopy(void* dst, const void* src, unsigned int n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;

    for (unsigned int i = 0; i < n; i++)
        d[i] = s[i];
}

static void cpu_relax(void)
{
    __asm__ volatile ("pause" ::: "memory");
}

static unsigned int reg_read(volatile unsigned char* base, unsigned int off)
{
    return *(volatile unsigned int*)(base + off);
}

static void reg_write(volatile unsigned char* base, unsigned int off, unsigned int value)
{
    *(volatile unsigned int*)(base + off) = value;
}

static void barrier(void)
{
    __asm__ volatile ("mfence" ::: "memory");
}

static volatile unsigned char* port_base(volatile unsigned char* abar, unsigned int port)
{
    return abar + HBA_PORT_BASE + port * HBA_PORT_SIZE;
}

static int hba_wait(volatile unsigned char* abar, unsigned int mask,
                    unsigned int value, unsigned int spins)
{
    for (unsigned int i = 0; i < spins; i++) {
        if ((reg_read(abar, HBA_GHC) & mask) == value)
            return 0;
        cpu_relax();
    }
    return -1;
}

static int hba_reset(volatile unsigned char* abar)
{
    unsigned int ghc = reg_read(abar, HBA_GHC);

    if (ghc & GHC_AE)
        return 0;

    reg_write(abar, HBA_GHC, ghc | GHC_AE);

    if (hba_wait(abar, GHC_HR, 0, 10000000u) != 0)
        return -1;

    ghc = reg_read(abar, HBA_GHC);
    reg_write(abar, HBA_GHC, (ghc & ~GHC_IE) | GHC_AE | GHC_HR);

    if (hba_wait(abar, GHC_HR, 0, 50000000u) != 0)
        return -1;

    reg_write(abar, HBA_GHC, reg_read(abar, HBA_GHC) | GHC_AE);
    return 0;
}

static int port_wait_det(volatile unsigned char* p, unsigned int det,
                         unsigned int spins)
{
    for (unsigned int i = 0; i < spins; i++) {
        if ((reg_read(p, PORT_SSTS) & 0x0Fu) == det)
            return 0;
        cpu_relax();
    }
    return -1;
}

static int port_wait_idle(volatile unsigned char* p, unsigned int spins)
{
    for (unsigned int i = 0; i < spins; i++) {
        if (!(reg_read(p, PORT_TFD) & (TFD_BSY | TFD_DRQ)))
            return 0;
        cpu_relax();
    }
    return -1;
}

static int port_link_up(volatile unsigned char* p)
{
    unsigned int det;

    for (unsigned int i = 0; i < 2000000u; i++) {
        det = reg_read(p, PORT_SSTS) & 0x0Fu;
        if (det == 3u)
            return 0;
        if (det == 1u)
            break;
        cpu_relax();
    }

    det = reg_read(p, PORT_SSTS) & 0x0Fu;
    if (det == 0u)
        return -1;

    if (det == 1u) {
        unsigned int sctl = reg_read(p, PORT_SCTL);
        reg_write(p, PORT_SCTL, (sctl & ~0x0Fu) | 1u);
        for (volatile unsigned int d = 0; d < 100000u; ++d)
            cpu_relax();
        reg_write(p, PORT_SCTL, sctl & ~0x0Fu);
    }

    return port_wait_det(p, 3u, 10000000u);
}

static void port_stop(volatile unsigned char* p)
{
    unsigned int cmd = reg_read(p, PORT_CMD);

    cmd &= ~CMD_ST;
    reg_write(p, PORT_CMD, cmd);

    for (unsigned int i = 0; i < 1000000u && (reg_read(p, PORT_CMD) & CMD_CR); i++)
        cpu_relax();

    cmd &= ~CMD_FRE;
    reg_write(p, PORT_CMD, cmd);

    for (unsigned int i = 0; i < 1000000u && (reg_read(p, PORT_CMD) & CMD_FR); i++)
        cpu_relax();
}

static int ahci_command(int idx, unsigned char command, int write,
                        unsigned int lba, unsigned int count,
                        void* buffer, unsigned int bytes)
{
    volatile unsigned char* p = port_base(abars[idx], (unsigned int)drives[idx].port);
    unsigned int* hdr = (unsigned int*)clb[idx];
    unsigned char* f = ct[idx];

    memzero(clb[idx], sizeof(clb[idx]));
    memzero(ct[idx], sizeof(ct[idx]));

    f[0] = 0x27;
    f[1] = 0x80;
    f[2] = command;
    f[3] = 0;
    f[4] = (unsigned char)lba;
    f[5] = (unsigned char)(lba >> 8);
    f[6] = (unsigned char)(lba >> 16);
    f[7] = (command == ATA_CMD_IDENTIFY || command == ATA_CMD_FLUSH) ? 0 : 0x40;
    f[8] = (unsigned char)(lba >> 24);
    f[9] = 0;
    f[10] = 0;
    f[11] = 0;
    f[12] = (unsigned char)count;
    f[13] = (unsigned char)(count >> 8);
    f[14] = 0;
    f[15] = 0;

    unsigned int entries = 0;

    if (buffer && bytes) {
        unsigned long long addr = (unsigned long long)(unsigned long)buffer;
        unsigned int* prdt = (unsigned int*)(ct[idx] + 0x80);

        prdt[0] = (unsigned int)addr;
        prdt[1] = (unsigned int)(addr >> 32);
        prdt[2] = 0;
        prdt[3] = (bytes - 1u) | (1u << 31);
        entries = 1;
    }

    hdr[0] = 5u | (write ? (1u << 6) : 0u) | (entries << 16);
    hdr[1] = 0;
    hdr[2] = (unsigned int)(unsigned long)ct[idx];
    hdr[3] = (unsigned int)((unsigned long long)(unsigned long)ct[idx] >> 32);
    hdr[4] = 0;
    hdr[5] = 0;
    hdr[6] = 0;
    hdr[7] = 0;

    reg_write(p, PORT_IS, 0xFFFFFFFFu);
    barrier();
    reg_write(p, PORT_CI, 1u);
    barrier();

    for (unsigned int spin = 0; spin < 50000000u; spin++) {
        unsigned int is = reg_read(p, PORT_IS);

        if (is & (IS_TFES | IS_HBDS | IS_IF))
            return -1;

        if (!(reg_read(p, PORT_CI) & 1u)) {
            if (reg_read(p, PORT_TFD) & TFD_ERR)
                return -1;
            return 0;
        }

        cpu_relax();
    }

    return -2;
}

static void read_model(char* dst, const unsigned short* id)
{
    for (int i = 0; i < 20; i++) {
        dst[i * 2] = (char)(id[27 + i] >> 8);
        dst[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }

    dst[40] = '\0';

    for (int i = 39; i >= 0 && dst[i] == ' '; i--)
        dst[i] = '\0';
}

static int port_init(volatile unsigned char* abar, unsigned int port, int idx)
{
    volatile unsigned char* p = port_base(abar, port);

    if (port_link_up(p) != 0)
        return -1;

    unsigned int sig = reg_read(p, PORT_SIG);
    if (sig != AHCI_SIG_SATA) {
        unsigned int sctl = reg_read(p, PORT_SCTL);
        reg_write(p, PORT_SCTL, (sctl & ~0x0Fu) | 1u);
        for (volatile unsigned int d = 0; d < 1000000u; ++d)
            cpu_relax();
        reg_write(p, PORT_SCTL, sctl & ~0x0Fu);

        if (port_wait_det(p, 3u, 10000000u) != 0)
            return -1;

        sig = reg_read(p, PORT_SIG);
        if (sig != AHCI_SIG_SATA)
            return -1;
    }

    port_stop(p);

    memzero(clb[idx], sizeof(clb[idx]));
    memzero(fis[idx], sizeof(fis[idx]));
    memzero(ct[idx], sizeof(ct[idx]));
    memzero(idbuf[idx], sizeof(idbuf[idx]));

    abars[idx] = abar;
    drives[idx].port = (int)port;

    reg_write(p, PORT_CLB, (unsigned int)(unsigned long)clb[idx]);
    reg_write(p, PORT_CLBU, (unsigned int)((unsigned long long)(unsigned long)clb[idx] >> 32));
    reg_write(p, PORT_FB, (unsigned int)(unsigned long)fis[idx]);
    reg_write(p, PORT_FBU, (unsigned int)((unsigned long long)(unsigned long)fis[idx] >> 32));

    reg_write(p, PORT_IS, 0xFFFFFFFFu);
    reg_write(p, PORT_SERR, 0xFFFFFFFFu);
    reg_write(p, PORT_SACT, 0);
    reg_write(p, PORT_CI, 0);

    unsigned int cmd = reg_read(p, PORT_CMD);
    cmd |= CMD_FRE | CMD_SUD | CMD_POD | CMD_ST;
    reg_write(p, PORT_CMD, cmd);
    barrier();

    if (port_wait_idle(p, 10000000u) != 0)
        return -1;

    if (ahci_command(idx, ATA_CMD_IDENTIFY, 0, 0, 0, idbuf[idx], 512) != 0)
        return -1;

    unsigned short* id = idbuf[idx];
    unsigned int sectors = ((unsigned int)id[61] << 16) | id[60];

    if (id[83] & (1u << 10)) {
        unsigned long long lba48 =
            ((unsigned long long)id[103] << 48) |
            ((unsigned long long)id[102] << 32) |
            ((unsigned long long)id[101] << 16) |
            id[100];

        sectors = (lba48 > 0xFFFFFFFFull) ? 0xFFFFFFFFu : (unsigned int)lba48;
    }

    drives[idx].exists = 1;
    drives[idx].sectors = sectors;
    read_model(drives[idx].model, id);
    return 0;
}

int ahci_init(void)
{
    pci_dev_t dev;
    uint64_t bar = 0;
    int is64 = 0;

    info_count = 0;

    for (int i = 0; i < AHCI_MAX_DRIVES; i++) {
        drives[i].exists = 0;
        abars[i] = 0;
    }

    if (!pci_find_class(0x01u, 0x06u, 0x01u, &dev) &&
        !pci_find_class(0x01u, 0x06u, 0xFFu, &dev))
        return 0;

    if (!pci_enable_mem_busmaster(&dev))
        return 0;

    if (!pci_get_mmio_bar(&dev, 5u, &bar, &is64)) {
        bar = 0;
        for (unsigned int i = 0; i < 5u; i++) {
            if (pci_get_mmio_bar(&dev, i, &bar, &is64))
                break;
        }
    }

    if (!bar || bar >= 0x100000000ull)
        return 0;

    volatile unsigned char* abar = (volatile unsigned char*)(uintptr_t)bar;

    if (hba_reset(abar) != 0)
        return 0;

    unsigned int pi = reg_read(abar, HBA_PI);

    for (unsigned int port = 0; port < 32u && info_count < AHCI_MAX_DRIVES; port++) {
        if (!(pi & (1u << port)))
            continue;

        if (port_init(abar, port, info_count) == 0)
            info_count++;
    }

    return info_count;
}

int ahci_drive_count(void)
{
    return info_count;
}

ahci_drive_t* ahci_get_drive(int index)
{
    if (index < 0 || index >= info_count)
        return 0;

    return &drives[index];
}

int ahci_read_sector(int index, unsigned int lba, unsigned char* buffer)
{
    if (index < 0 || index >= info_count || !buffer)
        return -1;

    int result = ahci_command(index, ATA_CMD_READ, 0, lba, 1, bounce, 512);

    if (result == 0)
        memcopy(buffer, bounce, 512);

    return result;
}

int ahci_write_sector(int index, unsigned int lba, const unsigned char* buffer)
{
    if (index < 0 || index >= info_count || !buffer)
        return -1;

    memcopy(bounce, buffer, 512);

    int result = ahci_command(index, ATA_CMD_WRITE, 1, lba, 1, bounce, 512);

    if (result == 0)
        (void)ahci_command(index, ATA_CMD_FLUSH, 0, 0, 0, 0, 0);

    return result;
}
