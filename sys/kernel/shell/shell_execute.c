#include "nwos_internal.h"

static void trim_whitespace(char* s);

void cmd_panic_safe(void)
{
    KPANIC_SAFE(ERR_NONE, "Manual test panic (safe)");
}

void cmd_panic_fatal(void)
{
    KPANIC_FATAL(ERR_BAD_STATE, "Manual test panic (fatal)");
}

int command_name_length(const char* s)
{
    int n = 0;

    while (s[n] != '\0')
        n++;

    return n;
}

static void trim_whitespace(char* s)
{
    if (!s)
        return;

    while (*s == ' ' || *s == '\t')
        s++;

    char* p = s;
    while (*p)
        p++;
    while (p > s && (p[-1] == ' ' || p[-1] == '\t'))
        p--;
    *p = '\0';

    if (p == s)
        return;
    char* d = s;
    char* src = s;
    while (*src == ' ' || *src == '\t')
        src++;
    while (*src)
    {
        *d++ = *src++;
    }
    *d = '\0';
}

int shell_execute(const char* command)
{
    char segment[256];
    int length = 0;
    int single_quote = 0;
    int double_quote = 0;
    int escaped = 0;
    int run_next = 1;
    int last_status = 1;

    if (command == 0 || command[0] == '\0')
        return 1;

    for (int i = 0; ; i++)
    {
        char c = command[i];
        int at_end = (c == '\0');
        int separator = 0;
        int is_and = 0;

        if (!at_end && escaped)
        {
            escaped = 0;
        }
        else if (!at_end && c == '\\' && !single_quote)
        {
            escaped = 1;
        }
        else if (!at_end && c == '\'' && !double_quote)
        {
            single_quote = !single_quote;
        }
        else if (!at_end && c == '"' && !single_quote)
        {
            double_quote = !double_quote;
        }
        else if (!single_quote && !double_quote)
        {
            if (at_end || c == ';')
                separator = 1;
            else if (c == '&' && command[i + 1] == '&')
            {
                separator = 1;
                is_and = 1;
            }
        }

        if (separator)
        {
            segment[length] = '\0';
            script_trim(segment);

            if (segment[0] != '\0' && run_next)
            {
                last_status = execute_single_command(segment);
            }

            if (at_end)
                break;

            if (is_and)
            {
                if (!last_status)
                    run_next = 0;
                i++;
            }
            else
            {
                run_next = 1;
            }

            length = 0;
            continue;
        }

        if (!at_end)
        {
            if (length >= (int)sizeof(segment) - 1)
            {
                print_error("Command is too long (maximum 255 characters).\n");
                return 0;
            }
            segment[length++] = c;
        }
        else
        {
            if (single_quote || double_quote)
            {
                print_error("Unclosed quote in command.\n");
                return 0;
            }
            break;
        }
    }

    return last_status;
}
