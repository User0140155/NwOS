#include "nwos_internal.h"

const char* shell_prompt_text(void)
{
    static char drive_prompt[8];
    static char unix_prompt[384];

    if (shell_p == 4)
    {
        char home[80];
        const char* cwd = fs_current_path();
        int pos = 0;

        unix_prompt[0] = '\0';

        if (!fs_build_home_path(home, sizeof(home)))
            cfg_strcpy(home, "/home/user", sizeof(home));

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, cfg_user);

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, "@");

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, cfg_hostname);

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, ":");

        int home_len = 0;

        while (home[home_len] != '\0')
            home_len++;

        if (strncmp(cwd, home, home_len) == 0 &&
            (cwd[home_len] == '\0' || cwd[home_len] == '/'))
        {
            prompt_append(unix_prompt, sizeof(unix_prompt),
                          &pos, "~");

            prompt_append(unix_prompt, sizeof(unix_prompt),
                          &pos, cwd + home_len);
        }
        else
        {
            prompt_append(unix_prompt, sizeof(unix_prompt),
                          &pos, cwd);
        }

        prompt_append(unix_prompt, sizeof(unix_prompt),
                      &pos, "$ ");

        return unix_prompt;
    }

    if (shell_p == 0)
        return OS_NAME "> ";

    if (shell_p == 1)
        return "D:/> ";

    if (shell_p == 2)
        return "C:/> ";

    if (shell_p == 3)
    {
        drive_prompt[0] = fs_get_drive_letter();
        drive_prompt[1] = ':';
        drive_prompt[2] = '/';
        drive_prompt[3] = '>';
        drive_prompt[4] = ' ';
        drive_prompt[5] = '\0';

        return drive_prompt;
    }

    return invite_user;
}

void cmd_settings(void)
{
    print_title("\n=== SETTINGS ===\n\n");
    print("user        = "); print(cfg_user);        putchar_os('\n');
    print("os          = "); print(cfg_os);          putchar_os('\n');
    print("version     = "); print(OS_VERSION_STR);  putchar_os('\n');
    print("prompt      = "); print(shell_prompt_text()); putchar_os('\n');
    print("prompt_color= "); print(color_names[prompt_color & 0x0F]); putchar_os('\n');
    print("color       = "); print(color_names[base_color & 0x0F]);   putchar_os('\n');
    print("help_color  = "); print(color_names[text_color & 0x0F]);   putchar_os('\n');
    print("disk        = "); putchar_os(fs_get_drive_letter());      putchar_os('\n');
    print("hostname    = "); print(cfg_hostname);    putchar_os('\n'); putchar_os('\n');
}
