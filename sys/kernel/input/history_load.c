#include "nwos_internal.h"

static void history_check_size(void);

int console_f7_enabled = 0;

char cmd_history[MAX_HISTORY_COMMANDS][MAX_CMD_LENGTH];
int history_count = 0;
int history_pos = -1;

void wait_key(void)
{
    print("\nPress any key to continue...\n");
    keyboard_getkey();
}

static void history_check_size(void)
{
    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < fs_dir_cap; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1) return;
    current_dir = sys_dir;

    unsigned int size = fs_get_file_size("history.txt");
    if (size > HISTORY_MAX_BYTES)
    {
        fs_delete("history.txt");
        history_count = 0;
        history_pos = -1;
    }

    current_dir = prev_dir;
}

void history_load(void)
{
    if (!cfg_disk_history) return;

    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < fs_dir_cap; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1) return;
    current_dir = sys_dir;

    history_check_size();

    char buffer[2048];
    if (fs_read_text("history.txt", buffer, sizeof(buffer)))
    {
        int i = 0;
        int line_idx = 0;
        int char_idx = 0;

        while (buffer[i] != '\0' && line_idx < MAX_HISTORY_COMMANDS)
        {
            if (buffer[i] == '\n')
            {
                cmd_history[line_idx][char_idx] = '\0';
                line_idx++;
                char_idx = 0;
            }
            else if (buffer[i] != '\r')
            {
                if (char_idx < MAX_CMD_LENGTH - 1)
                {
                    cmd_history[line_idx][char_idx++] = buffer[i];
                }
            }
            i++;
        }
        history_count = line_idx;
    }

    current_dir = prev_dir;
}

void history_save(void)
{
    if (!cfg_disk_history) return;

    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < fs_dir_cap; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1) return;
    current_dir = sys_dir;

    history_check_size();

    char accum[2048];
    int pos = 0;
    accum[0] = '\0';

    for (int i = 0; i < history_count; i++)
    {
        int j = 0;
        while (cmd_history[i][j] != '\0' && pos < (int)sizeof(accum) - 3)
        {
            accum[pos++] = cmd_history[i][j++];
        }
        accum[pos++] = '\n';
    }
    accum[pos] = '\0';

    if (fs_find("history.txt") == -1)
    {
        fs_create("history.txt");
    }
    fs_write("history.txt", accum);

    current_dir = prev_dir;
}
