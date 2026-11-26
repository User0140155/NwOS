#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag/diag.h"
#include "codegen/codegen.h"
#include "preproc/preproc.h"
#include "set/setfile.h"

#define LINK_MAX_INPUTS 128
#define LINK_MAX_SETS   64
#define LINK_MAX_SOURCES LINK_MAX_INPUTS


typedef struct
{
    const char* path;
    char* source;
    ASTNode* program;

} SourceUnit;


static void link_usage(void)
{
    printf("NwLink 0.1 - NwOS static linker for .set prototypes\n\n");
    printf("Usage:\n");
    printf("  link <file.set> [...] <file.nw> [...] -o <output.nwo>\n");
    printf("  link [-I <dir>] <file.set> <main.nw> -o <main.nwo>\n\n");
    printf("Build .set archives with: nwc -h a.nwh b.nwh -o headers.set\n");
}

static void link_error(const char* message)
{
    fprintf(stderr, "[ ERROR ]: %s\n", message);
}

static int link_has_suffix(const char* text, const char* suffix)
{
    size_t a = strlen(text);
    size_t b = strlen(suffix);

    if (a < b)
        return 0;

    for (size_t i = 0; i < b; i++)
    {
        char ca = text[a - b + i];
        char cb = suffix[i];

        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');

        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');

        if (ca != cb)
            return 0;
    }

    return 1;
}

static int link_param_count(ASTNode* function)
{
    int count = 0;
    ASTNode* param = function != NULL ? function->left : NULL;

    while (param != NULL)
    {
        count++;
        param = param->next;
    }

    return count;
}

static ASTNode* link_find_function(ASTNode* program, const char* name)
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

static int link_signature_matches(const NwcSetProto* proto,
                                  ASTNode* function)
{
    ASTNode* param;
    int index = 0;

    if (link_param_count(function) != proto->param_count)
        return 0;

    if (proto->return_type != TYPE_UNKNOWN &&
        function->value_type != TYPE_UNKNOWN &&
        proto->return_type != function->value_type)
        return 0;

    param = function->left;

    while (param != NULL)
    {
        unsigned char array = (unsigned char)(
            param->type == AST_ARRAY_DECLARATION || param->array_size > 0);

        if (param->value_type != proto->param_types[index] ||
            array != proto->param_arrays[index])
            return 0;

        index++;
        param = param->next;
    }

    return 1;
}

static void link_set_location(NwcDiagLoc* location,
                              const NwcSetArchive* archive,
                              const NwcSetHeader* header,
                              const NwcSetProto* proto,
                              const char* set_path)
{
    static char display[NWC_SET_PATH_MAX * 2];

    snprintf(display, sizeof(display), "%s | %s",
             set_path != NULL ? set_path : archive->path,
             header->name);

    location->path = display;
    location->source = header->source;
    location->line = proto->line;
    location->column = proto->column;
    location->length = proto->length > 0 ? proto->length : 1;
}

static void link_node_location(NwcDiagLoc* location, ASTNode* node,
                               const char* token)
{
    int column;
    int length;

    location->path = node->source_path != NULL
                         ? node->source_path : "<source>";
    location->source = node->source_text;
    location->line = node->line;

    if (token != NULL &&
        nwc_diag_locate(node->source_text, node->line, token,
                        &column, &length))
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

static int link_check_prototypes(NwcSetArchive** sets, int set_count,
                                 ASTNode* program)
{
    int errors = 0;

    for (int i = 0; i < set_count; i++)
    {
        for (int h = 0; h < sets[i]->header_count; h++)
        {
            NwcSetHeader* header = &sets[i]->headers[h];

            for (int p = 0; p < header->proto_count; p++)
            {
                NwcSetProto* proto = &header->protos[p];
                ASTNode* function = link_find_function(program, proto->name);
                NwcDiagLoc a;
                NwcDiagLoc b;

                if (function == NULL)
                {
                    link_set_location(&a, sets[i], header, proto,
                                      sets[i]->path);

                    nwc_diag_error("Unknown function name",
                                   &a, NULL, NULL);
                    errors++;
                    continue;
                }

                if (!link_signature_matches(proto, function))
                {
                    link_set_location(&a, sets[i], header, proto,
                                      sets[i]->path);
                    link_node_location(&b, function, function->text);

                    nwc_diag_error(
                        "Prototype does not match implementation",
                        &a, &b, NULL);
                    errors++;
                }
            }
        }
    }

    return errors;
}

static int link_check_duplicates(ASTNode* program)
{
    ASTNode* first = program != NULL ? program->child : NULL;
    int errors = 0;
    char note[160];

    while (first != NULL)
    {
        ASTNode* second;

        if (first->type != AST_FUNCTION || first->text == NULL)
        {
            first = first->next;
            continue;
        }

        second = first->next;

        while (second != NULL)
        {
            if (second->type == AST_FUNCTION &&
                second->text != NULL &&
                strcmp(first->text, second->text) == 0)
            {
                /* Duplicates in one file are already reported by the parser. */
                if (first->source_path == NULL ||
                    second->source_path == NULL ||
                    strcmp(first->source_path, second->source_path) != 0)
                {
                    NwcDiagLoc a;
                    NwcDiagLoc b;

                    link_node_location(&a, first, first->text);
                    link_node_location(&b, second, second->text);

                    snprintf(note, sizeof(note),
                             "Help this, Remove one of %s", first->text);

                    nwc_diag_error_plain("Conflicting function names",
                                         &a, &b, note);
                    errors++;
                }
            }

            second = second->next;
        }

        first = first->next;
    }

    return errors;
}

static ASTNode** link_append_program(ASTNode* program, ASTNode** tail)
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

int main(int argc, char** argv)
{
    const char* set_paths[LINK_MAX_SETS];
    const char* source_paths[LINK_MAX_SOURCES];
    int set_count = 0;
    int source_count = 0;
    const char* output = NULL;
    int errors = 0;

    NwcSetArchive* sets[LINK_MAX_SETS];
    SourceUnit units[LINK_MAX_SOURCES];
    ASTNode program;
    ASTNode** tail;

    int set_unit_count = 0;
    int source_unit_count = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0)
        {
            link_usage();
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (++i >= argc)
            {
                link_error("-o needs an output file");
                return 1;
            }

            output = argv[i];
        }
        else if (strcmp(argv[i], "-I") == 0)
        {
            /* Accepted for compatibility; prototypes come from .set files. */
            if (++i >= argc)
            {
                link_error("-I needs a directory");
                return 1;
            }
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "[ ERROR ]: Unknown option '%s'\n", argv[i]);
            link_usage();
            return 1;
        }
        else if (link_has_suffix(argv[i], ".set"))
        {
            if (set_count >= LINK_MAX_SETS)
            {
                link_error("Too many .set inputs");
                return 1;
            }

            set_paths[set_count++] = argv[i];
        }
        else if (link_has_suffix(argv[i], ".nw"))
        {
            if (source_count >= LINK_MAX_SOURCES)
            {
                link_error("Too many .nw inputs");
                return 1;
            }

            source_paths[source_count++] = argv[i];
        }
        else
        {
            fprintf(stderr,
                    "[ ERROR ]: Expected a .set or .nw input, got '%s'\n",
                    argv[i]);
            return 1;
        }
    }

    if (output == NULL)
    {
        link_error("No output file (use -o <file.nwo>)");
        return 1;
    }

    if (source_count == 0)
    {
        link_error("No .nw sources to link");
        return 1;
    }

    memset(&program, 0, sizeof(program));
    program.type = AST_PROGRAM;
    program.line = 1;
    program.column = 1;
    tail = &program.child;

    for (int i = 0; i < set_count; i++)
    {
        int result = 0;

        sets[set_unit_count] =
            (NwcSetArchive*)malloc(sizeof(*sets[set_unit_count]));

        if (sets[set_unit_count] == NULL)
        {
            link_error("Out of memory");
            return 1;
        }

        if (!nwc_set_read(set_paths[i], sets[set_unit_count], &result))
        {
            free(sets[set_unit_count]);
            errors += result > 0 ? result : 1;
            continue;
        }

        set_unit_count++;
    }

    for (int i = 0; i < source_count; i++)
    {
        SourceUnit* unit = &units[source_unit_count];
        int result = 0;

        memset(unit, 0, sizeof(*unit));
        unit->path = source_paths[i];
        unit->source = nwc_preproc_read_file(source_paths[i]);

        if (unit->source == NULL)
        {
            fprintf(stderr, "[ ERROR ]: Cannot read '%s'\n", source_paths[i]);
            errors++;
            continue;
        }

        unit->program = nwc_preproc_parse_source(source_paths[i],
                                                  unit->source,
                                                  &result);

        if (unit->program == NULL)
        {
            errors += result > 0 ? result : 1;
            continue;
        }

        source_unit_count++;
    }

    if (errors == 0)
    {
        for (int i = 0; i < source_unit_count; i++)
            tail = link_append_program(units[i].program, tail);

        errors += link_check_duplicates(&program);
        errors += link_check_prototypes(sets, set_unit_count, &program);
    }

    if (errors == 0)
    {
        codegen_set_diag(nwc_diag_codegen, "<source>", NULL);

        if (!codegen_write_nwo(&program, output, 0))
            errors++;
    }

    for (int i = 0; i < source_unit_count; i++)
    {
        ast_free(units[i].program);
        free(units[i].source);
    }

    for (int i = 0; i < set_unit_count; i++)
        nwc_set_destroy(sets[i]);

    if (errors != 0)
    {
        fprintf(stderr, "[ ERROR ]: Link failed (%d error%s)\n",
                errors, errors == 1 ? "" : "s");
        return 1;
    }

    printf("Linked: %s\n", output);
    return 0;
}
