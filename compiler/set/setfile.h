#ifndef NWC_SETFILE_H
#define NWC_SETFILE_H

#include "../parser/parser.h"

#define NWC_SET_MAGIC       "NWSET001"
#define NWC_SET_VERSION     1
#define NWC_SET_MAX_NAME    32
#define NWC_SET_MAX_PARAMS  8
#define NWC_SET_MAX_PROTOS  64
#define NWC_SET_MAX_HEADERS 32
#define NWC_SET_PATH_MAX    256

typedef struct
{
    char name[NWC_SET_MAX_NAME];
    ValueType return_type;
    unsigned char param_count;
    unsigned char param_arrays[NWC_SET_MAX_PARAMS];
    ValueType param_types[NWC_SET_MAX_PARAMS];
    char param_names[NWC_SET_MAX_PARAMS][NWC_SET_MAX_NAME];

    int line;
    int column;
    int length;

} NwcSetProto;

typedef struct
{
    char name[NWC_SET_PATH_MAX];
    char* source;

    NwcSetProto protos[NWC_SET_MAX_PROTOS];
    int proto_count;

} NwcSetHeader;

typedef struct
{
    char path[NWC_SET_PATH_MAX];

    NwcSetHeader headers[NWC_SET_MAX_HEADERS];
    int header_count;

} NwcSetArchive;

/* Compiles .nwh files into a binary prototype archive. */
int nwc_set_write(const char* output_path,
                  char** header_paths, int header_count,
                  char** include_dirs, int include_dir_count,
                  int* error_count);

/* Reads a binary prototype archive produced by nwc -h. */
int nwc_set_read(const char* path, NwcSetArchive* archive,
                 int* error_count);

void nwc_set_free(NwcSetArchive* archive);

/* Frees an archive allocated by nwc_set_read(). */
void nwc_set_destroy(NwcSetArchive* archive);

#endif
