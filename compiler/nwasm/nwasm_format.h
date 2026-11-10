#ifndef NWASM_FORMAT_H
#define NWASM_FORMAT_H

#include <stdint.h>

#define NWOBJ_MAGIC          "NWOBJ001"
#define NWOBJ_MAGIC_SIZE     8
#define NWOBJ_VERSION        1

#define NWOBJ_TARGET_ELF64   1
#define NWOBJ_TARGET_NWO64   2

#define NWOBJ_SEC_CODE       0
#define NWOBJ_SEC_DATA       1
#define NWOBJ_SEC_BSS        2
#define NWOBJ_SEC_ABS        3
#define NWOBJ_SEC_UNDEF      0xFF

#define NWOBJ_BIND_LOCAL     0
#define NWOBJ_BIND_GLOBAL    1

#define NWOBJ_RELOC_ABS64    1
#define NWOBJ_RELOC_PC32     2
#define NWOBJ_RELOC_ABS32    3

typedef struct __attribute__((packed))
{
    uint8_t  magic[NWOBJ_MAGIC_SIZE];
    uint32_t version;
    uint32_t target;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t entry_sym;
    uint32_t sym_count;
    uint32_t reloc_count;
    uint32_t symbol_off;
    uint32_t reloc_off;
} NwObjHeader;

typedef struct __attribute__((packed))
{
    uint8_t  section;
    uint8_t  binding;
    uint16_t reserved;
    uint32_t offset;
    uint32_t name_len;
} NwObjSymbol;

typedef struct __attribute__((packed))
{
    uint8_t  section;
    uint8_t  type;
    uint16_t reserved;
    uint32_t offset;
    uint32_t symbol;
    int64_t  addend;
} NwObjReloc;

#define NWO64_MAGIC          "NWO64\0\0\0"
#define NWO64_MAGIC_SIZE     8
#define NWO64_VERSION        1

#define NWO64_RELOC_TYPE_SHIFT 30
#define NWO64_RELOC_OFFSET_MASK 0x3FFFFFFFu
#define NWO64_RELOC_ABS64    1
#define NWO64_RELOC_ABS32    2

typedef struct __attribute__((packed))
{
    uint8_t  magic[NWO64_MAGIC_SIZE];
    uint32_t version;
    uint32_t flags;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t entry;
    uint32_t image_off;
    uint32_t image_size;
    uint32_t data_off;
    uint32_t reloc_count;
    uint32_t reloc_off;
    uint32_t reserved[3];
} Nwo64Header;

#endif
