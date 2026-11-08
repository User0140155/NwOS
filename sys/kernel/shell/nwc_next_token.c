#include "nwos_internal.h"

void cmd_echo(const char* args)
{
    print(args);
    putchar_os('\n');
}

void cmd_touch(const char* args)
{
    const char* p = args ? args : "";
    int files = 0;

    while (*p != '\0')
    {
        char name[MAX_FILENAME];
        int n = 0;
        int too_long = 0;

        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == '\0') break;

        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',')
        {
            if (n < MAX_FILENAME - 1)
                name[n++] = *p;
            else
                too_long = 1;
            p++;
        }
        name[n] = '\0';

        if (n == 0)
            continue;

        files++;

        if (too_long)
        {
            print_error("Filename is too long (maximum 31 characters).\n");
            continue;
        }

        fs_create(name);
    }

    if (files == 0)
        print_error("Usage: touch <file1> [file2 ...]\n");
}

void cmd_write(const char* args)
{
    char name[128];
    int i = 0;

    while (args[i] != ' ' && args[i] != '\0' && i < 127)
    {
        name[i] = args[i];
        i++;
    }

    name[i] = '\0';

    if (args[i] != ' ')
    {
        print_error("Usage: write <file> <text>\n");
        return;
    }

    fs_write(name, args + i + 1);
}

int nwc_next_token(const char* args, int* position,
                          char* output, int output_capacity)
{
    int i = *position;
    int n = 0;
    char quote = 0;

    while (args[i] == ' ' || args[i] == '\t') i++;
    if (args[i] == '\0')
    {
        *position = i;
        return 0;
    }

    if (args[i] == '\'' || args[i] == '"')
        quote = args[i++];

    while (args[i] != '\0')
    {
        if (quote)
        {
            if (args[i] == quote)
            {
                i++;
                quote = 0;
                break;
            }
        }
        else if (args[i] == ' ' || args[i] == '\t')
        {
            break;
        }

        if (n >= output_capacity - 1)
            return -1;
        output[n++] = args[i++];
    }

    if (quote)
        return -1;

    output[n] = '\0';
    while (args[i] == ' ' || args[i] == '\t') i++;
    *position = i;
    return n > 0 ? 1 : -1;
}
