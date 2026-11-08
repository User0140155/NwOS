#include "nwos_internal.h"

const char* const color_names[16] = {
    "black", "blue", "green", "cyan",
    "red", "magenta", "brown", "light_gray",
    "dark_gray", "light_blue", "light_green", "light_cyan",
    "light_red", "light_magenta", "yellow", "white"
};

void set_p_shell(void)
{
    int choose_p = 0;
    print("1.Default (" OS_NAME "> )\n2.D disk (D:/>)\n3.C disk (C:/>)\n4.Custom\n5.Current disk (\n6.Unix prompt");
    putchar_os(fs_get_drive_letter());
    print(":/>)\n");
    print("|\n");
    print("----> ");
    choose_p = read_int();
    if (choose_p == 1) {
        print_success("Set to 'Default'\n");
        shell_p = 0;
        cfg_strcpy(invite_user, OS_NAME "> ", 28);
    } else if (choose_p == 2) {
        print_success("Set to 'D disk'\n");
        shell_p = 1;
        cfg_strcpy(invite_user, "D:/> ", 28);
    } else if (choose_p == 3) {
        print_success("Set to 'C disk'\n");
        shell_p = 2;
        cfg_strcpy(invite_user, "C:/> ", 28);
    } else if (choose_p == 4) {
        print("Invite: ");
        read_line(invite_user, 28);
        shell_p = 9;
        print_success("Set to custom invite\n");
    } else if (choose_p == 5) {
        print_success("Set to 'Current disk'\n");
        shell_p = 3;
        cfg_strcpy(invite_user, "drive", 28);
    } else if (choose_p == 6) {
        shell_p = 4;
        print_success("Set to 'Unix'\n");
    } else {
        print_error("Invalid choose!\n");
        return;
    }

    settings_save();
}

unsigned char color_from_name(const char* name)
{
    for (int i = 0; i < 16; i++)
    {
        if (strcmp(name, color_names[i]) == 0)
            return (unsigned char)i;
    }

    return TERM_COLOR_LIGHT_GRAY;
}

int cfg_find_key(const char* buffer, const char* key, char* out, int out_max)
{
    int klen = 0;
    while (key[klen] != '\0') klen++;

    int i = 0;
    while (buffer[i] != '\0')
    {
        int start = i;

        while (buffer[i] != '\0' && buffer[i] != '\n')
            i++;

        if (i - start > klen && buffer[start + klen] == '=')
        {
            int match = 1;

            for (int k = 0; k < klen; k++)
            {
                if (buffer[start + k] != key[k])
                {
                    match = 0;
                    break;
                }
            }

            if (match)
            {
                int j = start + klen + 1;
                int o = 0;

                while (j < i && buffer[j] != '\r' && o < out_max - 1)
                    out[o++] = buffer[j++];

                out[o] = '\0';
                return 1;
            }
        }

        if (buffer[i] == '\n') i++;
    }

    return 0;
}
