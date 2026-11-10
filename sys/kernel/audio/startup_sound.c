#include "nwos_internal.h"
#include "audio/hda.h"

static int find_in_dir(int dir, const char* name)
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

void startup_sound_start(void)
{
    static const char path_dir[] = "/sys";
    static const char file_name[] = "starting.wav";
    int dir;
    int index;
    unsigned int size;
    unsigned char* wav;

    dir = fs_resolve_dir_path(path_dir);

    if (dir < 0)
        return;

    index = find_in_dir(dir, file_name);

    if (index < 0)
        return;

    size = directory[index].size;

    if (size < 44 || size > 16u * 1024u * 1024u)
        return;

    if (!hda_init())
        return;

    wav = (unsigned char*)fs_mem_alloc(size);

    if (wav == 0)
        return;

    if (fs_file_read(index, 0, wav, size))
        hda_play_wav(wav, size);

    fs_mem_free(wav, size);
}
