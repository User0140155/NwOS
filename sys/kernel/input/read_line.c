#include "nwos_internal.h"

void history_add_command(const char* cmd)
{
    if (cmd == 0 || cmd[0] == '\0') return;

    if (history_count > 0 && strcmp(cmd_history[history_count - 1], cmd) == 0)
        return;

    if (history_count < MAX_HISTORY_COMMANDS)
    {
        strcpy(cmd_history[history_count++], cmd);
    }
    else
    {
        for (int i = 0; i < MAX_HISTORY_COMMANDS - 1; i++)
        {
            strcpy(cmd_history[i], cmd_history[i + 1]);
        }
        strcpy(cmd_history[MAX_HISTORY_COMMANDS - 1], cmd);
    }

    history_pos = history_count;
    history_save();
}

void read_line(char* buffer, int max)
{
    int length = 0;
    history_pos = history_count;

    while (1)
    {
        int key = keyboard_getkey();

        if (console_f7_enabled && key == KEY_F7)
        {
            uint8_t event_modifiers = keyboard_get_last_event_modifiers();

            if (event_modifiers & (KEY_MOD_LSHIFT | KEY_MOD_RSHIFT))
            {

                buffer[length] = '\0';
                putchar_os('\n');
                nw_god_bible_passage();
                set_color(prompt_color);
                print_shell_p();
                set_color(base_color);
                print(buffer);
                continue;
            }

            const char* word = nwf_random_word();
            int start = length;

            while (start > 0 && buffer[start - 1] != ' ')
                start--;

            while (length > start)
            {
                putchar_os('\b');
                length--;
            }

            buffer[length] = '\0';

            int i = 0;
            while (word[i] != '\0' && length < max - 1)
            {
                buffer[length++] = word[i];
                putchar_os(word[i]);
                i++;
            }

            if (length < max - 1)
            {
                buffer[length++] = ' ';
                putchar_os(' ');
            }

            buffer[length] = '\0';
            continue;
        }

        if (key == KEY_TAB)
        {
            for (int i = 0; i < 4; i++)
            {
                if (length >= max - 1)
                    break;

                buffer[length++] = ' ';
                putchar_os(' ');
            }

            continue;
        }

        if (key == KEY_ENTER)
        {
            buffer[length] = '\0';
            putchar_os('\n');

            if (length > 0)
            {
                history_add_command(buffer);
            }
            return;
        }

        if (key == KEY_BACKSPACE)
        {
            if (length > 0)
            {
                length--;
                buffer[length] = '\0';
                putchar_os('\b');
            }

            continue;
        }

        if (key == KEY_UP)
        {
            if (history_count > 0 && history_pos > 0)
            {
                history_pos--;

                while (length > 0)
                {
                    putchar_os('\b');
                    length--;
                }

                int i = 0;
                while (cmd_history[history_pos][i] != '\0' && i < max - 1)
                {
                    buffer[i] = cmd_history[history_pos][i];
                    putchar_os(buffer[i]);
                    i++;
                }
                length = i;
                buffer[length] = '\0';
            }
            continue;
        }

        if (key == KEY_DOWN)
        {
            if (history_count > 0 && history_pos < history_count)
            {
                history_pos++;

                while (length > 0)
                {
                    putchar_os('\b');
                    length--;
                }

                if (history_pos < history_count)
                {
                    int i = 0;
                    while (cmd_history[history_pos][i] != '\0' && i < max - 1)
                    {
                        buffer[i] = cmd_history[history_pos][i];
                        putchar_os(buffer[i]);
                        i++;
                    }
                    length = i;
                    buffer[length] = '\0';
                }
                else
                {
                    buffer[0] = '\0';
                    length = 0;
                }
            }
            continue;
        }

        if (key > 0 && key < 256)
        {
            if (length < max - 1)
            {
                buffer[length] = (char)key;
                length++;
                putchar_os((char)key);
            }
        }
    }
}
