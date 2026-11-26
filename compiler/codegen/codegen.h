#ifndef NWC_CODEGEN_H
#define NWC_CODEGEN_H

#include "../parser/parser.h"

/* Host tools may install a pretty diagnostic sink. The kernel compiler
   leaves it unset and keeps the plain fprintf diagnostics. */
typedef void (*NwcCodegenDiag)(const char* path, const char* source,
                               int line, int column, int length,
                               const char* message, int plain);

void codegen_set_diag(NwcCodegenDiag diag,
                      const char* path, const char* source);

int codegen_write_nwo(ASTNode* ast, const char* path, int flags);

#endif
