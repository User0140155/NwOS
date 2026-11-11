#ifndef NWOS_HDA_H
#define NWOS_HDA_H

#include <stdint.h>
#include "wav.h"

/* Intel High Definition Audio output driver (polling, no interrupts). */

int hda_init(void);
int hda_ready(void);

int hda_play_wav(const uint8_t* wav, uint32_t size);

void hda_service(void);

void hda_wait(void);

int hda_is_playing(void);

#endif
