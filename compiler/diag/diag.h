#ifndef NWC_DIAG_H
#define NWC_DIAG_H

typedef struct
{
    const char* path;
    const char* source;
    int line;
    int column;
    int length;

} NwcDiagLoc;

/* Both print an underlined source snippet. The first uses "[ ERROR ]:",
   the second uses "[ ERROR ]" (no colon). */
void nwc_diag_error(const char* message,
                    const NwcDiagLoc* primary,
                    const NwcDiagLoc* secondary,
                    const char* note);

void nwc_diag_error_plain(const char* message,
                          const NwcDiagLoc* primary,
                          const NwcDiagLoc* secondary,
                          const char* note);

/* Adapters for codegen_set_diag(). */
void nwc_diag_codegen(const char* path, const char* source,
                      int line, int column, int length,
                      const char* message, int plain);

/* Finds a token on the given line and fills column/length for underlining. */
int nwc_diag_locate(const char* source, int line, const char* token,
                    int* column, int* length);

/* Underlines a declaration from its name through the last parameter name,
   excluding the closing parenthesis. */
int nwc_diag_locate_declaration(const char* source, int line,
                                const char* token,
                                int* column, int* length);

#endif
