#include "nwc_kernel.h"
#include "nwc_runtime.h"
#include "nwc_stdio.h"

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "diag/diag.h"

#include <string.h>

#define NK_SET_MAGIC        "NWSET001"
#define NK_SET_VERSION      1u

#define NK_LINE_MAX         1024
#define NK_SET_NAME_MAX     NWC_KMAX_NAME


typedef struct
{
    char name[NWC_KMAX_NAME];
    unsigned char return_type;
    unsigned char param_count;
    unsigned char param_types[NWC_KMAX_PARAMS];
    unsigned char param_arrays[NWC_KMAX_PARAMS];
    char param_names[NWC_KMAX_PARAMS][NWC_KMAX_NAME];
    int line;
    int column;
    int length;

} NkWProto;


typedef struct
{
    char name[NWC_KMAX_PATH];
    const char* source;
    NkWProto protos[NWC_KMAX_PROTOS];
    int proto_count;

} NkWHeader;


typedef struct
{
    char path[NWC_KMAX_PATH];
    NkWHeader headers[NWC_KMAX_HEADERS];
    int header_count;

} NkWArchive;


/* Token text is copied by the lexer, so one scratch buffer can be reused for
   every source. Keeping all cleaned copies in the arena would exhaust it. */
static char nk_clean_source[NWC_MAX_SOURCE_SIZE + 2];
static char nk_legacy_source[NWC_MAX_SOURCE_SIZE + 1];


static int nk_memcmp(const unsigned char* a, const unsigned char* b,
                     unsigned length)
{
    for (unsigned i = 0; i < length; i++)
    {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }

    return 0;
}


static void nk_copy(char* dst, unsigned capacity, const char* src)
{
    unsigned i = 0;

    if (capacity == 0)
        return;

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1 < capacity)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}


static int nk_has_suffix(const char* text, const char* suffix)
{
    unsigned a = 0;
    unsigned b = 0;

    if (text == NULL || suffix == NULL)
        return 0;

    while (text[a] != '\0') a++;
    while (suffix[b] != '\0') b++;

    if (a < b)
        return 0;

    return strcmp(text + a - b, suffix) == 0;
}


static const char* nk_skip_spaces(const char* text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r')
        text++;

    return text;
}


static void nk_scan_comments(const char* line, int* in_block)
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


static int nk_known_directive(const char* text)
{
    static const char* names[] = {
        "include", "def", "end", "set", "export"
    };
    unsigned i;

    if (*text != '#')
        return 0;

    text++;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        unsigned length = (unsigned)strlen(names[i]);

        if (strncmp(text, names[i], length) == 0 &&
            (text[length] == '\0' || text[length] == ' ' ||
             text[length] == '\t' || text[length] == '\r'))
            return 1;
    }

    return 0;
}


static int nk_read_name(const char** rest, char* out, unsigned capacity)
{
    const char* text = nk_skip_spaces(*rest);
    unsigned i = 0;

    if (!((text[0] >= 'a' && text[0] <= 'z') ||
          (text[0] >= 'A' && text[0] <= 'Z') ||
          text[0] == '_'))
        return 0;

    while (i + 1 < capacity &&
           ((text[i] >= 'a' && text[i] <= 'z') ||
            (text[i] >= 'A' && text[i] <= 'Z') ||
            (text[i] >= '0' && text[i] <= '9') ||
            text[i] == '_'))
    {
        out[i] = text[i];
        i++;
    }

    out[i] = '\0';
    *rest = text + i;
    return 1;
}


static void nk_directive_error(const char* path, const char* source,
                               int line, const char* text,
                               unsigned length)
{
    NwcDiagLoc location;

    location.path = path != NULL ? path : "<source>";
    location.source = source;
    location.line = line > 0 ? line : 1;
    location.column = 1;
    location.length = length > 0 ? (int)length : 1;

    if (text != NULL && source != NULL)
    {
        int column = 1;
        int token_length = 1;

        if (nwc_diag_locate(source, line, text, &column, &token_length))
        {
            location.column = column;
            location.length = token_length;
        }
    }

    nwc_diag_error("Unknown preprocessor directive", &location, NULL, NULL);
}


/* Replaces directive lines with empty lines so tokens keep their original
   line numbers. Quoted includes are intentionally not followed here: the
   in-OS shell loads included headers explicitly before building a .set. */
static void nk_strip_directives(const char* path, const char* source,
                                unsigned size)
{
    const char* cursor = source;
    unsigned used = 0;
    int line_number = 1;
    int in_block_comment = 0;
    unsigned capacity = size + 2;

    if (capacity > sizeof(nk_clean_source))
        capacity = sizeof(nk_clean_source);

    while (*cursor != '\0' && used + 1 < capacity)
    {
        const char* newline = strchr(cursor, '\n');
        unsigned length = newline != NULL
                              ? (unsigned)(newline - cursor)
                              : (unsigned)strlen(cursor);
        char line[NK_LINE_MAX];
        const char* text;
        int comment_at_start = in_block_comment;

        if (length >= sizeof(line))
            length = sizeof(line) - 1;

        memcpy(line, cursor, length);
        line[length] = '\0';

        if (length > 0 && line[length - 1] == '\r')
            line[length - 1] = '\0';

        nk_scan_comments(line, &in_block_comment);
        text = nk_skip_spaces(line);

        if (!comment_at_start && *text == '#')
        {
            if (!nk_known_directive(text))
            {
                const char* name_text = text + 1;
                unsigned token_length = 1;

                while (name_text[token_length] != '\0' &&
                       name_text[token_length] != ' ' &&
                       name_text[token_length] != '\t')
                    token_length++;

                nk_directive_error(path, source, line_number,
                                   name_text, token_length);
            }

            nk_clean_source[used++] = '\n';
        }
        else
        {
            unsigned i;

            for (i = 0; line[i] != '\0' && used + 1 < capacity; i++)
                nk_clean_source[used++] = line[i];

            nk_clean_source[used++] = '\n';
        }

        if (newline == NULL)
            break;

        cursor = newline + 1;
        line_number++;
    }

    nk_clean_source[used] = '\0';
}


static int nk_header_guard(const char* source, char* name, unsigned capacity)
{
    const char* cursor = source;

    if (source == NULL)
        return 0;

    while (*cursor != '\0')
    {
        const char* newline = strchr(cursor, '\n');
        unsigned length = newline != NULL
                              ? (unsigned)(newline - cursor)
                              : (unsigned)strlen(cursor);
        char line[NK_LINE_MAX];
        const char* text;
        const char* rest;

        if (length >= sizeof(line))
            length = sizeof(line) - 1;

        memcpy(line, cursor, length);
        line[length] = '\0';

        if (length > 0 && line[length - 1] == '\r')
            line[length - 1] = '\0';

        text = nk_skip_spaces(line);

        if (*text == '#')
        {
            rest = text + 1;

            if (strncmp(rest, "def", 3) == 0 &&
                (rest[3] == '\0' || rest[3] == ' ' || rest[3] == '\t'))
            {
                rest = nk_skip_spaces(rest + 3);

                if (nk_read_name(&rest, name, capacity))
                    return 1;
            }
        }

        if (newline == NULL)
            break;

        cursor = newline + 1;
    }

    return 0;
}


static char* nk_keep_source(const char* source, unsigned size)
{
    char* copy = (char*)malloc(size + 1);

    if (copy == NULL)
        return NULL;

    if (size > 0)
        memcpy(copy, source, size);

    copy[size] = '\0';
    return copy;
}


static ASTNode* nk_parse_source(const char* path, const char* source,
                                unsigned size)
{
    Lexer lexer;
    Parser parser;
    ASTNode* program;
    char* kept;

    if (source == NULL || size > NWC_MAX_SOURCE_SIZE)
        return NULL;

    kept = nk_keep_source(source, size);

    if (kept == NULL)
        return NULL;

    nk_strip_directives(path, kept, size);

    lexer_init(&lexer, nk_clean_source);
    parser_init(&parser, &lexer);
    parser_set_filename(&parser, path);
    program = parser_parse(&parser);

    parser_set_node_sources(&parser, program, path, kept);

    if (program == NULL || parser.error_count != 0)
    {
        ast_free(program);
        return NULL;
    }

    return program;
}


static ASTNode** nk_program_tail(ASTNode* program)
{
    ASTNode** tail = &program->child;

    while (*tail != NULL)
        tail = &(*tail)->next;

    return tail;
}


static ASTNode** nk_append_program(ASTNode* program, ASTNode** tail)
{
    ASTNode* node = program->child;

    while (node != NULL)
    {
        ASTNode* next = node->next;

        *tail = node;
        tail = &node->next;

        node = next;
    }

    *tail = NULL;
    program->child = NULL;
    return tail;
}


static int nk_emit_nwo(ASTNode* program, unsigned char* buffer,
                       unsigned capacity, unsigned* size)
{
    int generated;

    if (size != NULL)
        *size = 0;

    if (program == NULL || buffer == NULL || capacity == 0)
        return 0;

    codegen_set_diag(nwc_diag_codegen, "<source>", NULL);
    nwc_stdio_begin_output(buffer, capacity, size);
    generated = codegen_write_nwo(program, "memory.nwo", 0);
    nwc_stdio_end_output();

    return generated ? 1 : 0;
}


int nwc_compile_source(const char* source,
                       unsigned int source_size,
                       unsigned char* nwo_buffer,
                       unsigned int nwo_capacity,
                       unsigned int* nwo_size)
{
    NwcKernelSource unit;

    if (nwo_size != NULL)
        *nwo_size = 0;

    if (source == NULL || nwo_buffer == NULL || nwo_capacity == 0)
    {
        printf("NwC: invalid compiler arguments.\n");
        return 0;
    }

    if (source_size > NWC_MAX_SOURCE_SIZE)
    {
        printf("NwC: source is too large for the in-kernel compiler.\n");
        return 0;
    }

    memcpy(nk_legacy_source, source, source_size);
    nk_legacy_source[source_size] = '\0';

    unit.path = "<source>";
    unit.source = nk_legacy_source;
    unit.size = source_size;

    return nwc_compile_sources(&unit, 1,
                               nwo_buffer, nwo_capacity, nwo_size);
}


int nwc_compile_sources(const NwcKernelSource* sources, int source_count,
                        unsigned char* nwo_buffer,
                        unsigned int nwo_capacity,
                        unsigned int* nwo_size)
{
    ASTNode* program = NULL;
    ASTNode** tail = NULL;
    int errors = 0;
    int generated = 0;

    if (nwo_size != NULL)
        *nwo_size = 0;

    if (sources == NULL || source_count < 1 ||
        source_count > NWC_KMAX_UNITS ||
        nwo_buffer == NULL || nwo_capacity == 0)
    {
        printf("NwC: invalid compiler arguments.\n");
        return 0;
    }

    nwc_arena_reset();

    for (int i = 0; i < source_count; i++)
    {
        ASTNode* part;

        if (sources[i].source == NULL ||
            sources[i].size > NWC_MAX_SOURCE_SIZE)
        {
            printf("NwC: source %d is invalid or too large.\n", i + 1);
            goto done;
        }

        part = nk_parse_source(
            sources[i].path != NULL ? sources[i].path : "<source>",
            sources[i].source,
            sources[i].size
        );

        if (part == NULL)
            goto done;

        if (program == NULL)
        {
            program = part;
            tail = nk_program_tail(program);
        }
        else
        {
            tail = nk_append_program(part, tail);
        }
    }

    nwc_parser_check_duplicate_functions(program, &errors);

    if (errors != 0)
        goto done;

    generated = nk_emit_nwo(program, nwo_buffer, nwo_capacity, nwo_size);

done:
    ast_free(program);
    nwc_arena_reset();
    return generated;
}


/* ----- Binary .set archives ------------------------------------------- */

static int nk_put_u8(unsigned char* buffer, unsigned capacity,
                     unsigned* position, unsigned char value)
{
    if (*position >= capacity)
        return 0;

    buffer[(*position)++] = value;
    return 1;
}


static int nk_put_u16(unsigned char* buffer, unsigned capacity,
                      unsigned* position, unsigned value)
{
    if (!nk_put_u8(buffer, capacity, position,
                   (unsigned char)(value & 0xFFu)))
        return 0;

    return nk_put_u8(buffer, capacity, position,
                     (unsigned char)((value >> 8) & 0xFFu));
}


static int nk_put_u32(unsigned char* buffer, unsigned capacity,
                      unsigned* position, unsigned value)
{
    if (!nk_put_u16(buffer, capacity, position, value & 0xFFFFu))
        return 0;

    return nk_put_u16(buffer, capacity, position, value >> 16);
}


static int nk_put_string(unsigned char* buffer, unsigned capacity,
                         unsigned* position, const char* text)
{
    unsigned length = 0;

    if (text != NULL)
        while (text[length] != '\0')
            length++;

    if (!nk_put_u32(buffer, capacity, position, length))
        return 0;

    if (length > 0)
    {
        if (*position + length > capacity)
            return 0;

        memcpy(buffer + *position, text, length);
        *position += length;
    }

    return 1;
}


static int nk_get_u8(const unsigned char* buffer, unsigned size,
                     unsigned* position, unsigned char* value)
{
    if (*position >= size)
        return 0;

    *value = buffer[(*position)++];
    return 1;
}


static int nk_get_u16(const unsigned char* buffer, unsigned size,
                      unsigned* position, unsigned* value)
{
    unsigned char low = 0;
    unsigned char high = 0;

    if (!nk_get_u8(buffer, size, position, &low) ||
        !nk_get_u8(buffer, size, position, &high))
        return 0;

    *value = (unsigned)low | ((unsigned)high << 8);
    return 1;
}


static int nk_get_u32(const unsigned char* buffer, unsigned size,
                      unsigned* position, unsigned* value)
{
    unsigned low = 0;
    unsigned high = 0;

    if (!nk_get_u16(buffer, size, position, &low) ||
        !nk_get_u16(buffer, size, position, &high))
        return 0;

    *value = low | (high << 16);
    return 1;
}


static int nk_get_string(const unsigned char* buffer, unsigned size,
                         unsigned* position, char* out, unsigned capacity)
{
    unsigned length = 0;
    unsigned copy_length;
    unsigned i;

    if (!nk_get_u32(buffer, size, position, &length) || length > 0xFFFFu)
        return 0;

    copy_length = length >= capacity ? capacity - 1 : length;

    if (length > 0)
    {
        if (*position + copy_length > size)
            return 0;

        for (i = 0; i < copy_length; i++)
            out[i] = (char)buffer[(*position)++];
    }

    if (length > copy_length)
    {
        unsigned skip = length - copy_length;

        if (*position + skip > size)
            return 0;

        *position += skip;
    }

    out[copy_length] = '\0';
    return 1;
}


static char* nk_get_owned_string(const unsigned char* buffer, unsigned size,
                                 unsigned* position)
{
    unsigned length = 0;
    char* text;

    if (!nk_get_u32(buffer, size, position, &length) ||
        length > 16u * 1024u * 1024u)
        return NULL;

    text = (char*)malloc(length + 1);

    if (text == NULL)
        return NULL;

    if (length > 0)
    {
        if (*position + length > size)
            return NULL;

        memcpy(text, buffer + *position, length);
        *position += length;
    }

    text[length] = '\0';
    return text;
}


static void nk_fill_proto(NkWProto* proto, ASTNode* node,
                          const char* source)
{
    ASTNode* param;

    memset(proto, 0, sizeof(*proto));
    nk_copy(proto->name, sizeof(proto->name),
            node->text != NULL ? node->text : "");
    proto->return_type = (unsigned char)node->value_type;
    proto->line = node->line;
    proto->column = node->column;
    proto->length = 1;

    if (node->text != NULL && node->text[0] != '\0')
    {
        int column = 1;
        int length = 1;

        if (nwc_diag_locate_declaration(source, node->line, node->text,
                                        &column, &length))
        {
            proto->column = column;
            proto->length = length;
        }
    }

    param = node->left;

    while (param != NULL && proto->param_count < NWC_KMAX_PARAMS)
    {
        int index = proto->param_count++;

        proto->param_types[index] = (unsigned char)param->value_type;
        proto->param_arrays[index] = (unsigned char)(
            param->type == AST_ARRAY_DECLARATION || param->array_size > 0);
        nk_copy(proto->param_names[index], sizeof(proto->param_names[index]),
                param->text != NULL ? param->text : "");

        param = param->next;
    }
}


static int nk_set_write(NkWArchive* archive,
                        unsigned char* buffer, unsigned capacity,
                        unsigned* output_size)
{
    unsigned position = 0;

    if (archive == NULL || buffer == NULL || output_size == NULL)
        return 0;

    *output_size = 0;

    if (position + 8 > capacity)
        return 0;

    memcpy(buffer, NK_SET_MAGIC, 8);
    position = 8;

    if (!nk_put_u32(buffer, capacity, &position, NK_SET_VERSION) ||
        !nk_put_u32(buffer, capacity, &position,
                    (unsigned)archive->header_count))
        return 0;

    for (int i = 0; i < archive->header_count; i++)
    {
        NkWHeader* header = &archive->headers[i];

        if (!nk_put_string(buffer, capacity, &position, header->name) ||
            !nk_put_string(buffer, capacity, &position,
                           header->source != NULL ? header->source : "") ||
            !nk_put_u32(buffer, capacity, &position,
                        (unsigned)header->proto_count))
            return 0;

        for (int j = 0; j < header->proto_count; j++)
        {
            NkWProto* proto = &header->protos[j];

            if (!nk_put_u32(buffer, capacity, &position,
                            (unsigned)proto->line) ||
                !nk_put_u32(buffer, capacity, &position,
                            (unsigned)proto->column) ||
                !nk_put_u32(buffer, capacity, &position,
                            (unsigned)proto->length) ||
                !nk_put_u8(buffer, capacity, &position,
                           proto->return_type) ||
                !nk_put_u8(buffer, capacity, &position,
                           proto->param_count) ||
                !nk_put_string(buffer, capacity, &position, proto->name))
                return 0;

            for (int k = 0; k < (int)proto->param_count; k++)
            {
                if (!nk_put_u8(buffer, capacity, &position,
                               proto->param_types[k]) ||
                    !nk_put_u8(buffer, capacity, &position,
                               proto->param_arrays[k]) ||
                    !nk_put_string(buffer, capacity, &position,
                                   proto->param_names[k]))
                    return 0;
            }
        }
    }

    *output_size = position;
    return 1;
}


int nwc_build_set(const NwcKernelSource* headers, int header_count,
                  unsigned char* set_buffer,
                  unsigned int set_capacity,
                  unsigned int* set_size)
{
    NkWArchive* archive;
    char guards[NWC_KMAX_HEADERS][NWC_KMAX_NAME];
    int guard_count = 0;
    int errors = 0;
    int written = 0;

    if (set_size != NULL)
        *set_size = 0;

    if (headers == NULL || header_count < 1 ||
        header_count > NWC_KMAX_HEADERS ||
        set_buffer == NULL || set_capacity == 0)
    {
        printf("NwC: invalid .set arguments.\n");
        return 0;
    }

    nwc_arena_reset();
    archive = (NkWArchive*)malloc(sizeof(*archive));

    if (archive == NULL)
        return 0;

    memset(archive, 0, sizeof(*archive));
    nk_copy(archive->path, sizeof(archive->path), "<set>");

    for (int i = 0; i < header_count; i++)
    {
        char guard[NWC_KMAX_NAME];
        NkWHeader* header;
        ASTNode* program;
        ASTNode* node;
        char* source_copy;

        if (headers[i].source == NULL ||
            headers[i].size > NWC_MAX_SOURCE_SIZE)
        {
            printf("NwC: header %d is invalid or too large.\n", i + 1);
            errors++;
            continue;
        }

        if (nk_header_guard(headers[i].source, guard, sizeof(guard)))
        {
            int seen = 0;

            for (int g = 0; g < guard_count; g++)
            {
                if (strcmp(guards[g], guard) == 0)
                {
                    seen = 1;
                    break;
                }
            }

            if (seen)
                continue;

            if (guard_count < NWC_KMAX_HEADERS)
            {
                nk_copy(guards[guard_count], sizeof(guards[guard_count]),
                        guard);
                guard_count++;
            }
        }

        if (archive->header_count >= NWC_KMAX_HEADERS)
        {
            printf("NwC: too many headers in one .set.\n");
            errors++;
            continue;
        }

        program = nk_parse_source(
            headers[i].path != NULL ? headers[i].path : "<header>",
            headers[i].source,
            headers[i].size
        );

        if (program == NULL)
        {
            errors++;
            continue;
        }

        source_copy = nk_keep_source(headers[i].source, headers[i].size);

        if (source_copy == NULL)
        {
            ast_free(program);
            errors++;
            continue;
        }

        header = &archive->headers[archive->header_count++];
        nk_copy(header->name, sizeof(header->name),
                headers[i].path != NULL ? headers[i].path : "<header>");
        header->source = source_copy;

        node = program->child;

        while (node != NULL)
        {
            if (node->type == AST_PROTOTYPE && node->text != NULL)
            {
                if (header->proto_count < NWC_KMAX_PROTOS)
                    nk_fill_proto(&header->protos[header->proto_count++],
                                  node, source_copy);
                else
                {
                    printf("NwC: too many prototypes in '%s'.\n",
                           header->name);
                    errors++;
                }
            }
            else if (node->type == AST_FUNCTION)
            {
                NwcDiagLoc location;
                int column = 1;
                int length = 1;

                location.path = header->name;
                location.source = source_copy;
                location.line = node->line > 0 ? node->line : 1;
                location.column = node->column > 0 ? node->column : 1;
                location.length = 1;

                if (node->text != NULL &&
                    nwc_diag_locate(source_copy, location.line,
                                    node->text, &column, &length))
                {
                    location.column = column;
                    location.length = length;
                }

                nwc_diag_error("Only prototypes are allowed in .nwh headers",
                               &location, NULL, NULL);
                errors++;
            }
            else if (node->type != AST_INCLUDE)
            {
                NwcDiagLoc location;

                location.path = header->name;
                location.source = source_copy;
                location.line = node->line > 0 ? node->line : 1;
                location.column = node->column > 0 ? node->column : 1;
                location.length = 1;

                nwc_diag_error("Unexpected statement in .nwh header",
                               &location, NULL, NULL);
                errors++;
            }

            node = node->next;
        }

        ast_free(program);
    }

    if (errors == 0)
        written = nk_set_write(archive, set_buffer, set_capacity, set_size);

    nwc_arena_reset();
    return written;
}


static int nk_set_read(const unsigned char* buffer, unsigned size,
                       NkWArchive* archive, const char* path)
{
    unsigned position = 8;
    unsigned version = 0;
    unsigned header_count = 0;

    memset(archive, 0, sizeof(*archive));
    nk_copy(archive->path, sizeof(archive->path),
            path != NULL ? path : "<set>");

    if (buffer == NULL || size < 16 ||
        nk_memcmp(buffer, (const unsigned char*)NK_SET_MAGIC, 8) != 0 ||
        !nk_get_u32(buffer, size, &position, &version) ||
        version != NK_SET_VERSION ||
        !nk_get_u32(buffer, size, &position, &header_count) ||
        header_count > NWC_KMAX_HEADERS)
    {
        printf("NwC: invalid .set file '%s'.\n",
               path != NULL ? path : "<set>");
        return 0;
    }

    for (unsigned i = 0; i < header_count; i++)
    {
        NkWHeader* header = &archive->headers[i];
        unsigned proto_count = 0;

        if (!nk_get_string(buffer, size, &position, header->name,
                           sizeof(header->name)))
            return 0;

        header->source = nk_get_owned_string(buffer, size, &position);

        if (header->source == NULL ||
            !nk_get_u32(buffer, size, &position, &proto_count) ||
            proto_count > NWC_KMAX_PROTOS)
            return 0;

        header->proto_count = (int)proto_count;
        archive->header_count++;

        for (unsigned j = 0; j < proto_count; j++)
        {
            NkWProto* proto = &header->protos[j];
            unsigned line = 0;
            unsigned column = 0;
            unsigned length = 0;
            unsigned char param_count = 0;

            memset(proto, 0, sizeof(*proto));

            if (!nk_get_u32(buffer, size, &position, &line) ||
                !nk_get_u32(buffer, size, &position, &column) ||
                !nk_get_u32(buffer, size, &position, &length) ||
                !nk_get_u8(buffer, size, &position, &proto->return_type) ||
                !nk_get_u8(buffer, size, &position, &param_count) ||
                param_count > NWC_KMAX_PARAMS ||
                !nk_get_string(buffer, size, &position, proto->name,
                               sizeof(proto->name)))
                return 0;

            proto->line = (int)line;
            proto->column = (int)column;
            proto->length = length > 0 ? (int)length : 1;
            proto->param_count = param_count;

            for (unsigned k = 0; k < param_count; k++)
            {
                if (!nk_get_u8(buffer, size, &position,
                               &proto->param_types[k]) ||
                    !nk_get_u8(buffer, size, &position,
                               &proto->param_arrays[k]) ||
                    !nk_get_string(buffer, size, &position,
                                   proto->param_names[k],
                                   sizeof(proto->param_names[k])))
                    return 0;
            }
        }
    }

    return 1;
}


static ASTNode* nk_find_function(ASTNode* program, const char* name)
{
    ASTNode* node = program != NULL ? program->child : NULL;

    while (node != NULL)
    {
        if (node->type == AST_FUNCTION &&
            node->text != NULL &&
            strcmp(node->text, name) == 0)
            return node;

        node = node->next;
    }

    return NULL;
}


static int nk_signature_matches(const NkWProto* proto, ASTNode* function)
{
    ASTNode* param;
    int count = 0;
    int index = 0;

    param = function->left;

    while (param != NULL)
    {
        count++;
        param = param->next;
    }

    if (count != (int)proto->param_count)
        return 0;

    if (proto->return_type != (unsigned char)TYPE_UNKNOWN &&
        function->value_type != TYPE_UNKNOWN &&
        proto->return_type != (unsigned char)function->value_type)
        return 0;

    param = function->left;

    while (param != NULL)
    {
        unsigned char array = (unsigned char)(
            param->type == AST_ARRAY_DECLARATION || param->array_size > 0);

        if ((unsigned char)param->value_type != proto->param_types[index] ||
            array != proto->param_arrays[index])
            return 0;

        index++;
        param = param->next;
    }

    return 1;
}


static void nk_set_location(NwcDiagLoc* location, const char* set_path,
                            NkWHeader* header, NkWProto* proto)
{
    char* display = (char*)malloc(NWC_KMAX_PATH * 2 + 4);

    if (display != NULL)
    {
        snprintf(display, NWC_KMAX_PATH * 2 + 4, "%s | %s",
                 set_path != NULL ? set_path : "<set>", header->name);
        location->path = display;
    }
    else
    {
        location->path = set_path != NULL ? set_path : "<set>";
    }

    location->source = header->source;
    location->line = proto->line > 0 ? proto->line : 1;
    location->column = proto->column > 0 ? proto->column : 1;
    location->length = proto->length > 0 ? proto->length : 1;
}


static void nk_node_location(NwcDiagLoc* location, ASTNode* node)
{
    int column = 1;
    int length = 1;

    location->path = node->source_path != NULL
                         ? node->source_path : "<source>";
    location->source = node->source_text;
    location->line = node->line > 0 ? node->line : 1;

    if (node->text != NULL &&
        nwc_diag_locate(node->source_text, location->line,
                        node->text, &column, &length))
    {
        location->column = column;
        location->length = length;
    }
    else
    {
        location->column = node->column > 0 ? node->column : 1;
        location->length = 1;
    }
}


static int nk_check_prototypes(NkWArchive* archive, const char* set_path,
                               ASTNode* program)
{
    int errors = 0;

    for (int h = 0; h < archive->header_count; h++)
    {
        NkWHeader* header = &archive->headers[h];

        for (int p = 0; p < header->proto_count; p++)
        {
            NkWProto* proto = &header->protos[p];
            ASTNode* function = nk_find_function(program, proto->name);

            if (function == NULL)
            {
                NwcDiagLoc location;

                nk_set_location(&location, set_path, header, proto);
                nwc_diag_error("Unknown function name", &location,
                               NULL, NULL);
                errors++;
            }
            else if (!nk_signature_matches(proto, function))
            {
                NwcDiagLoc a;
                NwcDiagLoc b;

                nk_set_location(&a, set_path, header, proto);
                nk_node_location(&b, function);
                nwc_diag_error("Prototype does not match implementation",
                               &a, &b, NULL);
                errors++;
            }
        }
    }

    return errors;
}


int nwc_link_sets(const NwcKernelSet* sets, int set_count,
                  const NwcKernelSource* sources, int source_count,
                  unsigned char* nwo_buffer,
                  unsigned int nwo_capacity,
                  unsigned int* nwo_size)
{
    ASTNode* program = NULL;
    ASTNode** tail = NULL;
    NkWArchive* archive;
    int errors = 0;
    int generated = 0;

    if (nwo_size != NULL)
        *nwo_size = 0;

    if (set_count < 0 || set_count > NWC_KMAX_SETS ||
        (set_count > 0 && sets == NULL) ||
        sources == NULL || source_count < 1 ||
        source_count > NWC_KMAX_UNITS ||
        nwo_buffer == NULL || nwo_capacity == 0)
    {
        printf("NwC: invalid link arguments.\n");
        return 0;
    }

    nwc_arena_reset();
    archive = (NkWArchive*)malloc(sizeof(*archive));

    if (archive == NULL)
        goto done;

    for (int i = 0; i < source_count; i++)
    {
        ASTNode* part;

        if (sources[i].source == NULL ||
            sources[i].size > NWC_MAX_SOURCE_SIZE)
        {
            printf("NwC: source %d is invalid or too large.\n", i + 1);
            errors++;
            continue;
        }

        part = nk_parse_source(
            sources[i].path != NULL ? sources[i].path : "<source>",
            sources[i].source,
            sources[i].size
        );

        if (part == NULL)
        {
            errors++;
            continue;
        }

        if (program == NULL)
        {
            program = part;
            tail = nk_program_tail(program);
        }
        else
        {
            tail = nk_append_program(part, tail);
        }
    }

    nwc_parser_check_duplicate_functions(program, &errors);

    for (int i = 0; i < set_count; i++)
    {
        if (!nk_set_read(sets[i].data, sets[i].size, archive, sets[i].path))
        {
            errors++;
            continue;
        }

        errors += nk_check_prototypes(archive, sets[i].path, program);
    }

    if (errors != 0)
        goto done;

    generated = nk_emit_nwo(program, nwo_buffer, nwo_capacity, nwo_size);

done:
    ast_free(program);
    nwc_arena_reset();
    return generated;
}
