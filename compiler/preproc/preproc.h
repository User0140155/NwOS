#ifndef NWC_PREPROC_H
#define NWC_PREPROC_H

#include "../parser/parser.h"

/* Reads a whole source file. The caller owns the returned buffer. */
char* nwc_preproc_read_file(const char* path);

/* Parses one .nw source. Directives are removed while line numbers are kept.
   Prototypes now live only in binary .set archives, so quoted includes are
   ignored here; the link tool supplies implementations explicitly. */
ASTNode* nwc_preproc_parse_source(const char* path, const char* source,
                                  int* error_count);

/* Convenience wrapper for the plain "nwc file.nw" path. Call
   nwc_preproc_release_last_source() after code generation. */
ASTNode* nwc_preproc_build(const char* main_path,
                           char** include_dirs,
                           int include_dir_count,
                           int* error_count);

const char* nwc_preproc_last_source(void);
void nwc_preproc_release_last_source(void);

#endif
