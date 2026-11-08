#include "nwos_internal.h"

void cmd_bash(const char* args)
{
    char filename[MAX_FILENAME];
    int i = 0;
    int n = 0;

    if (!args) args = "";
    while (script_is_space(args[i])) i++;
    while (args[i] != '\0' && !script_is_space(args[i]) && n < MAX_FILENAME - 1)
        filename[n++] = args[i++];
    filename[n] = '\0';
    while (script_is_space(args[i])) i++;

    if (n == 0 || args[i] != '\0' ||
        n < 3 || strcmp(filename + n - 3, ".sh") != 0)
    {
        print_error("Usage: bash <file.sh>\n");
        return;
    }

    if (script_run(filename, SCRIPT_MODE_BASH))
        print_success("Script completed.\n");
    else
        print_error("Script failed.\n");
}

void cmd_bat(const char* args)
{
    char filename[MAX_FILENAME];
    int i = 0;
    int n = 0;

    if (!args) args = "";
    while (script_is_space(args[i])) i++;
    while (args[i] != '\0' && !script_is_space(args[i]) && n < MAX_FILENAME - 1)
        filename[n++] = args[i++];
    filename[n] = '\0';
    while (script_is_space(args[i])) i++;

    if (n == 0 || args[i] != '\0' ||
        n < 4 || strcmp(filename + n - 4, ".bat") != 0)
    {
        print_error("Usage: bat <file.bat>\n");
        return;
    }

    if (script_run(filename, SCRIPT_MODE_BAT))
        print_success("Batch file completed.\n");
    else
        print_error("Batch file failed.\n");
}
