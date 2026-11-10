#ifndef NWO_LOADER_H
#define NWO_LOADER_H

#define NWO_MAX_SHELL_EXPORTS 16
#define NWO_SHELL_NAME_MAX     32

typedef struct
{
    char name[NWO_SHELL_NAME_MAX];
    char func[NWO_SHELL_NAME_MAX];
} NwoShellExport;

typedef struct
{
    unsigned char version;
    unsigned char flags;

    unsigned int code_size;
    unsigned int data_size;
    unsigned int entry;

    unsigned char* code;
    unsigned char* data;

    unsigned int total_size;

    NwoShellExport exports[NWO_MAX_SHELL_EXPORTS];
    int export_count;
} NwoProgram;

int nwo_load(const char* filename, NwoProgram* program);
const char* nwo_load_error(void);

void nwo_unload(NwoProgram* program);

int nwo_validate(const unsigned char* buffer,
                 unsigned int size);

#endif