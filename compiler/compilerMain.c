#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"
#include "preproc/preproc.h"
#include "diag/diag.h"
#include "set/setfile.h"

#define NWC_MAX_INCLUDE_DIRS 32
#define NWC_MAX_HEADERS      64


static void print_usage(void)
{
    printf("NwC 0.1 - NwOS Compiler\n\n");

    printf("Usage:\n");
    printf("  nwc <input.nw> [-o <output.nwo>] [-I <dir>]\n");
    printf("  nwc -h <header.nwh> [...] -o <headers.set> [-I <dir>]\n");
    printf("\n");

    printf("Binary sets and linking:\n");
    printf("  nwc -h hello.nwh main.nwh -o headers.set\n");
    printf("  link headers.set main.nw utils.nw -o main.nwo\n");
    printf("\n");

    printf("Example:\n");
    printf("  nwc nw\\game.nw -o sys\\game.nwo\n");
}


static int has_suffix(const char* text, const char* suffix)
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


int main(int argc, char** argv)
{
    const char* input_path = NULL;
    const char* output_path = NULL;

    char* headers[NWC_MAX_HEADERS];
    int header_count = 0;

    char* include_dirs[NWC_MAX_INCLUDE_DIRS];
    int include_dir_count = 0;

    char default_output[512];

    int header_mode = 0;
    int errors = 0;

    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            header_mode = 1;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            print_usage();
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            output_path = argv[++i];
        }
        else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc)
        {
            if (include_dir_count < NWC_MAX_INCLUDE_DIRS)
                include_dirs[include_dir_count++] = argv[++i];
            else
            {
                fprintf(stderr, "[ ERROR ]: Too many -I directories\n");
                return 1;
            }
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "[ ERROR ]: Unknown argument '%s'\n", argv[i]);
            print_usage();
            return 1;
        }
        else if (header_mode || has_suffix(argv[i], ".nwh"))
        {
            header_mode = 1;

            if (header_count >= NWC_MAX_HEADERS)
            {
                fprintf(stderr, "[ ERROR ]: Too many headers\n");
                return 1;
            }

            headers[header_count++] = argv[i];
        }
        else if (input_path == NULL)
        {
            input_path = argv[i];
        }
        else
        {
            fprintf(stderr, "[ ERROR ]: Unknown argument '%s'\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    if (header_mode)
    {
        if (header_count == 0)
        {
            fprintf(stderr,
                    "[ ERROR ]: nwc -h needs one or more .nwh headers\n");
            return 1;
        }

        if (output_path == NULL)
        {
            fprintf(stderr,
                    "[ ERROR ]: nwc -h needs -o <file.set>\n");
            return 1;
        }

        printf("NwC 0.1 - binary set\n");
        printf("Headers: %d\n", header_count);
        printf("Output : %s\n\n", output_path);

        if (!nwc_set_write(output_path, headers, header_count,
                           include_dirs, include_dir_count, &errors))
            return 1;

        printf("Created: %s\n", output_path);
        return 0;
    }

    if (input_path == NULL)
    {
        print_usage();
        return 1;
    }

    if (output_path == NULL)
    {
        const char* dot = strrchr(input_path, '.');

        if (dot != NULL)
        {
            size_t base_length = (size_t)(dot - input_path);

            if (base_length >= sizeof(default_output) - 5)
            {
                fprintf(stderr, "[ ERROR ]: Path too long\n");
                return 1;
            }

            memcpy(default_output, input_path, base_length);
            strcpy(default_output + base_length, ".nwo");
        }
        else
        {
            snprintf(default_output, sizeof(default_output),
                     "%s.nwo", input_path);
        }

        output_path = default_output;
    }

    printf("NwC 0.1\n");
    printf("Input : %s\n", input_path);
    printf("Output: %s\n\n", output_path);

    printf("Preprocessing... ");

    {
        char* source = nwc_preproc_read_file(input_path);
        ASTNode* ast;

        ast = nwc_preproc_parse_source(input_path, source, &errors);

        if (ast == NULL || errors != 0)
        {
            printf("FAILED\n");
            ast_free(ast);
            free(source);
            return 1;
        }

        printf("OK\n");
        printf("Codegen...   ");

        codegen_set_diag(nwc_diag_codegen, input_path, source);

        if (!codegen_write_nwo(ast, output_path, 0))
        {
            printf("FAILED\n");
            ast_free(ast);
            free(source);
            return 1;
        }

        printf("OK\n");
        printf("\nCreated: %s\n", output_path);

        ast_free(ast);
        free(source);
    }

    return 0;
}
