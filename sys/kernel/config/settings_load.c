#include "nwos_internal.h"

void settings_load(void)
{
    char buffer[SETTINGS_MAX];
    char value[80];
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

    if (sys_dir == -1)
        return;

    current_dir = sys_dir;

    if (fs_read_text(SETTINGS_FILE, buffer, sizeof(buffer)))
    {
        if (cfg_find_key(buffer, "name", value, sizeof(value)))
            cfg_strcpy(cfg_user, value, sizeof(cfg_user));

        if (cfg_find_key(buffer, "OS", value, sizeof(value)))
            cfg_strcpy(cfg_os, value, sizeof(cfg_os));

        if (cfg_find_key(buffer, "hostname", value, sizeof(value)))
            cfg_strcpy(cfg_hostname, value, sizeof(cfg_hostname));

        if (cfg_find_key(buffer, "root_salt", value, sizeof(value)))
            cfg_strcpy(cfg_root_salt, value, sizeof(cfg_root_salt));

        if (cfg_find_key(buffer, "root_hash", value, sizeof(value)))
            cfg_strcpy(cfg_root_hash, value, sizeof(cfg_root_hash));

        if (cfg_find_key(buffer, "require_login", value, sizeof(value)))
            cfg_require_login = (strcmp(value, "1") == 0);

        if (cfg_find_key(buffer, "disk_history", value, sizeof(value)))
                    cfg_disk_history = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);

        if (cfg_find_key(buffer, "setup_complete", value, sizeof(value)))
            cfg_setup_complete = (strcmp(value, "1") == 0);

        if (cfg_find_key(buffer, "prompt", value, sizeof(value)))
        {
            cfg_strcpy(invite_user, value, 28);

            if (strcmp(value, "unix") == 0)
                shell_p = 4;
            else if (strcmp(value, "drive") == 0)
                shell_p = 3;
            else if (strcmp(value, "D:/> ") == 0)
                shell_p = 1;
            else if (strcmp(value, "C:/> ") == 0)
                shell_p = 2;
            else if (strcmp(value, OS_NAME "> ") == 0)
                shell_p = 0;
            else
                shell_p = 9;
        }

        if (cfg_find_key(buffer, "prompt_color", value, sizeof(value)))
        {
            prompt_color = color_from_name(value);
            if ((prompt_color & 0x0F) == TERM_COLOR_BLACK)
                prompt_color = TERM_COLOR_CYAN;
        }

        if (cfg_find_key(buffer, "color", value, sizeof(value)))
        {
            base_color = color_from_name(value);
            if ((base_color & 0x0F) == TERM_COLOR_BLACK)
                base_color = TERM_COLOR_LIGHT_GRAY;
        }

        if (cfg_find_key(buffer, "help_color", value, sizeof(value)))
            text_color = color_from_name(value);
    }

    current_dir = prev_dir;
}

void cfg_append(char* out, int* pos, int max, const char* key, const char* value)
{
    int i = 0;

    while (key[i] != '\0' && *pos < max - 1)
        out[(*pos)++] = key[i++];

    if (*pos < max - 1) out[(*pos)++] = '=';

    i = 0;
    while (value[i] != '\0' && *pos < max - 1)
        out[(*pos)++] = value[i++];

    if (*pos < max - 1) out[(*pos)++] = '\n';

    out[*pos] = '\0';
}
