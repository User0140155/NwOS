#include "nwos_internal.h"

int cmd_sudo = 0;

int normalize_username(char* s)
{
    int n = 0;

    while (s[n] != '\0')
    {
        if (n >= 31)
            return 0;

        if (s[n] >= 'A' && s[n] <= 'Z')
            s[n] = (char)(s[n] - 'A' + 'a');

        char c = s[n];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return 0;

        if (n == 0 && (c == '-' || (c >= '0' && c <= '9')))
            return 0;

        n++;
    }

    if (n == 0 || strcmp(s, "root") == 0)
        return 0;

    return 1;
}

int normalize_hostname(char* s)
{
    int n = 0;

    while (s[n] != '\0')
    {
        if (n >= 63)
            return 0;

        if (s[n] >= 'A' && s[n] <= 'Z')
            s[n] = (char)(s[n] - 'A' + 'a');

        char c = s[n];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '-'))
            return 0;

        if (n == 0 && c == '-')
            return 0;

        n++;
    }

    if (n == 0 || s[n - 1] == '-')
        return 0;

    return 1;
}

int read_password(char* buffer, int max)
{
    int length = 0;

    if (buffer == 0 || max < 2)
        return -1;

    while (1)
    {
        int key = keyboard_getkey();

        if (key == KEY_ENTER)
        {
            buffer[length] = '\0';
            putchar_os('\n');
            return length;
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

        if (key >= 32 && key <= 126 && length < max - 1)
        {
            buffer[length++] = (char)key;
            putchar_os('*');
        }
    }
}
