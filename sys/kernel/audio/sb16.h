#ifndef NWOS_AUDIO_SB16_H
#define NWOS_AUDIO_SB16_H

#include <stdint.h>

/* Sound Blaster 16 (ISA, DSP) output driver using 16-bit DMA channel 5. */

int sb16_init(void);
int sb16_ready(void);

int sb16_play_wav(const uint8_t* wav, uint32_t size);

void sb16_service(void);

void sb16_wait(void);

int sb16_is_playing(void);

#endif
