#ifndef NWOS_AUDIO_H
#define NWOS_AUDIO_H

#include <stdint.h>

/* Common audio output front-end. Picks the first working backend. */

int audio_init(void);
int audio_ready(void);

int audio_play_wav(const uint8_t* wav, uint32_t size);

void audio_service(void);

void audio_wait(void);

int audio_is_playing(void);

const char* audio_backend_name(void);

void startup_sound_start(void);

#endif
