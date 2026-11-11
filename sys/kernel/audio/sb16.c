#include "sb16.h"
#include "wav.h"
#include "pci.h"
#include "memory/pmm.h"
#include "timer.h"

void sleep(unsigned int ms);

#define SB_RATE_OUT        44100u
#define SB_MAX_FRAMES      32768u

#define SB_DMA_CHANNEL     5u
#define SB_DMA_SLAVE_MASK  0xD4u
#define SB_DMA_SLAVE_MODE  0xD6u
#define SB_DMA_SLAVE_FF    0xD8u
#define SB_DMA_SLAVE_ADDR  0xC4u
#define SB_DMA_SLAVE_COUNT 0xC6u
#define SB_DMA_PAGE_CH5    0x8Bu

#define SB_DSP_RESET       0x06u
#define SB_DSP_READ        0x0Au
#define SB_DSP_WRITE       0x0Cu
#define SB_DSP_WRITE_STAT  0x0Cu
#define SB_DSP_READ_STAT   0x0Eu

#define SB_MIXER_INDEX     0x04u
#define SB_MIXER_DATA      0x05u

#define SB_CMD_RATE        0x41u
#define SB_CMD_SPEAKER_ON  0xD1u
#define SB_CMD_SPEAKER_OFF 0xD3u
#define SB_CMD_HALT_16     0xD5u
#define SB_CMD_PLAY_16     0xB0u

static uint16_t g_base;

static int g_ready;
static int g_failed;
static int g_playing;

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

static void short_delay(void)
{
    for (volatile int i = 0; i < 10000; i++)
        __asm__ volatile ("pause");
}

static int dsp_write_ready(void)
{
    for (uint32_t i = 0; i < 100000u; i++)
    {
        if (!(pci_inb(g_base + SB_DSP_WRITE_STAT) & 0x80u))
            return 1;
        __asm__ volatile ("pause");
    }

    return 0;
}

static void dsp_write(uint8_t value)
{
    if (dsp_write_ready())
        pci_outb(g_base + SB_DSP_WRITE, value);
}

static void mixer_write(uint8_t index, uint8_t value)
{
    pci_outb(g_base + SB_MIXER_INDEX, index);
    pci_outb(g_base + SB_MIXER_DATA, value);
}

static int detect_at(uint16_t base)
{
    unsigned long long start;

    g_base = base;

    pci_outb(base + SB_DSP_RESET, 1);
    short_delay();
    pci_outb(base + SB_DSP_RESET, 0);
    short_delay();

    start = timer_ms();
    while (timer_ms() - start < 30u)
    {
        if (pci_inb(base + SB_DSP_READ_STAT) & 0x80u)
            return pci_inb(base + SB_DSP_READ) == 0xAAu;
        __asm__ volatile ("pause");
    }

    return 0;
}

static void stop_stream(void)
{
    dsp_write(SB_CMD_HALT_16);
    dsp_write(SB_CMD_SPEAKER_OFF);
    pci_outb(SB_DMA_SLAVE_MASK, (uint8_t)(SB_DMA_CHANNEL & 3u) | 0x04u);
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

int sb16_init(void)
{
    static const uint16_t bases[] = { 0x220u, 0x240u, 0x260u, 0x280u };
    int found = 0;

    if (g_ready)
        return 1;
    if (g_failed)
        return 0;

    g_failed = 1;

    if (!pmm_is_ready())
        return 0;

    for (unsigned i = 0; i < sizeof(bases) / sizeof(bases[0]); i++)
    {
        if (detect_at(bases[i]))
        {
            found = 1;
            g_base = bases[i];
            break;
        }
    }

    if (!found)
        return 0;

    mixer_write(0x22u, 0x00u);
    mixer_write(0x04u, 0x00u);
    mixer_write(0x0Eu, 0x02u);

    dsp_write(SB_CMD_SPEAKER_OFF);

    g_ready = 1;
    g_failed = 0;
    return 1;
}

int sb16_ready(void)
{
    return g_ready;
}

int sb16_play_wav(const uint8_t* wav, uint32_t size)
{
    wav_info_t info;
    uint64_t pcm_phys;
    uint32_t frames;
    uint32_t bytes;
    uint32_t pages;
    uint32_t words;
    uint32_t count;
    uint32_t word_addr;
    uint32_t block_frames;

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

    frames = wav_output_frames_at(&info, SB_RATE_OUT);
    if (frames == 0)
        return 0;
    if (frames > SB_MAX_FRAMES)
        frames = SB_MAX_FRAMES;

    bytes = frames * 4u;
    pages = (bytes + 4095u) / 4096u;

    g_pcm = pmm_alloc_dma16(bytes, 4096, &pcm_phys);
    if (!g_pcm)
        return 0;

    g_pcm_phys = pcm_phys;
    g_pcm_pages = pages;

    zero_bytes(g_pcm, pages * 4096u);
    wav_convert_stereo16(&info, (int16_t*)g_pcm, frames, SB_RATE_OUT);
    cache_flush(g_pcm, pages * 4096u);

    stop_stream();

    words = bytes / 2u;
    count = words - 1u;
    word_addr = (uint32_t)(g_pcm_phys >> 1);

    pci_outb(SB_DMA_SLAVE_MASK, (uint8_t)(SB_DMA_CHANNEL & 3u) | 0x04u);
    pci_outb(SB_DMA_SLAVE_FF, 0xFFu);
    pci_outb(SB_DMA_SLAVE_ADDR, (uint8_t)(word_addr & 0xFFu));
    pci_outb(SB_DMA_SLAVE_ADDR, (uint8_t)((word_addr >> 8) & 0xFFu));
    pci_outb(SB_DMA_PAGE_CH5, (uint8_t)((g_pcm_phys >> 16) & 0xFFu));
    pci_outb(SB_DMA_SLAVE_FF, 0xFFu);
    pci_outb(SB_DMA_SLAVE_COUNT, (uint8_t)(count & 0xFFu));
    pci_outb(SB_DMA_SLAVE_COUNT, (uint8_t)((count >> 8) & 0xFFu));
    pci_outb(SB_DMA_SLAVE_MODE, 0x49u);

    dsp_write(SB_CMD_SPEAKER_ON);
    dsp_write(SB_CMD_RATE);
    dsp_write((uint8_t)(SB_RATE_OUT & 0xFFu));
    dsp_write((uint8_t)((SB_RATE_OUT >> 8) & 0xFFu));

    block_frames = frames - 1u;
    dsp_write(SB_CMD_PLAY_16);
    dsp_write(0x30u);
    dsp_write((uint8_t)(block_frames & 0xFFu));
    dsp_write((uint8_t)((block_frames >> 8) & 0xFFu));

    pci_outb(SB_DMA_SLAVE_MASK, (uint8_t)(SB_DMA_CHANNEL & 3u));

    g_end_ms = timer_ms() + ((unsigned long long)frames * 1000ull) / SB_RATE_OUT + 100ull;
    g_playing = 1;
    return 1;
}

void sb16_service(void)
{
    if (!g_playing)
        return;

    if (timer_ms() >= g_end_ms)
    {
        stop_stream();
        g_playing = 0;
        release_pcm();
    }
}

void sb16_wait(void)
{
    for (uint32_t i = 0; i < 15000u && g_playing; i++)
    {
        sb16_service();
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

int sb16_is_playing(void)
{
    return g_playing;
}
