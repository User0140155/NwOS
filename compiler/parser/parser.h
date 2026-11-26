#ifndef NWC_PARSER_H
#define NWC_PARSER_H

#include "../lexer/lexer.h"

typedef enum
{
    AST_PROGRAM = 0,
    AST_INCLUDE,

    AST_FUNCTION,
    AST_BLOCK,

    AST_VARIABLE_DECLARATION,
    AST_ARRAY_DECLARATION,

    AST_RETURN,
    AST_ASSIGNMENT,
    AST_ASM,

    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_DO_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_PASS,
    AST_GLOBAL,

    AST_LOW_CALL,

    AST_OUTPUT,
    AST_INPUT,
    AST_LINE_INPUT,

    AST_EXPRESSION,

    AST_NUMBER,
    AST_FLOAT,
    AST_STRING,
    AST_CHAR,
    AST_BOOLEAN,
    AST_IDENTIFIER,

    AST_BINARY_OPERATION,
    AST_INDEX,
    AST_CALL,
    AST_MEMBER,
    AST_METHOD_CALL,

    /* Declaration of a function implemented in a .set module.
       It carries the signature but no body and emits no code. */
    AST_PROTOTYPE

} ASTNodeType;

typedef enum
{
    TYPE_INT = 0,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_LONG,
    TYPE_LONG_LONG,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_VOID,
    TYPE_GUI,
    TYPE_UNKNOWN

} ValueType;

typedef struct ASTNode
{
    ASTNodeType type;

    int line;
    int column;

    /* Optional host-side source map for pretty diagnostics. Kernel-side
       compilation leaves these pointers NULL. */
    const char* source_path;
    const char* source_text;

    char* text;
    long long integer_value;
    double float_value;

    ValueType value_type;

    unsigned int array_size;
    unsigned int modifiers;

    char* text_color_name;
    char* text_label;

    unsigned int draw_flags;

    struct ASTNode* left;
    struct ASTNode* right;

    struct ASTNode* child;
    struct ASTNode* body;

    struct ASTNode* next;

} ASTNode;

typedef struct
{
    Lexer* lexer;

    const char* filename;

    Token current;
    Token previous;

    int error_count;
    int shift_allowed;

} Parser;

void parser_init(Parser* parser, Lexer* lexer);

void parser_set_filename(Parser* parser, const char* filename);

void parser_set_node_sources(Parser* parser, ASTNode* node,
                             const char* path, const char* source);

ASTNode* parser_parse(Parser* parser);

/* Checks one linked program for duplicate function definitions. */
void nwc_parser_check_duplicate_functions(ASTNode* program,
                                          int* error_count);

void ast_free(ASTNode* node);

void ast_print(ASTNode* node, int depth);

#endif