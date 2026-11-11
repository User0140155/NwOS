#ifndef NWOS_AUDIO_ES1370_H
#define NWOS_AUDIO_ES1370_H

#include <stdint.h>

/* Ensoniq ES1370 (AudioPCI) output driver, polled completion. */

int es1370_init(void);
int es1370_ready(void);

int es1370_play_wav(const uint8_t* wav, uint32_t size);

void es1370_service(void);

void es1370_wait(void);

int es1370_is_playing(void);

#endif
