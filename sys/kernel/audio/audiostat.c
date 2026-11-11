#include "nwos_internal.h"
#include "audio/audio.h"

void cmd_audio(void)
{
    print("Supported audio drivers:\n");
    print("  Intel HDA\n");
    print("  AC'97\n");
    print("  Ensoniq ES1370\n");
    print("  Sound Blaster 16\n\n");

    if (!audio_init())
    {
        print_error("No supported audio device detected.\n");
        return;
    }

    print_success("Active audio backend: ");
    print(audio_backend_name());
    print("\n");

    print("State: ");
    print(audio_is_playing() ? "playing\n" : "idle\n");
}
