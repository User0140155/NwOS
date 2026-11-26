#include <stdio.h>
#include <string.h>

#include "diag.h"

#define NWC_DIAG_LINE_MAX 512
#define NWC_DIAG_SPACE_MAX 256


static const char* nwc_diag_line(const char* source, int line)
{
    static char buffer[NWC_DIAG_LINE_MAX];
    int current = 1;
    size_t index = 0;

    if (source == NULL || line < 1)
        return "";

    while (source[index] != '\0' && current < line)
    {
        if (source[index] == '\n')
            current++;
        index++;
    }

    if (current != line)
        return "";

    size_t count = 0;

    while (source[index + count] != '\0' &&
           source[index + count] != '\n' &&
           count + 1 < sizeof(buffer))
    {
        buffer[count] = source[index + count];
        count++;
    }

    buffer[count] = '\0';

    if (count > 0 && buffer[count - 1] == '\r')
        buffer[count - 1] = '\0';

    return buffer;
}

static int nwc_diag_digits(int value)
{
    int digits = 1;

    while (value >= 10)
    {
        value /= 10;
        digits++;
    }

    return digits;
}

/* This file is also compiled into the kernel, whose libc has no strstr(). */
static const char* nwc_diag_find(const char* text, const char* token)
{
    size_t token_length;
    size_t index;

    if (text == NULL || token == NULL)
        return NULL;

    token_length = strlen(token);

    if (token_length == 0)
        return text;

    for (index = 0; text[index] != '\0'; index++)
    {
        size_t matched = 0;

        while (matched < token_length &&
               text[index + matched] == token[matched])
            matched++;

        if (matched == token_length)
            return text + index;
    }

    return NULL;
}

static void nwc_diag_print_loc(const NwcDiagLoc* loc)
{
    char spaces[NWC_DIAG_SPACE_MAX];
    int count;
    int length;

    if (loc == NULL)
        return;

    fprintf(stderr, "%s\n", loc->path);
    fprintf(stderr, "%d. %s\n", loc->line,
            nwc_diag_line(loc->source, loc->line));

    count = nwc_diag_digits(loc->line) + 2 + loc->column - 1;

    if (count < 0)
        count = 0;
    if (count >= NWC_DIAG_SPACE_MAX)
        count = NWC_DIAG_SPACE_MAX - 1;

    memset(spaces, ' ', (size_t)count);
    spaces[count] = '\0';
    fprintf(stderr, "%s", spaces);

    length = loc->length > 0 ? loc->length : 1;

    for (int i = 0; i < length; i++)
        fprintf(stderr, "~");

    fprintf(stderr, "\n");
}

static void nwc_diag_print(const char* message, int plain,
                           const NwcDiagLoc* primary,
                           const NwcDiagLoc* secondary,
                           const char* note)
{
    fprintf(stderr, plain ? "[ ERROR ] %s\n" : "[ ERROR ]: %s\n",
            message != NULL ? message : "error");

    if (primary != NULL)
        nwc_diag_print_loc(primary);

    if (secondary != NULL)
        nwc_diag_print_loc(secondary);

    if (note != NULL && note[0] != '\0')
        fprintf(stderr, "Note: %s\n", note);
}

void nwc_diag_error(const char* message,
                    const NwcDiagLoc* primary,
                    const NwcDiagLoc* secondary,
                    const char* note)
{
    nwc_diag_print(message, 0, primary, secondary, note);
}

void nwc_diag_error_plain(const char* message,
                          const NwcDiagLoc* primary,
                          const NwcDiagLoc* secondary,
                          const char* note)
{
    nwc_diag_print(message, 1, primary, secondary, note);
}

void nwc_diag_codegen(const char* path, const char* source,
                      int line, int column, int length,
                      const char* message, int plain)
{
    NwcDiagLoc location;

    location.path = path;
    location.source = source;
    location.line = line;
    location.column = column;
    location.length = length;

    nwc_diag_print(message, plain, &location, NULL, NULL);
}

int nwc_diag_locate(const char* source, int line, const char* token,
                    int* column, int* length)
{
    const char* text;
    const char* found;

    if (column != NULL)
        *column = 1;
    if (length != NULL)
        *length = 1;

    if (source == NULL || token == NULL)
        return 0;

    text = nwc_diag_line(source, line);
    found = nwc_diag_find(text, token);

    if (found == NULL)
        return 0;

    if (column != NULL)
        *column = (int)(found - text) + 1;
    if (length != NULL)
        *length = (int)strlen(token);

    return 1;
}

int nwc_diag_locate_declaration(const char* source, int line,
                                const char* token,
                                int* column, int* length)
{
    const char* text;
    const char* found;
    const char* close;

    if (!nwc_diag_locate(source, line, token, column, length))
        return 0;

    text = nwc_diag_line(source, line);
    found = nwc_diag_find(text, token);
    close = found != NULL ? strchr(found, ')') : NULL;

    if (close != NULL && close > found)
        *length = (int)(close - found);

    return 1;
}
