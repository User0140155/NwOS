#ifndef NWOS_AUDIO_AC97_H
#define NWOS_AUDIO_AC97_H

#include <stdint.h>

/* Intel AC'97 audio output driver (bus-master, polling, no interrupts). */

int ac97_init(void);
int ac97_ready(void);

int ac97_play_wav(const uint8_t* wav, uint32_t size);

void ac97_service(void);

void ac97_wait(void);

int ac97_is_playing(void);

#endif
