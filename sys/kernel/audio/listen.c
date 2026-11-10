#include "nwos_internal.h"
#include "audio/hda.h"

static int find_audio_file(int dir, const char* name)
{
    for (int i = 1; i < fs_dir_cap; i++)
    {
        if (directory[i].used &&
            directory[i].type == FS_TYPE_FILE &&
            directory[i].parent == (uint16_t)dir &&
            strcmp(directory[i].name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

void cmd_listen(const char* args)
{
    char dir_path[256];
    char leaf[MAX_FILENAME];
    unsigned int size;
    unsigned char* wav;
    int length = 0;
    int parent;
    int index;
    int saved_dir;

    if (args == 0 || args[0] == '\0')
    {
        print_error("Usage: listen <file.wav>\n");
        return;
    }

    if (fs_split_file_path(args, dir_path, sizeof(dir_path), leaf))
    {
        parent = fs_resolve_dir_path(dir_path);
    }
    else
    {
        parent = current_dir;

        while (args[length] != '\0')
        {
            if (length >= MAX_FILENAME - 1)
            {
                print_error("Filename is too long.\n");
                return;
            }

            leaf[length] = args[length];
            length++;
        }

        leaf[length] = '\0';
    }

    if (parent < 0)
    {
        print_error("Directory not found.\n");
        return;
    }

    saved_dir = current_dir;
    current_dir = (uint16_t)parent;
    index = find_audio_file(parent, leaf);
    current_dir = saved_dir;

    if (index < 0)
    {
        print_error("File not found.\n");
        return;
    }

    size = directory[index].size;

    if (size < 44 || size > 16u * 1024u * 1024u)
    {
        print_error("Invalid WAV file size.\n");
        return;
    }

    if (!hda_init())
    {
        print_error("Audio device is unavailable.\n");
        return;
    }

    wav = (unsigned char*)fs_mem_alloc(size);

    if (wav == 0)
    {
        print_error("Not enough memory for the WAV file.\n");
        return;
    }

    if (!fs_file_read(index, 0, wav, size))
    {
        fs_mem_free(wav, size);
        print_error("Cannot read the WAV file.\n");
        return;
    }

    print_success("Playing ");
    print(leaf);
    print("...\n");

    int started = hda_play_wav(wav, size);

    fs_mem_free(wav, size);

    if (!started)
    {
        print_error("Unsupported WAV file.\n");
        return;
    }

    hda_wait();

    print_success("Playback finished.\n");
}
