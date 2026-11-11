#include "hda.h"

#ifndef HDA_HOST_TEST
#include "pci.h"
#include "memory/pmm.h"
#include "memory/mmu.h"
void sleep(unsigned int ms);
#endif

#ifndef HDA_AFTER_WRITE
#define HDA_AFTER_WRITE(off) ((void)0)
#endif
#ifndef HDA_BEFORE_READ
#define HDA_BEFORE_READ(off) ((void)0)
#endif

#define HDA_RATE_OUT 48000u
#define HDA_STREAM_TAG 1u
#define HDA_TAIL_FRAMES (HDA_RATE_OUT * 4u)
#define HDA_MAX_FRAMES  (HDA_RATE_OUT * 30u)

#define R_GCAP      0x00
#define R_GCTL      0x08
#define R_STATESTS  0x0E
#define R_CORBLBASE 0x40
#define R_CORBUBASE 0x44
#define R_CORBWP    0x48
#define R_CORBRP    0x4A
#define R_CORBCTL   0x4C
#define R_CORBSIZE  0x4E
#define R_RIRBLBASE 0x50
#define R_RIRBUBASE 0x54
#define R_RIRBWP    0x58
#define R_RINTCNT   0x5A
#define R_RIRBCTL   0x5C
#define R_RIRBSTS   0x5D
#define R_RIRBSIZE  0x5E

#define SD_CTL  0x00
#define SD_STS  0x03
#define SD_LPIB 0x04
#define SD_CBL  0x08
#define SD_LVI  0x0C
#define SD_FMT  0x12
#define SD_BDPL 0x18
#define SD_BDPU 0x1C

#define MAX_WIDGETS 128
#define MAX_PINS 16

static volatile uint8_t* g_base;
static int g_ready;
static int g_failed;

static uint32_t* g_corb;
static uint64_t* g_rirb;
static uint8_t* g_bdl;
static uint64_t g_ring_phys;
static unsigned g_corb_entries;
static unsigned g_rirb_entries;
static unsigned g_corb_wp;
static unsigned g_rirb_rp;
static unsigned g_codec;
static uint32_t g_sd;

static unsigned g_dac_count;
static uint8_t g_dacs[MAX_PINS];

static int g_playing;
static uint8_t* g_pcm;
static uint64_t g_pcm_phys;
static uint32_t g_pcm_size;
static uint32_t g_end_bytes;
static uint32_t g_last_lpib;

static uint8_t hda_r8(uint32_t off)
{
    HDA_BEFORE_READ(off);
    return *(volatile uint8_t*)(g_base + off);
}

static uint16_t hda_r16(uint32_t off)
{
    HDA_BEFORE_READ(off);
    return *(volatile uint16_t*)(g_base + off);
}

static uint32_t hda_r32(uint32_t off)
{
    HDA_BEFORE_READ(off);
    return *(volatile uint32_t*)(g_base + off);
}

static void hda_w8(uint32_t off, uint8_t v)
{
    *(volatile uint8_t*)(g_base + off) = v;
    HDA_AFTER_WRITE(off);
}

static void hda_w16(uint32_t off, uint16_t v)
{
    *(volatile uint16_t*)(g_base + off) = v;
    HDA_AFTER_WRITE(off);
}

static void hda_w32(uint32_t off, uint32_t v)
{
    *(volatile uint32_t*)(g_base + off) = v;
    HDA_AFTER_WRITE(off);
}

#ifdef HDA_HOST_TEST
void hda_test_sleep(unsigned int ms);
#define hda_sleep(ms) hda_test_sleep(ms)
#else
#define hda_sleep(ms) sleep(ms)
#endif

static int wait8(uint32_t off, uint8_t mask, uint8_t want, unsigned ms)
{
    for (unsigned i = 0; i <= ms; i++)
    {
        if ((hda_r8(off) & mask) == want)
            return 1;
        hda_sleep(1);
    }
    return 0;
}

static int wait16(uint32_t off, uint16_t mask, uint16_t want, unsigned ms)
{
    for (unsigned i = 0; i <= ms; i++)
    {
        if ((hda_r16(off) & mask) == want)
            return 1;
        hda_sleep(1);
    }
    return 0;
}

static int wait32(uint32_t off, uint32_t mask, uint32_t want, unsigned ms)
{
    for (unsigned i = 0; i <= ms; i++)
    {
        if ((hda_r32(off) & mask) == want)
            return 1;
        hda_sleep(1);
    }
    return 0;
}

static void zero_bytes(void* p, uint32_t n)
{
    volatile uint8_t* b = (volatile uint8_t*)p;
    for (uint32_t i = 0; i < n; i++)
        b[i] = 0;
}

static void cache_flush(const void* p, uint32_t n)
{
#ifndef HDA_HOST_TEST
    const uint8_t* b = (const uint8_t*)p;
    for (uint32_t i = 0; i < n; i += 64)
        __asm__ volatile ("clflush (%0)" : : "r"(b + i) : "memory");
    __asm__ volatile ("mfence" : : : "memory");
#else
    (void)p; (void)n;
#endif
}

/* ------------------------------------------------------------------ */
/* Codec commands                                                      */
/* ------------------------------------------------------------------ */

static int codec_cmd(uint32_t cmd, uint32_t* response)
{
    unsigned next = (g_corb_wp + 1) % g_corb_entries;

    g_corb[next] = cmd;
    cache_flush(&g_corb[next], 4);
    g_corb_wp = next;
    hda_w16(R_CORBWP, (uint16_t)next);

    for (unsigned i = 0; i < 200; i++)
    {
        unsigned wp = hda_r16(R_RIRBWP) & 0xFFu;

        if (wp != g_rirb_rp)
        {
            g_rirb_rp = (g_rirb_rp + 1) % g_rirb_entries;
            cache_flush(&g_rirb[g_rirb_rp], 8);

            if (response)
                *response = (uint32_t)(g_rirb[g_rirb_rp] & 0xFFFFFFFFull);

            hda_w8(R_RIRBSTS, 0x01);
            return 1;
        }

        hda_sleep(1);
    }

    return 0;
}

static uint32_t verb12(unsigned nid, unsigned verb, unsigned payload)
{
    return ((uint32_t)g_codec << 28) | ((uint32_t)nid << 20) |
           ((uint32_t)verb << 8) | (payload & 0xFFu);
}

static uint32_t verb4(unsigned nid, unsigned verb, unsigned payload)
{
    return ((uint32_t)g_codec << 28) | ((uint32_t)nid << 20) |
           ((uint32_t)verb << 16) | (payload & 0xFFFFu);
}

static uint32_t get_param(unsigned nid, unsigned param)
{
    uint32_t r = 0;

    if (!codec_cmd(verb12(nid, 0xF00, param), &r))
        return 0;

    return r;
}

static uint32_t get_verb(unsigned nid, unsigned verb, unsigned payload)
{
    uint32_t r = 0;

    if (!codec_cmd(verb12(nid, verb, payload), &r))
        return 0;

    return r;
}

static void set_verb(unsigned nid, unsigned verb, unsigned payload)
{
    codec_cmd(verb12(nid, verb, payload), 0);
}

static void set_verb16(unsigned nid, unsigned verb, unsigned payload)
{
    codec_cmd(verb4(nid, verb, payload), 0);
}

/* ------------------------------------------------------------------ */
/* Widget graph                                                        */
/* ------------------------------------------------------------------ */

#define W_OUTPUT   0
#define W_MIXER    2
#define W_SELECTOR 3
#define W_PIN      4

static unsigned conn_list(unsigned nid, uint8_t* out, unsigned max)
{
    uint32_t len_param = get_param(nid, 0x0E);
    unsigned len = len_param & 0x7Fu;
    int is_long = (len_param & 0x80u) != 0;
    unsigned n = 0;
    unsigned prev = 0;
    int have_prev = 0;

    if (len > 32)
        len = 32;

    for (unsigned i = 0; i < len && n < max;)
    {
        uint32_t r = get_verb(nid, 0xF02, i);

        if (is_long)
        {
            for (unsigned k = 0; k < 2 && i < len; k++, i++)
            {
                unsigned e = (r >> (16 * k)) & 0xFFFFu;
                unsigned id = e & 0x7FFFu;

                if ((e & 0x8000u) && have_prev)
                {
                    for (unsigned x = prev + 1; x <= id && n < max; x++)
                        out[n++] = (uint8_t)x;
                }
                else if (n < max)
                {
                    out[n++] = (uint8_t)id;
                }

                prev = id;
                have_prev = 1;
            }
        }
        else
        {
            for (unsigned k = 0; k < 4 && i < len; k++, i++)
            {
                unsigned e = (r >> (8 * k)) & 0xFFu;
                unsigned id = e & 0x7Fu;

                if ((e & 0x80u) && have_prev)
                {
                    for (unsigned x = prev + 1; x <= id && n < max; x++)
                        out[n++] = (uint8_t)x;
                }
                else if (n < max)
                {
                    out[n++] = (uint8_t)id;
                }

                prev = id;
                have_prev = 1;
            }
        }
    }

    return n;
}

static void unmute_widget(unsigned nid, unsigned caps, unsigned in_index,
                          unsigned afg)
{
    uint32_t amp_caps;

    if (caps & 0x04u)               /* output amplifier */
    {
        amp_caps = (caps & 0x08u) ? get_param(nid, 0x12) : get_param(afg, 0x12);
        set_verb16(nid, 0x3, 0xB000u | (amp_caps & 0x7Fu));
    }

    if (caps & 0x02u)               /* input amplifier */
    {
        amp_caps = (caps & 0x08u) ? get_param(nid, 0x0D) : get_param(afg, 0x0D);
        set_verb16(nid, 0x3, 0x7000u | (in_index << 8) | (amp_caps & 0x7Fu));
    }
}

/* Depth-first search from a widget towards an audio output converter.
 * On success the connection selectors along the path are programmed. */
static int route_to_dac(unsigned nid, unsigned afg, int depth, unsigned* dac)
{
    uint32_t caps = get_param(nid, 0x09);
    unsigned type = (caps >> 20) & 0xFu;

    set_verb(nid, 0x705, 0);        /* power state D0 */

    if (type == W_OUTPUT)
    {
        unmute_widget(nid, caps, 0, afg);
        *dac = nid;
        return 1;
    }

    if (depth >= 5 || (type != W_PIN && type != W_MIXER && type != W_SELECTOR))
        return 0;

    uint8_t list[32];
    unsigned n = conn_list(nid, list, 32);

    for (unsigned i = 0; i < n; i++)
    {
        if (list[i] == nid || list[i] == 0)
            continue;

        if (route_to_dac(list[i], afg, depth + 1, dac))
        {
            if (n > 1)
                set_verb(nid, 0x701, i);

            unmute_widget(nid, caps, i, afg);
            return 1;
        }
    }

    return 0;
}

static void remember_dac(unsigned dac)
{
    for (unsigned i = 0; i < g_dac_count; i++)
    {
        if (g_dacs[i] == dac)
            return;
    }

    if (g_dac_count < MAX_PINS)
        g_dacs[g_dac_count++] = (uint8_t)dac;
}

static int setup_codec(unsigned codec)
{
    g_codec = codec;
    g_dac_count = 0;

    uint32_t root = get_param(0, 0x04);
    unsigned fg_start = (root >> 16) & 0xFFu;
    unsigned fg_count = root & 0xFFu;
    int routed = 0;

    for (unsigned f = 0; f < fg_count && f < 8; f++)
    {
        unsigned afg = fg_start + f;

        if ((get_param(afg, 0x05) & 0xFFu) != 0x01u)
            continue;                       /* not an audio function group */

        set_verb(afg, 0x705, 0);            /* D0 */
        hda_sleep(10);

        uint32_t sub = get_param(afg, 0x04);
        unsigned w_start = (sub >> 16) & 0xFFu;
        unsigned w_count = sub & 0xFFu;

        if (w_count > MAX_WIDGETS)
            w_count = MAX_WIDGETS;

        for (unsigned w = 0; w < w_count; w++)
        {
            unsigned nid = w_start + w;
            uint32_t caps = get_param(nid, 0x09);

            if (((caps >> 20) & 0xFu) != W_PIN)
                continue;

            uint32_t pincaps = get_param(nid, 0x0C);
            uint32_t config = get_verb(nid, 0xF1C, 0);
            unsigned conn = (config >> 30) & 3u;
            unsigned dev = (config >> 20) & 0xFu;

            if (!(pincaps & 0x10u))         /* not output capable */
                continue;
            if (conn == 1)                  /* no physical connection */
                continue;
            if (dev > 2)                    /* line out, speaker, headphone */
                continue;

            unsigned dac = 0;

            if (!route_to_dac(nid, afg, 0, &dac))
                continue;

            set_verb(nid, 0x707, dev == 2 ? 0xC0u : 0x40u);

            if (pincaps & 0x10000u)         /* EAPD capable */
                set_verb(nid, 0x70C, 0x02);

            remember_dac(dac);
            routed = 1;
        }
    }

    return routed;
}

/* ------------------------------------------------------------------ */
/* Controller                                                          */
/* ------------------------------------------------------------------ */

static int controller_reset(void)
{
    hda_w32(R_GCTL, hda_r32(R_GCTL) & ~1u);

    if (!wait32(R_GCTL, 1u, 0u, 200))
        return 0;

    hda_sleep(2);
    hda_w32(R_GCTL, hda_r32(R_GCTL) | 1u);

    if (!wait32(R_GCTL, 1u, 1u, 200))
        return 0;

    hda_sleep(20);                  /* codecs announce themselves */
    return 1;
}

static int rings_init(void)
{
    uint64_t phys = 0;
    uint8_t* mem;

#ifdef HDA_HOST_TEST
    extern void* hda_test_alloc(unsigned size, uint64_t* phys);
    mem = (uint8_t*)hda_test_alloc(4096, &phys);
#else
    mem = (uint8_t*)pmm_alloc_dma32(4096, 4096, &phys);
#endif

    if (mem == 0)
        return 0;

    zero_bytes(mem, 4096);

#ifndef HDA_HOST_TEST
    mmu_mark_mmio_uncached(phys, 4096);
#endif

    g_ring_phys = phys;
    g_corb = (uint32_t*)mem;
    g_rirb = (uint64_t*)(mem + 1024);
    g_bdl = mem + 3072;

    uint8_t csz = hda_r8(R_CORBSIZE);

    hda_w8(R_CORBCTL, 0);
    wait8(R_CORBCTL, 0x02, 0, 50);

    if (csz & 0x40u)       { g_corb_entries = 256; hda_w8(R_CORBSIZE, (uint8_t)((csz & ~3u) | 2u)); }
    else if (csz & 0x20u)  { g_corb_entries = 16;  hda_w8(R_CORBSIZE, (uint8_t)((csz & ~3u) | 1u)); }
    else                   { g_corb_entries = 2;   hda_w8(R_CORBSIZE, (uint8_t)(csz & ~3u)); }

    uint8_t rsz = hda_r8(R_RIRBSIZE);

    hda_w8(R_RIRBCTL, 0);
    wait8(R_RIRBCTL, 0x02, 0, 50);

    if (rsz & 0x40u)       { g_rirb_entries = 256; hda_w8(R_RIRBSIZE, (uint8_t)((rsz & ~3u) | 2u)); }
    else if (rsz & 0x20u)  { g_rirb_entries = 16;  hda_w8(R_RIRBSIZE, (uint8_t)((rsz & ~3u) | 1u)); }
    else                   { g_rirb_entries = 2;   hda_w8(R_RIRBSIZE, (uint8_t)(rsz & ~3u)); }

    hda_w32(R_CORBLBASE, (uint32_t)phys);
    hda_w32(R_CORBUBASE, (uint32_t)(phys >> 32));
    hda_w32(R_RIRBLBASE, (uint32_t)(phys + 1024));
    hda_w32(R_RIRBUBASE, (uint32_t)((phys + 1024) >> 32));

    hda_w16(R_CORBRP, 0x8000u);
    wait16(R_CORBRP, 0x8000u, 0x8000u, 50);
    hda_w16(R_CORBRP, 0);
    wait16(R_CORBRP, 0x8000u, 0, 50);

    g_corb_wp = 0;
    hda_w16(R_CORBWP, 0);

    hda_w16(R_RIRBWP, 0x8000u);
    g_rirb_rp = 0;
    hda_w16(R_RINTCNT, 1);

    hda_w8(R_RIRBCTL, 0x03);
    hda_w8(R_CORBCTL, 0x02);

    return 1;
}

int hda_ready(void)
{
    return g_ready;
}

int hda_is_playing(void)
{
    return g_playing;
}

static int hda_bring_up(void)
{
    if (!controller_reset())
        return 0;

    uint16_t gcap = hda_r16(R_GCAP);
    unsigned iss = (gcap >> 8) & 0xFu;
    unsigned oss = (gcap >> 12) & 0xFu;

    if (oss == 0)
        return 0;

    g_sd = 0x80u + iss * 0x20u;

    unsigned codecs = hda_r16(R_STATESTS) & 0x7FFFu;

    if (codecs == 0)
        return 0;

    if (!rings_init())
        return 0;

    for (unsigned c = 0; c < 15; c++)
    {
        if (!(codecs & (1u << c)))
            continue;

        if (setup_codec(c))
        {
            g_ready = 1;
            g_failed = 0;
            return 1;
        }
    }

    return 0;
}

#ifdef HDA_HOST_TEST
int hda_init(void)
{
    return hda_bring_up();
}
#else
int hda_init(void)
{
    pci_dev_t dev;
    uint64_t bar = 0;
    int is64 = 0;

    if (g_ready)
        return 1;
    if (g_failed)
        return 0;

    g_failed = 1;

    if (!pmm_is_ready())
        return 0;
    if (!pci_find_class(0x04, 0x03, 0x00, &dev))
        return 0;
    if (!pci_enable_mem_busmaster(&dev))
        return 0;
    pci_set_power_d0(&dev);
    if (!pci_get_mmio_bar(&dev, 0, &bar, &is64) || bar == 0)
        return 0;

    mmu_mark_mmio_uncached(bar, 0x4000);
    g_base = (volatile uint8_t*)(uintptr_t)bar;

    return hda_bring_up();
}
#endif

/* ------------------------------------------------------------------ */
/* Playback                                                            */
/* ------------------------------------------------------------------ */

static void stream_stop(void)
{
    uint32_t ctl = hda_r32(g_sd + SD_CTL);

    hda_w32(g_sd + SD_CTL, ctl & ~0x2u);
    wait32(g_sd + SD_CTL, 0x2u, 0, 20);
}

static int stream_reset(void)
{
    uint32_t ctl = hda_r32(g_sd + SD_CTL);

    hda_w32(g_sd + SD_CTL, (ctl & ~0x2u) | 1u);

    if (!wait32(g_sd + SD_CTL, 1u, 1u, 50))
        return 0;

    hda_w32(g_sd + SD_CTL, hda_r32(g_sd + SD_CTL) & ~1u);

    return wait32(g_sd + SD_CTL, 1u, 0u, 50);
}

static void release_pcm(void)
{
#ifndef HDA_HOST_TEST
    if (g_pcm)
        pmm_free_pages((uintptr_t)g_pcm, (g_pcm_size + 4095u) / 4096u);
#endif
    g_pcm = 0;
    g_pcm_size = 0;
}

void hda_service(void)
{
    if (!g_playing)
        return;

    uint32_t lpib = hda_r32(g_sd + SD_LPIB);

    if (lpib >= g_end_bytes || lpib < g_last_lpib)
    {
        stream_stop();
        g_playing = 0;
        release_pcm();
        return;
    }

    g_last_lpib = lpib;
}

void hda_wait(void)
{
    for (unsigned i = 0; g_playing && i < 15000; i++)
    {
        hda_service();
        hda_sleep(2);
    }

    if (g_playing)
    {
        stream_stop();
        g_playing = 0;
        release_pcm();
    }
}

int hda_play_wav(const uint8_t* wav, uint32_t size)
{
    wav_info_t info;
    uint32_t frames;
    uint32_t data_bytes;
    uint32_t total;
    uint64_t phys = 0;
    uint8_t* mem;

    if (!g_ready)
        return 0;

    if (g_playing)
    {
        stream_stop();
        g_playing = 0;
        release_pcm();
    }

    if (!wav_parse(wav, size, &info))
        return 0;

    frames = wav_output_frames(&info);

    if (frames == 0 || frames > HDA_MAX_FRAMES)
        return 0;

    data_bytes = frames * 4u;
    total = (data_bytes + HDA_TAIL_FRAMES * 4u + 255u) & ~255u;

#ifdef HDA_HOST_TEST
    extern void* hda_test_alloc(unsigned size, uint64_t* phys);
    mem = (uint8_t*)hda_test_alloc(total, &phys);
#else
    mem = (uint8_t*)pmm_alloc_dma32(total, 128, &phys);
#endif

    if (mem == 0)
        return 0;

    zero_bytes(mem, total);
    wav_convert_48k_stereo16(&info, (int16_t*)mem, frames);
    cache_flush(mem, total);

    g_pcm = mem;
    g_pcm_phys = phys;
    g_pcm_size = total;
    g_end_bytes = data_bytes;
    g_last_lpib = 0;

    /* two buffer descriptors covering the buffer */
    uint32_t half = total / 2u;
    uint32_t* bdl = (uint32_t*)g_bdl;

    bdl[0] = (uint32_t)phys;
    bdl[1] = (uint32_t)(phys >> 32);
    bdl[2] = half;
    bdl[3] = 0;
    bdl[4] = (uint32_t)(phys + half);
    bdl[5] = (uint32_t)((phys + half) >> 32);
    bdl[6] = total - half;
    bdl[7] = 0;
    cache_flush(g_bdl, 32);

    if (!stream_reset())
    {
        release_pcm();
        return 0;
    }

    /* every DAC found by the codec scan plays the same stream */
    for (unsigned i = 0; i < g_dac_count; i++)
    {
        set_verb16(g_dacs[i], 0x2, 0x0011u);                /* 48 kHz 16-bit stereo */
        set_verb(g_dacs[i], 0x706, (HDA_STREAM_TAG << 4));  /* stream 1, channel 0 */
    }

    hda_w32(g_sd + SD_CTL, (hda_r32(g_sd + SD_CTL) & 0x000F00FFu & ~0x2u) |
                           (HDA_STREAM_TAG << 20));
    hda_w32(g_sd + SD_CBL, total);
    hda_w16(g_sd + SD_LVI, 1);
    hda_w16(g_sd + SD_FMT, 0x0011u);
    hda_w32(g_sd + SD_BDPL, (uint32_t)(g_ring_phys + 3072));
    hda_w32(g_sd + SD_BDPU, (uint32_t)((g_ring_phys + 3072) >> 32));
    hda_w8(g_sd + SD_STS, 0x1C);

    hda_w32(g_sd + SD_CTL, hda_r32(g_sd + SD_CTL) | 0x2u);     /* RUN */

    g_playing = 1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* WAV                                                                 */
/* ------------------------------------------------------------------ */

static uint32_t le16(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t le32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int wav_parse(const uint8_t* wav, uint32_t size, wav_info_t* info)
{
    uint32_t pos = 12;
    int have_fmt = 0;
    int have_data = 0;
    uint32_t format = 0;

    if (!wav || !info || size < 44)
        return 0;
    if (wav[0] != 'R' || wav[1] != 'I' || wav[2] != 'F' || wav[3] != 'F')
        return 0;
    if (wav[8] != 'W' || wav[9] != 'A' || wav[10] != 'V' || wav[11] != 'E')
        return 0;

    while (pos + 8 <= size)
    {
        uint32_t csize = le32(wav + pos + 4);
        const uint8_t* body = wav + pos + 8;
        uint32_t avail = size - (pos + 8);

        if (wav[pos] == 'f' && wav[pos + 1] == 'm' &&
            wav[pos + 2] == 't' && wav[pos + 3] == ' ')
        {
            if (csize < 16 || avail < 16)
                return 0;

            format = le16(body);
            info->channels = le16(body + 2);
            info->rate = le32(body + 4);
            info->bits = le16(body + 14);

            if (format == 0xFFFEu && csize >= 26 && avail >= 26)
                format = le16(body + 24);

            have_fmt = 1;
        }
        else if (wav[pos] == 'd' && wav[pos + 1] == 'a' &&
                 wav[pos + 2] == 't' && wav[pos + 3] == 'a')
        {
            info->data = body;
            info->data_size = csize > avail ? avail : csize;
            have_data = 1;
            break;
        }

        if (csize > avail)
            break;

        pos += 8 + csize + (csize & 1u);
    }

    if (!have_fmt || !have_data || format != 1)
        return 0;
    if (info->channels < 1 || info->channels > 8)
        return 0;
    if (info->rate < 4000 || info->rate > 192000)
        return 0;
    if (info->bits != 8 && info->bits != 16 && info->bits != 24 && info->bits != 32)
        return 0;

    uint32_t frame_bytes = info->channels * (info->bits / 8u);

    info->frames = info->data_size / frame_bytes;
    return info->frames != 0;
}

uint32_t wav_output_frames_at(const wav_info_t* info, uint32_t out_rate)
{
    uint64_t n;

    if (!info || out_rate == 0)
        return 0;

    n = ((uint64_t)info->frames * out_rate + info->rate - 1u) / info->rate;

    if (n > 0xFFFFFFFFull)
        return 0;

    return (uint32_t)n;
}

uint32_t wav_output_frames(const wav_info_t* info)
{
    return wav_output_frames_at(info, HDA_RATE_OUT);
}

static int32_t sample_at(const wav_info_t* info, uint32_t frame, uint32_t ch)
{
    uint32_t bytes = info->bits / 8u;
    const uint8_t* p = info->data + ((uint64_t)frame * info->channels + ch) * bytes;

    if (info->bits == 8)
        return ((int32_t)p[0] - 128) << 8;
    if (info->bits == 16)
        return (int16_t)(p[0] | (p[1] << 8));
    if (info->bits == 24)
        return (int16_t)(p[1] | (p[2] << 8));

    return (int16_t)(p[2] | (p[3] << 8));
}

uint32_t wav_convert_stereo16(const wav_info_t* info, int16_t* out,
                              uint32_t out_frames, uint32_t out_rate)
{
    uint32_t right;

    if (!info || !out || out_rate == 0 || info->frames == 0)
        return 0;

    right = info->channels > 1 ? 1u : 0u;

    for (uint32_t i = 0; i < out_frames; i++)
    {
        uint64_t pos = ((uint64_t)i * info->rate << 16) / out_rate;
        uint32_t idx = (uint32_t)(pos >> 16);
        int32_t frac = (int32_t)(pos & 0xFFFFu);
        uint32_t next = idx + 1 < info->frames ? idx + 1 : idx;

        if (idx >= info->frames)
        {
            idx = info->frames - 1;
            next = idx;
        }

        int32_t l0 = sample_at(info, idx, 0);
        int32_t l1 = sample_at(info, next, 0);
        int32_t r0 = sample_at(info, idx, right);
        int32_t r1 = sample_at(info, next, right);

        out[i * 2]     = (int16_t)(l0 + (((l1 - l0) * frac) >> 16));
        out[i * 2 + 1] = (int16_t)(r0 + (((r1 - r0) * frac) >> 16));
    }

    return out_frames;
}

uint32_t wav_convert_48k_stereo16(const wav_info_t* info, int16_t* out,
                                  uint32_t out_frames)
{
    return wav_convert_stereo16(info, out, out_frames, HDA_RATE_OUT);
}
