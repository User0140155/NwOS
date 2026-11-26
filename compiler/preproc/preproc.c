#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preproc.h"
#include "../diag/diag.h"

#define PP_MAX_LINE 1024


static char* pp_last_source = NULL;


char* nwc_preproc_read_file(const char* path)
{
    FILE* file;
    long size;
    char* buffer;

    file = fopen(path, "rb");

    if (file == NULL)
        return NULL;

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);

    if (size < 0)
    {
        fclose(file);
        return NULL;
    }

    buffer = (char*)malloc((size_t)size + 1);

    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size)
    {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(file);

    return buffer;
}

const char* nwc_preproc_last_source(void)
{
    return pp_last_source;
}

void nwc_preproc_release_last_source(void)
{
    free(pp_last_source);
    pp_last_source = NULL;
}


/* Tracks block comments so commented-out '#' lines are never directives. */
static void pp_scan_comments(const char* line, int* in_block)
{
    const char* cursor = line;

    while (*cursor != '\0')
    {
        if (*in_block)
        {
            if (cursor[0] == '*' && cursor[1] == '/')
            {
                *in_block = 0;
                cursor += 2;
                continue;
            }
        }
        else
        {
            if (cursor[0] == '/' && cursor[1] == '*')
            {
                *in_block = 1;
                cursor += 2;
                continue;
            }

            if (cursor[0] == '/' && cursor[1] == '/')
                break;
        }

        cursor++;
    }
}

static const char* pp_skip_spaces(const char* text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r')
        text++;

    return text;
}

static int pp_known_directive(const char* line)
{
    static const char* names[] = {
        "include", "def", "end", "set", "export"
    };
    const char* text = pp_skip_spaces(line);
    size_t length;

    if (*text != '#')
        return 0;

    text++;

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        length = strlen(names[i]);

        if (strncmp(text, names[i], length) == 0 &&
            (text[length] == '\0' || text[length] == ' ' ||
             text[length] == '\t' || text[length] == '\r'))
            return 1;
    }

    return 0;
}

static void pp_append(char* buffer, size_t capacity, size_t* used,
                      const char* text, size_t length)
{
    if (*used + length >= capacity)
        length = capacity - *used - 1;

    if (length == 0)
        return;

    memcpy(buffer + *used, text, length);
    *used += length;
}

static char* pp_strip_directives(const char* path, const char* source,
                                 int* error_count)
{
    char* clean;
    size_t capacity = strlen(source) + 2;
    size_t used = 0;
    const char* cursor = source;
    int line_number = 1;
    int in_block_comment = 0;

    clean = (char*)malloc(capacity);

    if (clean == NULL)
        return NULL;

    while (*cursor != '\0')
    {
        const char* newline = strchr(cursor, '\n');
        size_t length = newline != NULL
                            ? (size_t)(newline - cursor)
                            : strlen(cursor);
        char line[PP_MAX_LINE];
        int comment_at_start = in_block_comment;
        const char* text;

        if (length >= sizeof(line))
            length = sizeof(line) - 1;

        memcpy(line, cursor, length);
        line[length] = '\0';

        if (length > 0 && line[length - 1] == '\r')
            line[length - 1] = '\0';

        pp_scan_comments(line, &in_block_comment);
        text = pp_skip_spaces(line);

        if (!comment_at_start && *text == '#')
        {
            if (!pp_known_directive(line))
            {
                NwcDiagLoc location;
                int token_length = 1;

                while (text[token_length] != '\0' &&
                       text[token_length] != ' ' &&
                       text[token_length] != '\t')
                    token_length++;

                location.path = path;
                location.source = source;
                location.line = line_number;
                location.column = (int)(text - line) + 1;
                location.length = token_length;

                nwc_diag_error("Unknown preprocessor directive",
                               &location, NULL, NULL);
                (*error_count)++;
            }

            pp_append(clean, capacity, &used, "\n", 1);
        }
        else
        {
            pp_append(clean, capacity, &used, line, strlen(line));
            pp_append(clean, capacity, &used, "\n", 1);
        }

        if (newline == NULL)
            break;

        cursor = newline + 1;
        line_number++;
    }

    clean[used] = '\0';
    return clean;
}

ASTNode* nwc_preproc_parse_source(const char* path, const char* source,
                                  int* error_count)
{
    Lexer lexer;
    Parser parser;
    ASTNode* program;
    char* clean;
    int errors = 0;

    if (error_count != NULL)
        *error_count = 0;

    if (source == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Cannot read '%s'\n",
                path != NULL ? path : "<source>");

        if (error_count != NULL)
            *error_count = 1;

        return NULL;
    }

    clean = pp_strip_directives(path, source, &errors);

    if (clean == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Out of memory\n");

        if (error_count != NULL)
            *error_count = errors > 0 ? errors : 1;

        return NULL;
    }

    lexer_init(&lexer, clean);
    parser_init(&parser, &lexer);
    parser_set_filename(&parser, path);

    program = parser_parse(&parser);

    errors += parser.error_count;

    /* Diagnostics must show the original file, not the directive-stripped
       copy. Line numbers are identical in both buffers. */
    parser_set_node_sources(&parser, program, path, source);

    if (error_count != NULL)
        *error_count = errors;

    free(clean);

    if (errors != 0)
    {
        ast_free(program);
        return NULL;
    }

    return program;
}

ASTNode* nwc_preproc_build(const char* main_path,
                           char** include_dirs,
                           int include_dir_count,
                           int* error_count)
{
    char* source;
    ASTNode* program;

    (void)include_dirs;
    (void)include_dir_count;

    nwc_preproc_release_last_source();

    source = nwc_preproc_read_file(main_path);

    if (source == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Cannot read '%s'\n", main_path);

        if (error_count != NULL)
            *error_count = 1;

        return NULL;
    }

    pp_last_source = source;
    program = nwc_preproc_parse_source(main_path, source, error_count);

    if (program == NULL)
        nwc_preproc_release_last_source();

    return program;
}
