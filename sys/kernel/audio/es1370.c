#include "es1370.h"
#include "wav.h"
#include "pci.h"
#include "memory/pmm.h"
#include "timer.h"

void sleep(unsigned int ms);

#define ES_RATE_OUT        44100u
#define ES_MAX_FRAMES      65536u

#define ES_VENDOR          0x1274u
#define ES_DEVICE          0x5000u

#define ES_REG_CONTROL     0x00u
#define ES_REG_MEMPAGE     0x0Cu
#define ES_REG_SERIAL      0x20u
#define ES_REG_DAC1_COUNT  0x24u

#define ES_MEMPAGE_DAC1    0x0Cu
#define ES_DAC1_FRAME_ADDR 0x30u
#define ES_DAC1_FRAME_CNT  0x34u

#define ES_CONTROL_SERR_DIS 0x00000001u
#define ES_CONTROL_DAC1_EN  0x00000040u
#define ES_SERIAL_P1_16_ST 0x00000003u
#define ES_WTSRSEL_44100    (3u << 12)

static uint16_t g_io;

static int g_ready;
static int g_failed;
static int g_playing;

static uint8_t* g_pcm;
static uint64_t g_pcm_phys;
static uint32_t g_pcm_pages;
static uint32_t g_frames;
static int g_started;
static uint32_t g_last_remaining;
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

static void stop_stream(void)
{
    pci_outl(g_io + ES_REG_CONTROL, ES_CONTROL_SERR_DIS);
}

static void release_pcm(void)
{
    if (g_pcm)
    {
        pmm_free_pages((uintptr_t)g_pcm, g_pcm_pages);
        g_pcm = 0;
        g_pcm_phys = 0;
        g_pcm_pages = 0;
        g_frames = 0;
    }

    g_started = 0;
    g_last_remaining = 0;
}

int es1370_init(void)
{
    pci_dev_t dev;

    if (g_ready)
        return 1;
    if (g_failed)
        return 0;

    g_failed = 1;

    if (!pmm_is_ready())
        return 0;
    if (!pci_find(ES_VENDOR, ES_DEVICE, &dev))
        return 0;
    if (!pci_enable_io_busmaster(&dev))
        return 0;

    pci_set_power_d0(&dev);

    if (!pci_get_io_bar(&dev, 0, &g_io))
        return 0;

    pci_outl(g_io + ES_REG_CONTROL, ES_CONTROL_SERR_DIS);
    pci_outl(g_io + ES_REG_SERIAL, 0);
    pci_outl(g_io + ES_REG_MEMPAGE, 0);

    g_ready = 1;
    g_failed = 0;
    return 1;
}

int es1370_ready(void)
{
    return g_ready;
}

int es1370_play_wav(const uint8_t* wav, uint32_t size)
{
    wav_info_t info;
    uint64_t pcm_phys;
    uint32_t frames;
    uint32_t bytes;
    uint32_t pages;

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

    frames = wav_output_frames_at(&info, ES_RATE_OUT);
    if (frames == 0)
        return 0;
    if (frames > ES_MAX_FRAMES)
        frames = ES_MAX_FRAMES;

    bytes = frames * 4u;
    pages = (bytes + 4095u) / 4096u;

    g_pcm = pmm_alloc_dma32(bytes, 4096, &pcm_phys);
    if (!g_pcm)
        return 0;

    g_pcm_phys = pcm_phys;
    g_pcm_pages = pages;
    g_frames = frames;

    zero_bytes(g_pcm, pages * 4096u);
    wav_convert_stereo16(&info, (int16_t*)g_pcm, frames, ES_RATE_OUT);
    cache_flush(g_pcm, pages * 4096u);

    stop_stream();

    pci_outl(g_io + ES_REG_MEMPAGE, ES_MEMPAGE_DAC1);
    pci_outl(g_io + ES_DAC1_FRAME_ADDR, (uint32_t)g_pcm_phys);
    pci_outl(g_io + ES_DAC1_FRAME_CNT, (frames - 1u) & 0xFFFFu);

    pci_outl(g_io + ES_REG_DAC1_COUNT, (frames - 1u) & 0xFFFFu);

    pci_outl(g_io + ES_REG_SERIAL, ES_SERIAL_P1_16_ST);
    pci_outl(g_io + ES_REG_CONTROL,
             ES_WTSRSEL_44100 | ES_CONTROL_DAC1_EN | ES_CONTROL_SERR_DIS);

    g_started = 0;
    g_last_remaining = frames - 1u;
    g_end_ms = timer_ms() +
               ((unsigned long long)frames * 1000ull) / ES_RATE_OUT + 500ull;
    g_playing = 1;
    return 1;
}

void es1370_service(void)
{
    uint32_t count;
    uint32_t remaining;
    uint32_t total;

    if (!g_playing)
        return;

    count = pci_inl(g_io + ES_REG_DAC1_COUNT);
    remaining = count >> 16;
    total = count & 0xFFFFu;

    if (!g_started)
    {
        if (remaining < total)
            g_started = 1;
    }
    else if (remaining > g_last_remaining)
    {
        stop_stream();
        g_playing = 0;
        release_pcm();
        return;
    }

    g_last_remaining = remaining;

    if (timer_ms() >= g_end_ms)
    {
        stop_stream();
        g_playing = 0;
        release_pcm();
    }
}

void es1370_wait(void)
{
    for (uint32_t i = 0; i < 15000u && g_playing; i++)
    {
        es1370_service();
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

int es1370_is_playing(void)
{
    return g_playing;
}
