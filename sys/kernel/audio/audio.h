#ifndef NWOS_AUDIO_H
#define NWOS_AUDIO_H

#include <stdint.h>

/* Common audio output front-end. Picks the first working backend. */

int audio_init(void);
int audio_ready(void);

int audio_play_wav(const uint8_t* wav, uint32_t size);

void audio_service(void);
void audio_stop(void);
void audio_wait(void);
int audio_is_playing(void);

/* Background WAV player. Keyboard shortcuts are routed here by the input
   driver so they also work while the shell or an application is waiting. */
void audio_player_service(void);
int audio_player_handle_key(int key, uint8_t modifiers);
void audio_player_stop(void);
int audio_player_active(void);
int audio_player_paused(void);

const char* audio_backend_name(void);

void startup_sound_start(void);

#endif
