#include "nwc_kernel.h"
#include "nwc_runtime.h"
#include "nwc_stdio.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"

static char source_buffer[NWC_MAX_SOURCE_SIZE + 1];

int nwc_compile_source(const char *source,
                       unsigned int source_size,
                       unsigned char *nwo_buffer,
                       unsigned int nwo_capacity,
                       unsigned int *nwo_size)
{
    Lexer lexer;
    Parser parser;
    ASTNode *ast;
    int generated;

    if (nwo_size) *nwo_size = 0;
    if (!source || !nwo_buffer || nwo_capacity == 0) {
        printf("NwC: invalid compiler arguments.\n");
        return 0;
    }
    if (source_size > NWC_MAX_SOURCE_SIZE) {
        printf("NwC: source is too large for the in-kernel compiler.\n");
        return 0;
    }

    nwc_arena_reset();
    memcpy(source_buffer, source, source_size);
    source_buffer[source_size] = '\0';
    lexer_init(&lexer, source_buffer);
    parser_init(&parser, &lexer);
    ast = parser_parse(&parser);

    if (!ast || parser.error_count != 0) {
        printf("NwC: parsing failed.\n");
        ast_free(ast);
        nwc_arena_reset();
        return 0;
    }

    nwc_stdio_begin_output(nwo_buffer, nwo_capacity, nwo_size);
    generated = codegen_write_nwo(ast, "memory.nwo", 0);
    nwc_stdio_end_output();

    if (!generated) {
        printf("NwC: code generation failed.\n");
        ast_free(ast);
        nwc_arena_reset();
        return 0;
    }

    ast_free(ast);
    nwc_arena_reset();
    return generated;
}
