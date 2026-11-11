#include "audio.h"
#include "hda.h"
#include "ac97.h"
#include "es1370.h"
#include "sb16.h"

typedef struct {
    const char* name;
    int (*init)(void);
    int (*ready)(void);
    int (*play_wav)(const uint8_t* wav, uint32_t size);
    void (*service)(void);
    void (*wait)(void);
    int (*is_playing)(void);
} audio_driver_t;

static const audio_driver_t g_drivers[] = {
    {
        "Intel HDA",
        hda_init, hda_ready, hda_play_wav,
        hda_service, hda_wait, hda_is_playing
    },
    {
        "AC'97",
        ac97_init, ac97_ready, ac97_play_wav,
        ac97_service, ac97_wait, ac97_is_playing
    },
    {
        "Ensoniq ES1370",
        es1370_init, es1370_ready, es1370_play_wav,
        es1370_service, es1370_wait, es1370_is_playing
    },
    {
        "Sound Blaster 16",
        sb16_init, sb16_ready, sb16_play_wav,
        sb16_service, sb16_wait, sb16_is_playing
    },
};

#define AUDIO_DRIVER_COUNT \
    (sizeof(g_drivers) / sizeof(g_drivers[0]))

static int g_active = -1;
static int g_attempted;

int audio_init(void)
{
    if (g_active >= 0)
        return 1;
    if (g_attempted)
        return 0;

    g_attempted = 1;

    for (unsigned int i = 0; i < AUDIO_DRIVER_COUNT; i++)
    {
        if (g_drivers[i].init())
        {
            g_active = (int)i;
            return 1;
        }
    }

    return 0;
}

int audio_ready(void)
{
    return g_active >= 0;
}

int audio_play_wav(const uint8_t* wav, uint32_t size)
{
    if (!audio_init())
        return 0;

    return g_drivers[g_active].play_wav(wav, size);
}

void audio_service(void)
{
    if (g_active >= 0)
        g_drivers[g_active].service();
}

void audio_wait(void)
{
    if (g_active >= 0)
        g_drivers[g_active].wait();
}

int audio_is_playing(void)
{
    if (g_active < 0)
        return 0;

    return g_drivers[g_active].is_playing();
}

const char* audio_backend_name(void)
{
    if (g_active < 0)
        return "none";

    return g_drivers[g_active].name;
}
