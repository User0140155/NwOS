#ifndef NWOS_HDA_H
#define NWOS_HDA_H

#include <stdint.h>

/* Intel High Definition Audio output driver (polling, no interrupts). */

typedef struct {
    uint32_t rate;
    uint32_t channels;
    uint32_t bits;
    const uint8_t* data;
    uint32_t data_size;
    uint32_t frames;
} wav_info_t;

int wav_parse(const uint8_t* wav, uint32_t size, wav_info_t* info);
uint32_t wav_output_frames(const wav_info_t* info);
uint32_t wav_convert_48k_stereo16(const wav_info_t* info, int16_t* out,
                                  uint32_t out_frames);

int hda_init(void);
int hda_ready(void);

int hda_play_wav(const uint8_t* wav, uint32_t size);

void hda_service(void);

void hda_wait(void);

int hda_is_playing(void);

void startup_sound_start(void);

#endif
