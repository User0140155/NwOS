#ifndef NWOS_AUDIO_WAV_H
#define NWOS_AUDIO_WAV_H

#include <stdint.h>

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
uint32_t wav_output_frames_at(const wav_info_t* info, uint32_t out_rate);
uint32_t wav_convert_48k_stereo16(const wav_info_t* info, int16_t* out,
                                  uint32_t out_frames);
uint32_t wav_convert_stereo16(const wav_info_t* info, int16_t* out,
                              uint32_t out_frames, uint32_t out_rate);

#endif
