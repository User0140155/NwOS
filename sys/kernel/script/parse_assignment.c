#include "nwos_internal.h"

void script_strip_bash_comment(char* line)
{
    int single_quote = 0;
    int double_quote = 0;
    int escaped = 0;

    for (int i = 0; line[i] != '\0'; i++)
    {
        char c = line[i];
        if (escaped) { escaped = 0; continue; }
        if (c == '\\' && !single_quote) { escaped = 1; continue; }
        if (c == '\'' && !double_quote) { single_quote = !single_quote; continue; }
        if (c == '"' && !single_quote) { double_quote = !double_quote; continue; }
        if (c == '#' && !single_quote && !double_quote)
        {
            line[i] = '\0';
            return;
        }
    }
}

int script_parse_assignment(char* line, int mode)
{
    char name[SCRIPT_VAR_NAME];
    char raw_value[SCRIPT_VAR_VALUE];
    char expanded[SCRIPT_VAR_VALUE];
    int name_len = 0;
    int value_len = 0;
    int i = 0;
    int is_bat_set = 0;

    if (mode == SCRIPT_MODE_BASH && script_has_prefix(line, "export "))
        i = 7;
    else if (mode == SCRIPT_MODE_BAT && script_has_prefix(line, "set "))
    {
        i = 4;
        is_bat_set = 1;
    }

    if (is_bat_set && line[i] == '"') i++;

    if (!script_is_name_start(line[i]))
        return is_bat_set ? -1 : 0;

    while (script_is_name_char(line[i]) && name_len < SCRIPT_VAR_NAME - 1)
        name[name_len++] = line[i++];
    name[name_len] = '\0';

    if (line[i] != '=')
        return is_bat_set ? -1 : 0;
    i++;

    while (line[i] != '\0' && value_len < SCRIPT_VAR_VALUE - 1)
        raw_value[value_len++] = line[i++];
    if (line[i] != '\0')
    {
        print_error("Script variable value is too long.\n");
        return -1;
    }
    raw_value[value_len] = '\0';

    if (is_bat_set && value_len > 0 && raw_value[value_len - 1] == '"')
        raw_value[--value_len] = '\0';

    if (value_len >= 2 &&
        ((raw_value[0] == '"' && raw_value[value_len - 1] == '"') ||
         (raw_value[0] == '\'' && raw_value[value_len - 1] == '\'')))
    {
        for (int j = 0; j < value_len - 2; j++)
            raw_value[j] = raw_value[j + 1];
        raw_value[value_len - 2] = '\0';
    }

    if (!script_expand_vars(raw_value, expanded, sizeof(expanded), mode))
    {
        print_error("Expanded script variable is too long.\n");
        return -1;
    }

    if (!script_set_var(name, expanded))
    {
        print_error("Invalid variable or variable table is full.\n");
        return -1;
    }

    return 1;
}

void script_echo(char* text)
{
    script_trim(text);
    if (text[0] == '\0')
    {
        putchar_os('\n');
        return;
    }

    int len = 0;
    while (text[len] != '\0') len++;
    if (len >= 2 &&
        ((text[0] == '"' && text[len - 1] == '"') ||
         (text[0] == '\'' && text[len - 1] == '\'')))
    {
        text[len - 1] = '\0';
        text++;
    }

    print(text);
    putchar_os('\n');
}
