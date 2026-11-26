#ifndef NWC_KERNEL_H
#define NWC_KERNEL_H

#define NWC_MAX_SOURCE_SIZE 16384
#define NWC_MAX_NWO_SIZE    131072

/* Limits of the in-kernel compiler driver. The kernel has a fixed arena,
   so unlike the host tools it cannot accept an unlimited number of files. */
#define NWC_KMAX_UNITS      6
#define NWC_KMAX_SETS       4
#define NWC_KMAX_SET_SIZE   16384
#define NWC_KMAX_HEADERS    8
#define NWC_KMAX_PROTOS     16
#define NWC_KMAX_PARAMS     8
#define NWC_KMAX_NAME       32
#define NWC_KMAX_PATH       64

typedef struct
{
    const char* path;
    const char* source;
    unsigned int size;

} NwcKernelSource;

typedef struct
{
    const char* path;
    const unsigned char* data;
    unsigned int size;

} NwcKernelSet;

/* Compiles one in-memory source (used by the editor and by packages). */
int nwc_compile_source(const char *source,
                       unsigned int source_size,
                       unsigned char *nwo_buffer,
                       unsigned int nwo_capacity,
                       unsigned int *nwo_size);

/* Compiles several in-memory .nw sources into one .nwo. */
int nwc_compile_sources(const NwcKernelSource* sources, int source_count,
                        unsigned char* nwo_buffer,
                        unsigned int nwo_capacity,
                        unsigned int* nwo_size);

/* Builds a binary NWSET001 archive from prototype-only .nwh headers. */
int nwc_build_set(const NwcKernelSource* headers, int header_count,
                  unsigned char* set_buffer,
                  unsigned int set_capacity,
                  unsigned int* set_size);

/* Links binary .set archives with .nw implementations into one .nwo. */
int nwc_link_sets(const NwcKernelSet* sets, int set_count,
                  const NwcKernelSource* sources, int source_count,
                  unsigned char* nwo_buffer,
                  unsigned int nwo_capacity,
                  unsigned int* nwo_size);

#endif
