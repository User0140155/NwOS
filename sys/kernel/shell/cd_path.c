#include "nwos_internal.h"
#include "usermode.h"

void print_shell_p(void)
{
    print(shell_prompt_text());
}

void shell(void)
{
    char command[128];

    if ((base_color & 0x0F) == TERM_COLOR_BLACK)
        base_color = TERM_COLOR_LIGHT_GRAY;
    if ((prompt_color & 0x0F) == TERM_COLOR_BLACK)
        prompt_color = TERM_COLOR_CYAN;

    if (!usermode_enter_shell())
        print_error("[WARN] Ring 3 shell unavailable; using kernel shell.\n");

    while (1)
    {
        set_color(prompt_color);
        print_shell_p();
        set_color(base_color);

        console_f7_enabled = 1;
        read_line(command, 128);
        console_f7_enabled = 0;

        shell_execute(command);
    }
}

void cmd_cd_path(const char* args)
{
    char target[256];
    int pos = 0;

    if (args == 0 || args[0] == '\0')
    {
        print_error("Usage: cd <path|~>\n");
        return;
    }

    if (args[0] == '~')
    {
        if (args[1] != '\0' && args[1] != '/')
        {
            print_error("Invalid home path.\n");
            return;
        }

        if (!fs_build_home_path(target, sizeof(target)))
        {
            print_error("Home path is too long.\n");
            return;
        }

        while (target[pos] != '\0')
            pos++;

        for (int i = 1; args[i] != '\0'; i++)
        {
            if (pos >= (int)sizeof(target) - 1)
            {
                print_error("Path is too long.\n");
                return;
            }

            target[pos++] = args[i];
        }

        target[pos] = '\0';
    }
    else
    {
        int i = 0;

        while (args[i] != '\0')
        {
            if (i >= (int)sizeof(target) - 1)
            {
                print_error("Path is too long.\n");
                return;
            }

            target[i] = args[i];
            i++;
        }

        target[i] = '\0';
    }

    int dir = fs_resolve_dir_path(target);

    if (dir < 0)
    {
        print_error("Directory not found: ");
        print(target);
        putchar_os('\n');
        return;
    }

    current_dir = (uint16_t)dir;
}
