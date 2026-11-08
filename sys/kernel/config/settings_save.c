#include "nwos_internal.h"

void settings_save(void)
{
    char buffer[SETTINGS_MAX];
    int pos = 0;
    int prev_dir = current_dir;
    int sys_dir = -1;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (directory[i].used && directory[i].type == FS_TYPE_DIR &&
            directory[i].parent == 0 && strcmp(directory[i].name, "sys") == 0)
        {
            sys_dir = i;
            break;
        }
    }

    if (sys_dir == -1)
        return;

    current_dir = sys_dir;

    buffer[0] = '\0';

    cfg_append(buffer, &pos, sizeof(buffer),
            "name", cfg_user);

    cfg_append(buffer, &pos, sizeof(buffer),
            "OS", cfg_os);

    cfg_append(buffer, &pos, sizeof(buffer),
            "hostname", cfg_hostname);

    cfg_append(buffer, &pos, sizeof(buffer),
            "root_salt", cfg_root_salt);

    cfg_append(buffer, &pos, sizeof(buffer),
            "root_hash", cfg_root_hash);

    cfg_append(buffer, &pos, sizeof(buffer),
            "require_login",
            cfg_require_login ? "1" : "0");

    cfg_append(buffer, &pos, sizeof(buffer),
                "disk_history",
                cfg_disk_history ? "true" : "false");

    cfg_append(buffer, &pos, sizeof(buffer),
            "version", OS_VERSION_STR);

    if (shell_p == 4)
        cfg_append(buffer, &pos, sizeof(buffer),
                "prompt", "unix");
    else if (shell_p == 3)
        cfg_append(buffer, &pos, sizeof(buffer),
                "prompt", "drive");
    else
        cfg_append(buffer, &pos, sizeof(buffer),
                "prompt", invite_user);

    cfg_append(buffer, &pos, sizeof(buffer),
            "prompt_color",
            color_names[prompt_color & 0x0F]);

    cfg_append(buffer, &pos, sizeof(buffer),
            "color",
            color_names[base_color & 0x0F]);

    cfg_append(buffer, &pos, sizeof(buffer),
            "help_color",
            color_names[text_color & 0x0F]);

    cfg_append(buffer, &pos, sizeof(buffer),
            "setup_complete",
            cfg_setup_complete ? "1" : "0");

    if (fs_find(SETTINGS_FILE) == -1)
        fs_create(SETTINGS_FILE);

    fs_write(SETTINGS_FILE, buffer);

    current_dir = prev_dir;
}

void prompt_append(
    char* out,
    int capacity,
    int* pos,
    const char* text
)
{
    while (*text && *pos < capacity - 1)
        out[(*pos)++] = *text++;

    out[*pos] = '\0';
}
