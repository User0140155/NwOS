#include "nwos_internal.h"
#include "audio.h"
#include "wav.h"
#include "timer.h"
#include "keyboard.h"

#define AUDIO_FILE_MAX_BYTES (16u * 1024u * 1024u)
#define VIBE_CHUNK_MS 650u

static unsigned char* vibe_file;
static unsigned int vibe_file_size;
static wav_info_t vibe_info;
static uint32_t vibe_frame;
static uint32_t vibe_chunk_start_frame;
static uint32_t vibe_chunk_frames;
static unsigned long long vibe_chunk_started_ms;
static int vibe_active;
static int vibe_paused;
static int vibe_chunk_running;

static int find_audio_file(int dir, const char* name)
{
    for (int i = 1; i < fs_dir_cap; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_FILE &&
            directory[i].parent == (uint16_t)dir &&
            strcmp(directory[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int load_audio_file(const char* args, unsigned char** data_out,
                           unsigned int* size_out, char* leaf_out,
                           unsigned int leaf_capacity)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    int parent;
    int index;
    int length = 0;
    int saved_dir;
    unsigned int size;
    unsigned char* wav;

    if (!args || !args[0] || !data_out || !size_out || !leaf_out ||
        leaf_capacity < 2)
    {
        print_error("Usage: <player> <file.wav>\n");
        return 0;
    }

    if (fs_split_file_path(args, dir_path, sizeof(dir_path), leaf))
        parent = fs_resolve_dir_path(dir_path);
    else
    {
        parent = current_dir;
        while (args[length] != '\0')
        {
            if (length >= MAX_FILENAME - 1)
            {
                print_error("Filename is too long.\n");
                return 0;
            }
            leaf[length] = args[length];
            length++;
        }
        leaf[length] = '\0';
    }

    if (parent < 0)
    {
        print_error("Directory not found.\n");
        return 0;
    }

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = find_audio_file(parent, leaf);
    current_dir = (uint16_t)saved_dir;
    if (index < 0)
    {
        print_error("File not found.\n");
        return 0;
    }

    size = directory[index].size;
    if (size < 44u || size > AUDIO_FILE_MAX_BYTES)
    {
        print_error("WAV file size must be between 44 bytes and 16 MiB.\n");
        return 0;
    }

    wav = (unsigned char*)fs_mem_alloc(size);
    if (!wav)
    {
        print_error("Not enough memory for the WAV file.\n");
        return 0;
    }
    if (!fs_file_read(index, 0, wav, size))
    {
        fs_mem_free(wav, size);
        print_error("Cannot read the WAV file.\n");
        return 0;
    }

    unsigned int i = 0;
    while (leaf[i] && i + 1 < leaf_capacity)
    {
        leaf_out[i] = leaf[i];
        ++i;
    }
    leaf_out[i] = '\0';
    *data_out = wav;
    *size_out = size;
    return 1;
}

static void put_le16(unsigned char* p, uint16_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void put_le32(unsigned char* p, uint32_t value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
    p[2] = (unsigned char)(value >> 16);
    p[3] = (unsigned char)(value >> 24);
}

static uint32_t vibe_current_frame(void)
{
    uint64_t advanced;
    uint64_t elapsed;
    if (!vibe_active || vibe_paused || !vibe_chunk_running)
        return vibe_frame;

    elapsed = timer_ms() - vibe_chunk_started_ms;
    advanced = (elapsed * vibe_info.rate) / 1000ULL;
    if (advanced > vibe_chunk_frames)
        advanced = vibe_chunk_frames;
    return vibe_chunk_start_frame + (uint32_t)advanced;
}

static void vibe_release(void)
{
    if (vibe_file)
        fs_mem_free(vibe_file, vibe_file_size);
    vibe_file = 0;
    vibe_file_size = 0;
    vibe_frame = 0;
    vibe_chunk_start_frame = 0;
    vibe_chunk_frames = 0;
    vibe_chunk_started_ms = 0;
    vibe_active = 0;
    vibe_paused = 0;
    vibe_chunk_running = 0;
}

void audio_player_stop(void)
{
    if (vibe_chunk_running)
        audio_stop();
    vibe_release();
}

int audio_player_active(void)
{
    return vibe_active;
}

int audio_player_paused(void)
{
    return vibe_active && vibe_paused;
}

static int vibe_start_segment(void)
{
    uint32_t block_align;
    uint32_t chunk_frames;
    uint32_t chunk_bytes;
    uint32_t remaining_frames;
    uint32_t segment_size;
    uint64_t suggested_frames;
    unsigned char* segment;
    int started;

    if (!vibe_active || vibe_frame >= vibe_info.frames)
        return 0;

    block_align = vibe_info.channels * (vibe_info.bits / 8u);
    if (block_align == 0)
        return 0;

    remaining_frames = vibe_info.frames - vibe_frame;
    suggested_frames = ((uint64_t)vibe_info.rate * VIBE_CHUNK_MS) / 1000ULL;
    if (suggested_frames == 0)
        suggested_frames = 1;
    if (suggested_frames > remaining_frames)
        suggested_frames = remaining_frames;

    chunk_frames = (uint32_t)suggested_frames;
    chunk_bytes = chunk_frames * block_align;
    if (chunk_bytes > vibe_info.data_size - vibe_frame * block_align ||
        chunk_bytes > 0xFFFFFFFFu - 44u)
        return 0;
    segment_size = 44u + chunk_bytes;

    segment = (unsigned char*)fs_mem_alloc(segment_size);
    if (!segment)
        return 0;

    segment[0] = 'R'; segment[1] = 'I'; segment[2] = 'F'; segment[3] = 'F';
    put_le32(segment + 4, 36u + chunk_bytes);
    segment[8] = 'W'; segment[9] = 'A'; segment[10] = 'V'; segment[11] = 'E';
    segment[12] = 'f'; segment[13] = 'm'; segment[14] = 't'; segment[15] = ' ';
    put_le32(segment + 16, 16u);
    put_le16(segment + 20, 1u);
    put_le16(segment + 22, (uint16_t)vibe_info.channels);
    put_le32(segment + 24, vibe_info.rate);
    put_le32(segment + 28, vibe_info.rate * block_align);
    put_le16(segment + 32, (uint16_t)block_align);
    put_le16(segment + 34, (uint16_t)vibe_info.bits);
    segment[36] = 'd'; segment[37] = 'a'; segment[38] = 't'; segment[39] = 'a';
    put_le32(segment + 40, chunk_bytes);

    const unsigned char* source = vibe_info.data + (uint64_t)vibe_frame * block_align;
    for (uint32_t i = 0; i < chunk_bytes; ++i)
        segment[44u + i] = source[i];

    started = audio_play_wav(segment, segment_size);
    fs_mem_free(segment, segment_size);
    if (!started)
        return 0;

    vibe_chunk_start_frame = vibe_frame;
    vibe_chunk_frames = chunk_frames;
    vibe_chunk_started_ms = timer_ms();
    vibe_chunk_running = 1;
    return 1;
}

void audio_player_service(void)
{
    if (!vibe_active)
        return;

    audio_service();
    if (vibe_paused || !vibe_chunk_running || audio_is_playing())
        return;

    vibe_frame = vibe_chunk_start_frame + vibe_chunk_frames;
    vibe_chunk_running = 0;
    if (vibe_frame >= vibe_info.frames)
    {
        vibe_release();
        return;
    }

    if (!vibe_start_segment())
        vibe_release();
}

static void vibe_toggle_pause(void)
{
    if (!vibe_active)
        return;

    if (vibe_paused)
    {
        vibe_paused = 0;
        if (!vibe_start_segment())
            vibe_release();
        return;
    }

    vibe_frame = vibe_current_frame();
    if (vibe_chunk_running)
        audio_stop();
    vibe_chunk_running = 0;
    vibe_paused = 1;
}

static void vibe_seek_seconds(int seconds)
{
    int64_t target;
    uint32_t current;
    if (!vibe_active)
        return;

    current = vibe_current_frame();
    target = (int64_t)current + (int64_t)seconds * (int64_t)vibe_info.rate;
    if (target < 0)
        target = 0;
    if ((uint64_t)target > vibe_info.frames)
        target = vibe_info.frames;

    if (vibe_chunk_running)
        audio_stop();
    vibe_chunk_running = 0;
    vibe_frame = (uint32_t)target;

    if (!vibe_paused)
    {
        if (vibe_frame >= vibe_info.frames || !vibe_start_segment())
            vibe_release();
    }
}

int audio_player_handle_key(int key, uint8_t modifiers)
{
    if (!vibe_active ||
        !(modifiers & (KEY_MOD_LALT | KEY_MOD_RALT)))
        return 0;

    if (key == KEY_ESC || key == 27)
    {
        vibe_toggle_pause();
        return 1;
    }
    if (key == KEY_LEFT)
    {
        vibe_seek_seconds(-10);
        return 1;
    }
    if (key == KEY_RIGHT)
    {
        vibe_seek_seconds(10);
        return 1;
    }
    return 0;
}

void cmd_listen(const char* args)
{
    unsigned char* wav = 0;
    unsigned int size = 0;
    char leaf[MAX_FILENAME];
    wav_info_t info;

    if (!load_audio_file(args, &wav, &size, leaf, sizeof(leaf)))
        return;
    if (!wav_parse(wav, size, &info))
    {
        fs_mem_free(wav, size);
        print_error("Unsupported WAV format. Use uncompressed PCM WAV.\n");
        return;
    }

    audio_player_stop();
    print_success("Playing ");
    print(leaf);
    print("...\n");
    int started = audio_play_wav(wav, size);
    fs_mem_free(wav, size);
    if (!started)
    {
        print_error("Audio device unavailable or WAV format unsupported.\n");
        return;
    }
    audio_wait();
    print_success("Playback finished.\n");
}

void cmd_vibelisten(const char* args)
{
    char leaf[MAX_FILENAME];
    unsigned char* wav = 0;
    unsigned int size = 0;

    if (args && strcmp(args, "stop") == 0)
    {
        if (audio_player_active())
        {
            audio_player_stop();
            print_success("Background playback stopped.\n");
        }
        else
            print("No background playback is active.\n");
        return;
    }

    if (!load_audio_file(args, &wav, &size, leaf, sizeof(leaf)))
        return;

    if (!wav_parse(wav, size, &vibe_info))
    {
        fs_mem_free(wav, size);
        print_error("Unsupported WAV format. Use uncompressed PCM WAV.\n");
        return;
    }

    if (!audio_init())
    {
        fs_mem_free(wav, size);
        print_error("Audio device is unavailable.\n");
        return;
    }

    audio_player_stop();
    vibe_file = wav;
    vibe_file_size = size;
    vibe_frame = 0;
    vibe_active = 1;
    vibe_paused = 0;
    vibe_chunk_running = 0;

    if (!vibe_start_segment())
    {
        vibe_release();
        print_error("Could not start WAV playback.\n");
        return;
    }

    print_success("Background playback started: ");
    print(leaf);
    print("\nAlt+Esc: pause/resume; Alt+Left/Right: seek 10 seconds.\n");
}
