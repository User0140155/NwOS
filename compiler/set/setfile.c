#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setfile.h"
#include "../diag/diag.h"

#define SET_MAX_LINE 1024
#define SET_MAX_GUARDS 128
#define SET_MAX_LOADED 128
#define SET_MAX_DIRS 32


typedef struct
{
    NwcSetArchive* archive;

    char guards[SET_MAX_GUARDS][NWC_SET_MAX_NAME];
    int guard_count;

    char loaded[SET_MAX_LOADED][NWC_SET_PATH_MAX];
    int loaded_count;

    char dirs[SET_MAX_DIRS][NWC_SET_PATH_MAX];
    int dir_count;

    char main_dir[NWC_SET_PATH_MAX];

    int errors;
} SetContext;


static char* set_read_file(const char* path)
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

static void set_copy_path(char* out, size_t capacity, const char* path)
{
    size_t i = 0;

    while (path[i] != '\0' && i + 1 < capacity)
    {
        out[i] = path[i] == '\\' ? '/' : path[i];
        i++;
    }

    out[i] = '\0';
}

static void set_dir_of(const char* path, char* out, size_t capacity)
{
    size_t length;
    size_t i;

    set_copy_path(out, capacity, path);
    length = strlen(out);

    if (length == 0)
    {
        strcpy(out, ".");
        return;
    }

    for (i = length; i > 0; i--)
    {
        if (out[i - 1] == '/')
        {
            if (i == 1)
                out[0] = '/';
            else
                out[i - 1] = '\0';
            return;
        }
    }

    strcpy(out, ".");
}

static void set_join(char* out, size_t capacity,
                     const char* dir, const char* name)
{
    size_t dir_len;

    if (name[0] == '/')
    {
        set_copy_path(out, capacity, name);
        return;
    }

    if (strcmp(dir, ".") == 0 || dir[0] == '\0')
    {
        set_copy_path(out, capacity, name);
        return;
    }

    set_copy_path(out, capacity, dir);
    dir_len = strlen(out);

    if (dir_len > 0 && out[dir_len - 1] == '/')
        dir_len--;

    if (dir_len + 1 + strlen(name) + 1 > capacity)
    {
        out[0] = '\0';
        return;
    }

    out[dir_len] = '/';
    strcpy(out + dir_len + 1, name);
}

static int set_file_exists(const char* path)
{
    FILE* file;

    if (path == NULL || path[0] == '\0')
        return 0;

    file = fopen(path, "rb");

    if (file == NULL)
        return 0;

    fclose(file);
    return 1;
}

static int set_resolve(SetContext* ctx, const char* from_dir,
                       const char* name, char* out, size_t capacity,
                       const char* requester, int line)
{
    char candidate[NWC_SET_PATH_MAX];

    set_join(candidate, sizeof(candidate), from_dir, name);

    if (set_file_exists(candidate))
    {
        set_copy_path(out, capacity, candidate);
        return 1;
    }

    if (strcmp(from_dir, ctx->main_dir) != 0)
    {
        set_join(candidate, sizeof(candidate), ctx->main_dir, name);

        if (set_file_exists(candidate))
        {
            set_copy_path(out, capacity, candidate);
            return 1;
        }
    }

    for (int i = 0; i < ctx->dir_count; i++)
    {
        set_join(candidate, sizeof(candidate), ctx->dirs[i], name);

        if (set_file_exists(candidate))
        {
            set_copy_path(out, capacity, candidate);
            return 1;
        }
    }

    if (set_file_exists(name))
    {
        set_copy_path(out, capacity, name);
        return 1;
    }

    {
        NwcDiagLoc location;

        location.path = requester;
        location.source = NULL;
        location.line = line > 0 ? line : 1;
        location.column = 1;
        location.length = 1;

        nwc_diag_error("Cannot find included header", &location,
                       NULL, NULL);
    }

    ctx->errors++;
    return 0;
}

static int set_add_loaded(SetContext* ctx, const char* path)
{
    for (int i = 0; i < ctx->loaded_count; i++)
    {
        if (strcmp(ctx->loaded[i], path) == 0)
            return 0;
    }

    if (ctx->loaded_count >= SET_MAX_LOADED)
        return -1;

    strcpy(ctx->loaded[ctx->loaded_count++], path);
    return 1;
}

static int set_find_guard(SetContext* ctx, const char* name)
{
    for (int i = 0; i < ctx->guard_count; i++)
    {
        if (strcmp(ctx->guards[i], name) == 0)
            return i;
    }

    return -1;
}

static void set_define_guard(SetContext* ctx, const char* name)
{
    if (set_find_guard(ctx, name) != -1 ||
        ctx->guard_count >= SET_MAX_GUARDS)
        return;

    strcpy(ctx->guards[ctx->guard_count++], name);
}

static void set_scan_comments(const char* line, int* in_block)
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

static const char* set_skip_spaces(const char* text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r')
        text++;

    return text;
}

static int set_read_name(const char** rest, char* out, size_t capacity)
{
    const char* text = set_skip_spaces(*rest);
    size_t i = 0;

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

static int set_read_path(const char** rest, char* out, size_t capacity)
{
    const char* text = set_skip_spaces(*rest);
    size_t i = 0;

    if (*text == '"')
    {
        text++;

        while (*text != '\0' && *text != '"')
        {
            if (i + 1 >= capacity)
                return 0;

            out[i++] = *text++;
        }

        out[i] = '\0';

        if (*text != '"')
            return 0;

        *rest = text + 1;
        return i != 0;
    }

    while (*text != '\0' && *text != ' ' && *text != '\t' &&
           *text != '\r')
    {
        if (i + 1 >= capacity)
            return 0;

        out[i++] = *text++;
    }

    out[i] = '\0';
    *rest = text;
    return i != 0;
}

static void set_append(char* buffer, size_t capacity, size_t* used,
                       const char* text, size_t length)
{
    if (*used + length >= capacity)
        length = capacity - *used - 1;

    if (length == 0)
        return;

    memcpy(buffer + *used, text, length);
    *used += length;
}

static void set_error_at(const char* path, const char* source,
                         int line, const char* token,
                         const char* message, int* error_count)
{
    NwcDiagLoc location;
    int column = 1;
    int length = 1;

    if (token != NULL && token[0] != '\0' &&
        !nwc_diag_locate(source, line, token, &column, &length))
    {
        column = 1;
        length = 1;
    }

    location.path = path;
    location.source = source;
    location.line = line > 0 ? line : 1;
    location.column = column;
    location.length = length;

    nwc_diag_error(message, &location, NULL, NULL);
    (*error_count)++;
}

static void set_add_proto(NwcSetHeader* header, ASTNode* node,
                          const char* source, int* error_count)
{
    NwcSetProto* proto;
    ASTNode* param;

    if (header->proto_count >= NWC_SET_MAX_PROTOS)
    {
        set_error_at(header->name, source, node->line, node->text,
                     "Too many prototypes in header", error_count);
        return;
    }

    proto = &header->protos[header->proto_count++];

    memset(proto, 0, sizeof(*proto));
    strncpy(proto->name, node->text != NULL ? node->text : "",
            NWC_SET_MAX_NAME - 1);
    proto->return_type = node->value_type;
    proto->line = node->line;
    proto->length = 1;

    if (!nwc_diag_locate_declaration(source, node->line, proto->name,
                                     &proto->column, &proto->length))
    {
        proto->column = node->column;
        proto->length = 1;
    }

    param = node->left;

    while (param != NULL && proto->param_count < NWC_SET_MAX_PARAMS)
    {
        int index = proto->param_count++;

        proto->param_types[index] = param->value_type;
        proto->param_arrays[index] =
            (unsigned char)((param->type == AST_ARRAY_DECLARATION ||
                             param->array_size > 0) ? 1 : 0);
        strncpy(proto->param_names[index],
                param->text != NULL ? param->text : "",
                NWC_SET_MAX_NAME - 1);

        param = param->next;
    }
}

static void set_compile_file(SetContext* ctx, const char* path);

static char* set_strip_directives(SetContext* ctx, const char* path,
                                  const char* source)
{
    char* clean;
    size_t capacity = strlen(source) + 2;
    size_t used = 0;
    const char* cursor = source;
    char file_dir[NWC_SET_PATH_MAX];
    int active_count = 0;
    int skipping = 0;
    int skip_depth = 0;
    int in_block_comment = 0;
    int line_number = 1;

    clean = (char*)malloc(capacity);

    if (clean == NULL)
        return NULL;

    set_dir_of(path, file_dir, sizeof(file_dir));

    while (*cursor != '\0')
    {
        const char* newline = strchr(cursor, '\n');
        size_t length = newline != NULL
                            ? (size_t)(newline - cursor)
                            : strlen(cursor);
        char line[SET_MAX_LINE];
        const char* rest = NULL;
        int comment_at_start = in_block_comment;
        int is_directive = 0;

        if (length >= sizeof(line))
            length = sizeof(line) - 1;

        memcpy(line, cursor, length);
        line[length] = '\0';

        if (length > 0 && line[length - 1] == '\r')
            line[length - 1] = '\0';

        set_scan_comments(line, &in_block_comment);
        rest = set_skip_spaces(line);

        if (!comment_at_start && *rest == '#')
        {
            const char* name_text = rest + 1;
            int token_length = 1;

            is_directive = 1;

            while (name_text[token_length] != '\0' &&
                   name_text[token_length] != ' ' &&
                   name_text[token_length] != '\t')
                token_length++;

            if (strncmp(name_text, "def", 3) == 0 &&
                (name_text[3] == '\0' || name_text[3] == ' ' ||
                 name_text[3] == '\t'))
            {
                char name[NWC_SET_MAX_NAME];

                rest = set_skip_spaces(name_text + 3);

                if (!set_read_name(&rest, name, sizeof(name)))
                {
                    set_error_at(path, source, line_number, "#def",
                                 "Expected a name after #def",
                                 &ctx->errors);
                }
                else if (skipping)
                {
                    skip_depth++;
                }
                else if (set_find_guard(ctx, name) != -1)
                {
                    skipping = 1;
                    skip_depth = 1;
                }
                else
                {
                    set_define_guard(ctx, name);
                    active_count++;
                }
            }
            else if (strncmp(name_text, "end", 3) == 0 &&
                     (name_text[3] == '\0' || name_text[3] == ' ' ||
                      name_text[3] == '\t'))
            {
                if (skipping)
                {
                    skip_depth--;

                    if (skip_depth <= 0)
                        skipping = 0;
                }
                else if (active_count > 0)
                {
                    active_count--;
                }
                else
                {
                    set_error_at(path, source, line_number, "#end",
                                 "#end without matching #def",
                                 &ctx->errors);
                }
            }
            else if (strncmp(name_text, "include", 7) == 0 &&
                     (name_text[7] == '\0' || name_text[7] == ' ' ||
                      name_text[7] == '\t'))
            {
                char child[NWC_SET_PATH_MAX];
                char resolved[NWC_SET_PATH_MAX];

                rest = set_skip_spaces(name_text + 7);

                if (!skipping && *rest == '"')
                {
                    if (!set_read_path(&rest, child, sizeof(child)))
                    {
                        set_error_at(path, source, line_number,
                                     "#include",
                                     "Expected \"file.nwh\" after #include",
                                     &ctx->errors);
                    }
                    else if (set_resolve(ctx, file_dir, child,
                                         resolved, sizeof(resolved),
                                         path, line_number))
                    {
                        set_compile_file(ctx, resolved);
                    }
                }
            }
            else if (strncmp(name_text, "set", 3) != 0 ||
                     (name_text[3] != '\0' && name_text[3] != ' ' &&
                      name_text[3] != '\t'))
            {
                if (strncmp(name_text, "export", 6) != 0 ||
                    (name_text[6] != '\0' && name_text[6] != ' ' &&
                     name_text[6] != '\t'))
                {
                    NwcDiagLoc location;

                    location.path = path;
                    location.source = source;
                    location.line = line_number;
                    location.column = (int)(rest - line) + 1;
                    location.length = token_length;

                    nwc_diag_error("Unknown preprocessor directive",
                                   &location, NULL, NULL);
                    ctx->errors++;
                }
            }
        }

        if (is_directive || skipping)
        {
            set_append(clean, capacity, &used, "\n", 1);
        }
        else
        {
            set_append(clean, capacity, &used, line, strlen(line));
            set_append(clean, capacity, &used, "\n", 1);
        }

        if (newline == NULL)
            break;

        cursor = newline + 1;
        line_number++;
    }

    clean[used] = '\0';
    return clean;
}

static void set_compile_file(SetContext* ctx, const char* path)
{
    NwcSetHeader* header;
    char* source;
    char* clean;
    Lexer lexer;
    Parser parser;
    ASTNode* program;
    ASTNode* node;

    if (set_add_loaded(ctx, path) <= 0)
        return;

    if (ctx->archive->header_count >= NWC_SET_MAX_HEADERS)
    {
        fprintf(stderr, "[ ERROR ]: Too many headers (max %d)\n",
                NWC_SET_MAX_HEADERS);
        ctx->errors++;
        return;
    }

    source = set_read_file(path);

    if (source == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Cannot read header '%s'\n", path);
        ctx->errors++;
        return;
    }

    header = &ctx->archive->headers[ctx->archive->header_count++];
    memset(header, 0, sizeof(*header));
    set_copy_path(header->name, sizeof(header->name), path);
    header->source = source;

    clean = set_strip_directives(ctx, path, source);

    if (clean == NULL)
    {
        ctx->errors++;
        return;
    }

    lexer_init(&lexer, clean);
    parser_init(&parser, &lexer);
    parser_set_filename(&parser, path);

    program = parser_parse(&parser);

    ctx->errors += parser.error_count;

    node = program != NULL ? program->child : NULL;

    while (node != NULL)
    {
        if (node->type == AST_PROTOTYPE && node->text != NULL)
            set_add_proto(header, node, source, &ctx->errors);
        else if (node->type == AST_FUNCTION && node->child != NULL)
            set_error_at(path, source, node->line, node->text,
                         "Only prototypes are allowed in .nwh headers",
                         &ctx->errors);
        else if (node->type != AST_INCLUDE && node->type != AST_PROTOTYPE)
            set_error_at(path, source, node->line, "",
                         "Unexpected statement in .nwh header",
                         &ctx->errors);

        node = node->next;
    }

    ast_free(program);
    free(clean);
}


static void set_emit_u8(FILE* file, unsigned char value)
{
    fputc(value, file);
}

static void set_emit_u16(FILE* file, unsigned int value)
{
    set_emit_u8(file, (unsigned char)(value & 0xFF));
    set_emit_u8(file, (unsigned char)((value >> 8) & 0xFF));
}

static void set_emit_u32(FILE* file, unsigned int value)
{
    set_emit_u16(file, value & 0xFFFF);
    set_emit_u16(file, value >> 16);
}

static void set_emit_string(FILE* file, const char* text)
{
    size_t length = strlen(text);

    if (length > 0xFFFFFFFFu)
        length = 0xFFFFFFFFu;

    set_emit_u32(file, (unsigned int)length);

    if (length > 0)
        fwrite(text, 1, length, file);
}

int nwc_set_write(const char* output_path,
                  char** header_paths, int header_count,
                  char** include_dirs, int include_dir_count,
                  int* error_count)
{
    NwcSetArchive* archive;
    SetContext ctx;
    FILE* file;

    memset(&ctx, 0, sizeof(ctx));

    if (error_count != NULL)
        *error_count = 0;

    if (output_path == NULL || header_paths == NULL || header_count < 1)
    {
        fprintf(stderr,
                "[ ERROR ]: nwc -h needs headers and -o <file.set>\n");
        if (error_count != NULL)
            *error_count = 1;
        return 0;
    }

    archive = (NwcSetArchive*)calloc(1, sizeof(*archive));

    if (archive == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Out of memory\n");

        if (error_count != NULL)
            *error_count = 1;

        return 0;
    }

    set_copy_path(archive->path, sizeof(archive->path), output_path);
    set_dir_of(header_paths[0], ctx.main_dir, sizeof(ctx.main_dir));
    ctx.archive = archive;

    for (int i = 0; i < include_dir_count && i < SET_MAX_DIRS; i++)
        set_copy_path(ctx.dirs[i], sizeof(ctx.dirs[i]), include_dirs[i]);
    ctx.dir_count = include_dir_count < SET_MAX_DIRS
                        ? include_dir_count
                        : SET_MAX_DIRS;

    for (int i = 0; i < header_count; i++)
        set_compile_file(&ctx, header_paths[i]);

    if (ctx.errors != 0)
    {
        nwc_set_destroy(archive);

        if (error_count != NULL)
            *error_count = ctx.errors;

        return 0;
    }

    file = fopen(output_path, "wb");

    if (file == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Cannot create '%s'\n", output_path);
        nwc_set_destroy(archive);

        if (error_count != NULL)
            *error_count = 1;
        return 0;
    }

    fwrite(NWC_SET_MAGIC, 1, 8, file);
    set_emit_u32(file, NWC_SET_VERSION);
    set_emit_u32(file, (unsigned int)archive->header_count);

    for (int i = 0; i < archive->header_count; i++)
    {
        NwcSetHeader* header = &archive->headers[i];

        set_emit_string(file, header->name);
        set_emit_string(file, header->source);
        set_emit_u32(file, (unsigned int)header->proto_count);

        for (int j = 0; j < header->proto_count; j++)
        {
            NwcSetProto* proto = &header->protos[j];

            set_emit_u32(file, (unsigned int)proto->line);
            set_emit_u32(file, (unsigned int)proto->column);
            set_emit_u32(file, (unsigned int)proto->length);
            set_emit_u8(file, (unsigned char)proto->return_type);
            set_emit_u8(file, proto->param_count);
            set_emit_string(file, proto->name);

            for (int k = 0; k < proto->param_count; k++)
            {
                set_emit_u8(file, (unsigned char)proto->param_types[k]);
                set_emit_u8(file, proto->param_arrays[k]);
                set_emit_string(file, proto->param_names[k]);
            }
        }
    }

    fclose(file);
    nwc_set_destroy(archive);

    if (error_count != NULL)
        *error_count = 0;

    return 1;
}


static int set_read_u8(FILE* file, unsigned char* value)
{
    return fread(value, 1, 1, file) == 1;
}

static int set_read_u16(FILE* file, unsigned int* value)
{
    unsigned char bytes[2];

    if (fread(bytes, 1, 2, file) != 2)
        return 0;

    *value = (unsigned int)bytes[0] |
             ((unsigned int)bytes[1] << 8);
    return 1;
}

static int set_read_u32(FILE* file, unsigned int* value)
{
    unsigned int low;
    unsigned int high;

    if (!set_read_u16(file, &low) || !set_read_u16(file, &high))
        return 0;

    *value = low | (high << 16);
    return 1;
}

static int set_read_string(FILE* file, char* out, size_t capacity)
{
    unsigned int length;
    size_t copy_length;

    if (!set_read_u32(file, &length) || length > 0xFFFF)
        return 0;

    copy_length = length >= capacity ? capacity - 1 : length;

    if (length > 0 && fread(out, 1, copy_length, file) != copy_length)
        return 0;

    if (length > copy_length)
    {
        char skip[256];

        while (length > copy_length)
        {
            size_t chunk = length - copy_length;

            if (chunk > sizeof(skip))
                chunk = sizeof(skip);

            if (fread(skip, 1, chunk, file) != chunk)
                return 0;

            length -= chunk;
        }
    }

    out[copy_length] = '\0';
    return 1;
}

static int set_read_file_bytes(FILE* file, char** out)
{
    unsigned int length;
    char* buffer;

    if (!set_read_u32(file, &length) || length > 16u * 1024u * 1024u)
        return 0;

    buffer = (char*)malloc((size_t)length + 1);

    if (buffer == NULL)
        return 0;

    if (length > 0 && fread(buffer, 1, length, file) != length)
    {
        free(buffer);
        return 0;
    }

    buffer[length] = '\0';
    *out = buffer;
    return 1;
}

int nwc_set_read(const char* path, NwcSetArchive* archive,
                 int* error_count)
{
    FILE* file;
    unsigned int version;
    unsigned int header_count;
    char magic[8];

    if (error_count != NULL)
        *error_count = 0;

    memset(archive, 0, sizeof(*archive));
    set_copy_path(archive->path, sizeof(archive->path), path);

    file = fopen(path, "rb");

    if (file == NULL)
    {
        fprintf(stderr, "[ ERROR ]: Cannot open set '%s'\n", path);
        if (error_count != NULL)
            *error_count = 1;
        return 0;
    }

    if (fread(magic, 1, 8, file) != 8 ||
        memcmp(magic, NWC_SET_MAGIC, 8) != 0 ||
        !set_read_u32(file, &version) ||
        version != NWC_SET_VERSION ||
        !set_read_u32(file, &header_count) ||
        header_count > NWC_SET_MAX_HEADERS)
    {
        fprintf(stderr, "[ ERROR ]: Invalid .set file '%s'\n", path);
        fclose(file);
        if (error_count != NULL)
            *error_count = 1;
        return 0;
    }

    for (unsigned int i = 0; i < header_count; i++)
    {
        NwcSetHeader* header = &archive->headers[i];
        unsigned int proto_count;

        if (!set_read_string(file, header->name, NWC_SET_PATH_MAX) ||
            !set_read_file_bytes(file, &header->source) ||
            !set_read_u32(file, &proto_count) ||
            proto_count > NWC_SET_MAX_PROTOS)
        {
            fprintf(stderr, "[ ERROR ]: Corrupted .set file '%s'\n", path);
            fclose(file);
            nwc_set_free(archive);
            if (error_count != NULL)
                *error_count = 1;
            return 0;
        }

        header->proto_count = (int)proto_count;
        archive->header_count++;

        for (unsigned int j = 0; j < proto_count; j++)
        {
            NwcSetProto* proto = &header->protos[j];
            unsigned char type;
            unsigned char param_count;
            unsigned int line;
            unsigned int column;
            unsigned int length;

            if (!set_read_u32(file, &line) ||
                !set_read_u32(file, &column) ||
                !set_read_u32(file, &length) ||
                !set_read_u8(file, &type) ||
                !set_read_u8(file, &param_count) ||
                param_count > NWC_SET_MAX_PARAMS ||
                !set_read_string(file, proto->name, NWC_SET_MAX_NAME))
            {
                fprintf(stderr,
                        "[ ERROR ]: Corrupted .set file '%s'\n", path);
                fclose(file);
                nwc_set_free(archive);
                if (error_count != NULL)
                    *error_count = 1;
                return 0;
            }

            proto->line = (int)line;
            proto->column = (int)column;
            proto->length = (int)length;
            proto->return_type = (ValueType)type;
            proto->param_count = param_count;

            for (int k = 0; k < (int)param_count; k++)
            {
                unsigned char param_type;
                unsigned char param_array;

                if (!set_read_u8(file, &param_type) ||
                    !set_read_u8(file, &param_array) ||
                    !set_read_string(file, proto->param_names[k],
                                     NWC_SET_MAX_NAME))
                {
                    fprintf(stderr,
                            "[ ERROR ]: Corrupted .set file '%s'\n", path);
                    fclose(file);
                    nwc_set_free(archive);
                    if (error_count != NULL)
                        *error_count = 1;
                    return 0;
                }

                proto->param_types[k] = (ValueType)param_type;
                proto->param_arrays[k] = param_array;
            }
        }
    }

    fclose(file);
    return 1;
}

void nwc_set_free(NwcSetArchive* archive)
{
    if (archive == NULL)
        return;

    for (int i = 0; i < archive->header_count; i++)
        free(archive->headers[i].source);

    archive->header_count = 0;
}

void nwc_set_destroy(NwcSetArchive* archive)
{
    if (archive == NULL)
        return;

    nwc_set_free(archive);
    free(archive);
}
