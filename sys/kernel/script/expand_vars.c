#include "nwos_internal.h"

static void script_status_text(char output[4]);

int script_is_name_char(char c)
{
    return script_is_name_start(c) || (c >= '0' && c <= '9');
}

int script_has_prefix(const char* text, const char* prefix)
{
    while (*prefix)
    {
        char a = *text++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

void script_trim(char* text)
{
    int start = 0;
    int end = 0;
    int out = 0;

    if (!text) return;
    while (script_is_space(text[start])) start++;
    while (text[start + end] != '\0') end++;
    while (end > 0 && script_is_space(text[start + end - 1])) end--;

    while (out < end)
    {
        text[out] = text[start + out];
        out++;
    }
    text[out] = '\0';
}

static void script_status_text(char output[4])
{
    output[0] = script_last_status ? '1' : '0';
    output[1] = '\0';
}

int script_expand_vars(const char* input, char* output,
                              unsigned int output_size, int mode)
{
    unsigned int out = 0;
    unsigned int i = 0;
    int single_quote = 0;
    int double_quote = 0;
    int escaped = 0;

    if (!input || !output || output_size == 0)
        return 0;

    while (input[i] != '\0')
    {
        char var_name[SCRIPT_VAR_NAME];
        unsigned int name_len = 0;
        const char* value = 0;
        int is_variable = 0;
        int can_expand = 1;

        if (mode == SCRIPT_MODE_BASH && escaped)
        {
            escaped = 0;
            can_expand = 0;
        }
        else if (mode == SCRIPT_MODE_BASH && input[i] == '\\' && !single_quote)
        {
            escaped = 1;
        }
        else if (mode == SCRIPT_MODE_BASH && input[i] == '\'' && !double_quote)
        {
            single_quote = !single_quote;
        }
        else if (mode == SCRIPT_MODE_BASH && input[i] == '"' && !single_quote)
        {
            double_quote = !double_quote;
        }

        if (mode == SCRIPT_MODE_BASH && can_expand && !single_quote && input[i] == '$' && input[i + 1] == '?')
        {
            char status_text[4];
            unsigned int k = 0;
            script_status_text(status_text);
            while (status_text[k] != '\0')
            {
                if (out + 1 >= output_size) return 0;
                output[out++] = status_text[k++];
            }
            i += 2;
            continue;
        }
        else if (mode == SCRIPT_MODE_BASH && can_expand && !single_quote && input[i] == '$')
        {
            unsigned int j = i + 1;
            int braced = input[j] == '{';
            if (braced) j++;

            if (script_is_name_start(input[j]))
            {
                while (script_is_name_char(input[j]) &&
                       name_len < SCRIPT_VAR_NAME - 1)
                    var_name[name_len++] = input[j++];

                if (braced)
                {
                    if (input[j] == '}')
                    {
                        j++;
                        is_variable = 1;
                    }
                }
                else
                {
                    is_variable = 1;
                }

                if (is_variable)
                {
                    var_name[name_len] = '\0';
                    value = script_get_var(var_name);
                    i = j;
                }
            }
        }
        else if (mode == SCRIPT_MODE_BAT && input[i] == '%' &&
                 input[i + 1] != '\0')
        {
            unsigned int j = i + 1;
            while (input[j] != '\0' && input[j] != '%' &&
                   name_len < SCRIPT_VAR_NAME - 1)
                var_name[name_len++] = input[j++];

            if (input[j] == '%' && name_len > 0)
            {
                var_name[name_len] = '\0';
                value = script_get_var(var_name);
                i = j + 1;
                is_variable = 1;
            }
        }

        if (is_variable)
        {
            unsigned int k = 0;
            while (value[k] != '\0')
            {
                if (out + 1 >= output_size) return 0;
                output[out++] = value[k++];
            }
            continue;
        }

        if (out + 1 >= output_size) return 0;
        output[out++] = input[i++];
    }

    output[out] = '\0';
    return 1;
}
