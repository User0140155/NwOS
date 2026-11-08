#include "nwos_internal.h"

#define SCRIPT_MAX_VARS       32

static struct
{
    char name[SCRIPT_VAR_NAME];
    char value[SCRIPT_VAR_VALUE];
} script_vars[SCRIPT_MAX_VARS];

static int script_var_count = 0;
int script_depth = 0;
int script_last_status = 0;

int script_command_failed = 0;

void script_clear_vars(void)
{
    for (int i = 0; i < SCRIPT_MAX_VARS; i++)
    {
        script_vars[i].name[0] = '\0';
        script_vars[i].value[0] = '\0';
    }
    script_var_count = 0;
}

const char* script_get_var(const char* name)
{
    for (int i = 0; i < script_var_count; i++)
    {
        if (strcmp(script_vars[i].name, name) == 0)
            return script_vars[i].value;
    }
    return "";
}

int script_set_var(const char* name, const char* value)
{
    int index = -1;
    int name_len = 0;
    int value_len = 0;

    if (!name || !value || !script_is_name_start(name[0]))
        return 0;

    while (name[name_len] != '\0')
    {
        if (!script_is_name_char(name[name_len]) ||
            name_len >= SCRIPT_VAR_NAME - 1)
            return 0;
        name_len++;
    }

    while (value[value_len] != '\0')
    {
        if (value_len >= SCRIPT_VAR_VALUE - 1)
            return 0;
        value_len++;
    }

    for (int i = 0; i < script_var_count; i++)
    {
        if (strcmp(script_vars[i].name, name) == 0)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        if (script_var_count >= SCRIPT_MAX_VARS)
            return 0;
        index = script_var_count++;
    }

    strcpy(script_vars[index].name, name);
    strcpy(script_vars[index].value, value);
    return 1;
}

int script_unset_var(const char* name)
{
    for (int i = 0; i < script_var_count; i++)
    {
        if (strcmp(script_vars[i].name, name) == 0)
        {
            for (int j = i; j < script_var_count - 1; j++)
                script_vars[j] = script_vars[j + 1];
            script_var_count--;
            script_vars[script_var_count].name[0] = '\0';
            script_vars[script_var_count].value[0] = '\0';
            return 1;
        }
    }
    return 1;
}

int script_is_space(char c)
{
    return c == ' ' || c == '\t';
}

int script_is_name_start(char c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_';
}
