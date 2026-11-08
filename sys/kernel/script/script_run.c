#include "nwos_internal.h"

int script_run(const char* filename, int mode)
{
    static char file_buffer[MAX_FILE_SIZE + 1];
    char line[SCRIPT_LINE_MAX];
    char expanded[SCRIPT_LINE_MAX];
    unsigned int pos = 0;
    int line_number = 0;
    int steps = 0;
    int index;
    int result = 1;
    int errexit = 0;
    int explicit_exit = 0;

    if (!filename || filename[0] == '\0')
    {
        print_error(mode == SCRIPT_MODE_BASH ?
            "Usage: bash <file.sh>\n" : "Usage: bat <file.bat>\n");
        return 0;
    }

    if (script_depth >= 1)
    {
        print_error("Nested scripts are not supported yet.\n");
        return 0;
    }

    index = fs_find(filename);
    if (index < 0 || directory[index].type != FS_TYPE_FILE)
    {
        print_error("Script file not found.\n");
        return 0;
    }

    if (!fs_read_text(filename, file_buffer, sizeof(file_buffer)))
    {
        print_error("Could not read script file.\n");
        return 0;
    }

    script_clear_vars();
    script_last_status = 0;
    script_depth++;

    while (file_buffer[pos] != '\0')
    {
        int n = 0;
        int too_long = 0;

        line_number++;
        while (file_buffer[pos] != '\0' &&
               file_buffer[pos] != '\n')
        {
            char c = file_buffer[pos++];
            if (c == '\r') continue;
            if (n < SCRIPT_LINE_MAX - 1)
                line[n++] = c;
            else
                too_long = 1;
        }
        if (file_buffer[pos] == '\n') pos++;
        line[n] = '\0';

        steps++;
        if (steps > SCRIPT_MAX_STEPS)
        {
            print_error("Script stopped: instruction limit exceeded.\n");
            result = 0;
            break;
        }
        if (too_long)
        {
            print_error("Script line is too long at line ");
            print_int(line_number);
            putchar_os('\n');
            result = 0;
            break;
        }

        script_trim(line);
        if (line[0] == '\0') continue;

        if (mode == SCRIPT_MODE_BASH)
        {
            if (line[0] == '#') continue;
            script_strip_bash_comment(line);
            script_trim(line);
            if (line[0] == '\0') continue;

            if (strcmp(line, "set -e") == 0)
            {
                errexit = 1;
                continue;
            }
            if (strcmp(line, "set +e") == 0)
            {
                errexit = 0;
                continue;
            }
        }
        else
        {
            if (line[0] == '@')
            {
                for (int i = 0; line[i] != '\0'; i++) line[i] = line[i + 1];
                script_trim(line);
            }
            if (line[0] == '\0' || line[0] == ':' ||
                script_has_prefix(line, "rem ") ||
                script_has_prefix(line, "rem\t") ||
                strcmp(line, "rem") == 0)
                continue;
        }

        if (mode == SCRIPT_MODE_BAT &&
            (strcmp(line, "echo off") == 0 || strcmp(line, "echo on") == 0))
            continue;

        if (mode == SCRIPT_MODE_BAT && script_has_prefix(line, "set "))
        {
            int parsed = script_parse_assignment(line, mode);
            if (parsed < 0) { result = 0; break; }
            script_last_status = 0;
            continue;
        }

        if (mode == SCRIPT_MODE_BASH)
        {
            if (script_has_prefix(line, "unset "))
            {
                char* name = line + 6;
                script_trim(name);
                if (name[0] == '\0')
                {
                    print_error("Usage: unset NAME\n");
                    result = 0;
                    break;
                }
                script_unset_var(name);
                script_last_status = 0;
                continue;
            }

            if (script_has_prefix(line, "export "))
            {
                char* assignment = line + 7;
                int has_equal = 0;
                for (int i = 0; assignment[i] != '\0'; i++)
                    if (assignment[i] == '=') { has_equal = 1; break; }
                if (!has_equal)
                {
                    script_last_status = 0;
                    continue;
                }
            }

            int parsed = script_parse_assignment(line, mode);
            if (parsed < 0) { result = 0; break; }
            if (parsed > 0)
            {
                script_last_status = 0;
                continue;
            }
        }

        if (strcmp(line, "true") == 0)
        {
            script_last_status = 0;
            continue;
        }
        if (strcmp(line, "false") == 0)
        {
            script_last_status = 1;
            if (errexit) { result = 0; break; }
            continue;
        }

        if (strcmp(line, "exit") == 0 ||
            strcmp(line, "exit 0") == 0 ||
            strcmp(line, "exit /b") == 0 ||
            strcmp(line, "exit /b 0") == 0)
        {
            result = script_last_status == 0;
            explicit_exit = 1;
            break;
        }

        if (script_has_prefix(line, "exit "))
        {
            const char* code = line + 5;
            while (script_is_space(*code)) code++;
            result = (*code == '0' && code[1] == '\0');
            explicit_exit = 1;
            break;
        }

        if (script_has_prefix(line, "echo") &&
            (line[4] == '\0' || script_is_space(line[4])))
        {
            char* text = line + 4;
            while (script_is_space(*text)) text++;
            if (!script_expand_vars(text, expanded, sizeof(expanded), mode))
            {
                print_error("Expanded echo line is too long.\n");
                result = 0;
                break;
            }
            script_echo(expanded);
            script_last_status = 0;
            continue;
        }

        if (!script_expand_vars(line, expanded, sizeof(expanded), mode))
        {
            print_error("Expanded command is too long at line ");
            print_int(line_number);
            putchar_os('\n');
            result = 0;
            break;
        }

        script_last_status = shell_execute(expanded) ? 0 : 1;
        if (script_last_status != 0 && errexit)
        {
            result = 0;
            break;
        }
    }

    if (!explicit_exit && result)
        result = (script_last_status == 0);

    script_depth--;
    script_clear_vars();
    return result;
}
