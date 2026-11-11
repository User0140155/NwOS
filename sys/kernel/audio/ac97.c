#include "ac97.h"
#include "wav.h"
#include "pci.h"
#include "memory/pmm.h"
#include "timer.h"

void sleep(unsigned int ms);

#define AC97_RATE_OUT      48000u
#define AC97_MAX_BD        32u
#define AC97_MAX_BD_BYTES  131068u
#define AC97_MAX_BYTES     (AC97_MAX_BD * AC97_MAX_BD_BYTES)
#define AC97_BDL_SIZE      4096u

#define AC97_NAM_RESET     0x00u
#define AC97_NAM_MASTER    0x02u
#define AC97_NAM_PCM_OUT   0x18u
#define AC97_NAM_POWERDOWN 0x26u
#define AC97_NAM_EXT_ID    0x28u
#define AC97_NAM_EXT_CTRL  0x2Au
#define AC97_NAM_PCM_RATE  0x2Cu

#define AC97_NABM_PO_BDBAR 0x10u
#define AC97_NABM_PO_LVI   0x15u
#define AC97_NABM_PO_SR    0x16u
#define AC97_NABM_PO_CR    0x1Bu

#define AC97_CR_RPBM       0x01u
#define AC97_CR_RR         0x02u
#define AC97_SR_DCH        0x01u
#define AC97_SR_CLEAR      0x1Cu

#define AC97_BD_IOC        (1u << 31)
#define AC97_BD_BUP        (1u << 30)

static uint16_t g_nam;
static uint16_t g_nabm;

static int g_ready;
static int g_failed;
static int g_playing;

static uint8_t* g_bdl;
static uint64_t g_bdl_phys;
static uint8_t* g_pcm;
static uint64_t g_pcm_phys;
static uint32_t g_pcm_pages;
static unsigned long long g_end_ms;

static void cache_flush(const void* p, uint32_t n)
{
    const uint8_t* b = (const uint8_t*)p;

    for (uint32_t i = 0; i < n; i += 64)
        __asm__ volatile ("clflush (%0)" : : "r"(b + i) : "memory");
    __asm__ volatile ("mfence" : : : "memory");
}

static void zero_bytes(void* p, uint32_t n)
{
    uint8_t* b = (uint8_t*)p;

    for (uint32_t i = 0; i < n; i++)
        b[i] = 0;
}

static int find_controller(pci_dev_t* out)
{
    pci_dev_t devices[64];
    int count = pci_scan(devices, 64);

    for (int i = 0; i < count; i++)
    {
        uint16_t nam;
        uint16_t nabm;

        if (devices[i].class_code != 0x04u || devices[i].subclass != 0x01u)
            continue;

        /* Ensoniq ES1370/ES1371 are handled by their own driver. */
        if (devices[i].vendor_id == 0x1274u)
            continue;

        if (!pci_get_io_bar(&devices[i], 0, &nam))
            continue;
        if (!pci_get_io_bar(&devices[i], 1, &nabm))
            continue;

        *out = devices[i];
        return 1;
    }

    return 0;
}

static void stop_stream(void)
{
    pci_outb(g_nabm + AC97_NABM_PO_CR, 0);
    pci_outb(g_nabm + AC97_NABM_PO_CR, AC97_CR_RR);
    sleep(1);
    pci_outb(g_nabm + AC97_NABM_PO_CR, 0);
}

static void release_pcm(void)
{
    if (g_pcm)
    {
        pmm_free_pages((uintptr_t)g_pcm, g_pcm_pages);
        g_pcm = 0;
        g_pcm_phys = 0;
        g_pcm_pages = 0;
    }
}

int ac97_init(void)
{
    pci_dev_t dev;
    uint64_t bdl_phys;

    if (g_ready)
        return 1;
    if (g_failed)
        return 0;

    g_failed = 1;

    if (!pmm_is_ready())
        return 0;
    if (!find_controller(&dev))
        return 0;
    if (!pci_enable_io_busmaster(&dev))
        return 0;

    pci_set_power_d0(&dev);

    if (!pci_get_io_bar(&dev, 0, &g_nam))
        return 0;
    if (!pci_get_io_bar(&dev, 1, &g_nabm))
        return 0;

    pci_outw(g_nam + AC97_NAM_RESET, 0);
    for (int i = 0; i < 10; i++)
    {
        (void)pci_inw(g_nam + AC97_NAM_RESET);
        sleep(1);
    }

    pci_outw(g_nam + AC97_NAM_POWERDOWN, 0);
    pci_outw(g_nam + AC97_NAM_MASTER, 0);
    pci_outw(g_nam + AC97_NAM_PCM_OUT, 0);

    (void)pci_inw(g_nam + AC97_NAM_EXT_ID);
    pci_outw(g_nam + AC97_NAM_EXT_CTRL, 0x0001u);
    pci_outw(g_nam + AC97_NAM_PCM_RATE, AC97_RATE_OUT);

    g_bdl = pmm_alloc_dma32(AC97_BDL_SIZE, AC97_BDL_SIZE, &bdl_phys);
    if (!g_bdl)
        return 0;

    g_bdl_phys = bdl_phys;
    zero_bytes(g_bdl, AC97_BDL_SIZE);
    cache_flush(g_bdl, AC97_BDL_SIZE);

    stop_stream();
    pci_outw(g_nabm + AC97_NABM_PO_SR, AC97_SR_CLEAR);

    g_ready = 1;
    g_failed = 0;
    return 1;
}

int ac97_ready(void)
{
    return g_ready;
}

int ac97_play_wav(const uint8_t* wav, uint32_t size)
{
    wav_info_t info;
    uint64_t pcm_phys;
    uint32_t frames;
    uint32_t data_bytes;
    uint32_t pages;
    uint32_t offset = 0;
    uint32_t entries = 0;

    if (!g_ready)
        return 0;
    if (g_playing)
    {
        stop_stream();
        g_playing = 0;
        release_pcm();
    }

    if (!wav_parse(wav, size, &info))
        return 0;

    frames = wav_output_frames_at(&info, AC97_RATE_OUT);
    if (frames == 0)
        return 0;

    data_bytes = frames * 4u;
    if (data_bytes > AC97_MAX_BYTES)
    {
        data_bytes = AC97_MAX_BYTES;
        frames = data_bytes / 4u;
    }

    pages = (data_bytes + 4095u) / 4096u;
    g_pcm = pmm_alloc_dma32(data_bytes, 4096, &pcm_phys);
    if (!g_pcm)
        return 0;

    g_pcm_phys = pcm_phys;
    g_pcm_pages = pages;

    zero_bytes(g_pcm, pages * 4096u);
    wav_convert_stereo16(&info, (int16_t*)g_pcm, frames, AC97_RATE_OUT);
    cache_flush(g_pcm, pages * 4096u);

    while (offset < data_bytes && entries < AC97_MAX_BD)
    {
        uint32_t chunk = data_bytes - offset;
        uint32_t* bd = (uint32_t*)(g_bdl + entries * 8u);
        uint32_t control;

        if (chunk > AC97_MAX_BD_BYTES)
            chunk = AC97_MAX_BD_BYTES;
        chunk &= ~3u;
        if (chunk == 0)
            break;

        control = (chunk / 2u) & 0xFFFFu;

        if (offset + chunk >= data_bytes)
            control |= AC97_BD_IOC | AC97_BD_BUP;

        bd[0] = (uint32_t)(g_pcm_phys + offset);
        bd[1] = control;

        offset += chunk;
        entries++;
    }

    if (entries == 0)
    {
        release_pcm();
        return 0;
    }

    cache_flush(g_bdl, entries * 8u);

    stop_stream();
    pci_outw(g_nabm + AC97_NABM_PO_SR, AC97_SR_CLEAR);

    pci_outl(g_nabm + AC97_NABM_PO_BDBAR, (uint32_t)g_bdl_phys);
    pci_outb(g_nabm + AC97_NABM_PO_LVI, (uint8_t)(entries - 1u));
    pci_outb(g_nabm + AC97_NABM_PO_CR, AC97_CR_RPBM);

    g_end_ms = timer_ms() +
               ((unsigned long long)frames * 1000ull) / AC97_RATE_OUT + 250ull;
    g_playing = 1;
    return 1;
}

void ac97_service(void)
{
    uint16_t status;

    if (!g_playing)
        return;

    status = pci_inw(g_nabm + AC97_NABM_PO_SR);
    if ((status & AC97_SR_DCH) || timer_ms() >= g_end_ms)
    {
        pci_outb(g_nabm + AC97_NABM_PO_CR, 0);
        g_playing = 0;
        release_pcm();
    }
}

void ac97_wait(void)
{
    for (uint32_t i = 0; i < 15000u && g_playing; i++)
    {
        ac97_service();
        if (!g_playing)
            return;
        sleep(2);
    }

    if (g_playing)
    {
        stop_stream();
        g_playing = 0;
        release_pcm();
    }
}

int ac97_is_playing(void)
{
    return g_playing;
}
