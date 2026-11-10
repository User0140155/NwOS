#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nwasm_format.h"

#define MAX_OBJECTS 32
#define ELF_VADDR   0x400000ull
#define ELF_IMAGE   0x401000ull
#define ELF_OFFSET  0x1000ull

typedef struct
{
    const char    *path;
    NwObjHeader    h;
    uint8_t       *code;
    uint8_t       *data;
    NwObjSymbol   *syms;
    char         **names;
    NwObjReloc    *relocs;
    uint64_t      *values;
    uint8_t       *is_abs;
    uint64_t       code_base;
    uint64_t       data_base;
    uint64_t       bss_base;
    uint64_t       data_off;
} Object;

static void fail(const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "link: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);

    if (!p)
        fail("out of memory");

    return p;
}

static void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count ? count : 1, size ? size : 1);

    if (!p)
        fail("out of memory");

    return p;
}

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    if (alignment <= 1)
        return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *f = fopen(path, "rb");
    long size;
    uint8_t *buffer;

    if (!f)
        fail("cannot open '%s': %s", path, strerror(errno));

    if (fseek(f, 0, SEEK_END) != 0)
        fail("cannot seek '%s'", path);

    size = ftell(f);

    if (size < 0)
        fail("cannot size '%s'", path);

    rewind(f);

    buffer = xmalloc((size_t)size);

    if (size && fread(buffer, 1, (size_t)size, f) != (size_t)size)
        fail("cannot read '%s'", path);

    fclose(f);
    *size_out = (size_t)size;
    return buffer;
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void write_u16(FILE *f, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    fwrite(b, 1, 2, f);
}

static void write_u32(FILE *f, uint32_t v)
{
    uint8_t b[4] = {
        (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)
    };
    fwrite(b, 1, 4, f);
}

static void write_u64(FILE *f, uint64_t v)
{
    uint8_t b[8];

    for (int i = 0; i < 8; ++i)
        b[i] = (uint8_t)(v >> (i * 8));

    fwrite(b, 1, 8, f);
}

static void load_object(Object *obj, const char *path)
{
    size_t size;
    uint8_t *file = read_file(path, &size);
    uint64_t cursor;
    uint64_t needed;

    memset(obj, 0, sizeof(*obj));
    obj->path = path;

    if (size < sizeof(NwObjHeader))
        fail("%s: file is too small", path);

    memcpy(&obj->h, file, sizeof(obj->h));

    if (memcmp(obj->h.magic, NWOBJ_MAGIC, NWOBJ_MAGIC_SIZE) != 0)
        fail("%s: not a NwAsm object", path);

    if (obj->h.version != NWOBJ_VERSION)
        fail("%s: unsupported object version %u", path, obj->h.version);

    if (obj->h.target != NWOBJ_TARGET_ELF64 &&
        obj->h.target != NWOBJ_TARGET_NWO64)
        fail("%s: unsupported target %u", path, obj->h.target);

    needed = sizeof(NwObjHeader) + (uint64_t)obj->h.code_size +
             obj->h.data_size;

    if (needed > size)
        fail("%s: truncated code/data", path);

    if (obj->h.code_size)
    {
        obj->code = xmalloc(obj->h.code_size);
        memcpy(obj->code, file + sizeof(NwObjHeader), obj->h.code_size);
    }

    if (obj->h.data_size)
    {
        obj->data = xmalloc(obj->h.data_size);
        memcpy(obj->data, file + sizeof(NwObjHeader) + obj->h.code_size,
               obj->h.data_size);
    }

    if (obj->h.sym_count)
    {
        obj->syms = xcalloc(obj->h.sym_count, sizeof(NwObjSymbol));
        obj->names = xcalloc(obj->h.sym_count, sizeof(char *));
        obj->values = xcalloc(obj->h.sym_count, sizeof(uint64_t));
        obj->is_abs = xcalloc(obj->h.sym_count, sizeof(uint8_t));
    }

    cursor = obj->h.symbol_off;

    for (uint32_t i = 0; i < obj->h.sym_count; ++i)
    {
        NwObjSymbol sym;

        if (cursor + sizeof(sym) > size)
            fail("%s: truncated symbol table", path);

        memcpy(&sym, file + cursor, sizeof(sym));
        cursor += sizeof(sym);

        if (cursor + sym.name_len > size)
            fail("%s: truncated symbol name", path);

        obj->syms[i] = sym;
        obj->names[i] = xmalloc((size_t)sym.name_len + 1);
        memcpy(obj->names[i], file + cursor, sym.name_len);
        obj->names[i][sym.name_len] = '\0';
        cursor += sym.name_len;
    }

    if (obj->h.reloc_count)
    {
        obj->relocs = xcalloc(obj->h.reloc_count, sizeof(NwObjReloc));

        if (obj->h.reloc_off + (uint64_t)obj->h.reloc_count * sizeof(NwObjReloc)
            > size)
            fail("%s: truncated relocation table", path);

        memcpy(obj->relocs, file + obj->h.reloc_off,
               (size_t)obj->h.reloc_count * sizeof(NwObjReloc));
    }

    free(file);
}

static int find_global(Object *objects, int count, const char *name,
                       int *obj_index, int *sym_index, int skip_obj)
{
    for (int i = 0; i < count; ++i)
    {
        Object *o = &objects[i];

        for (uint32_t j = 0; j < o->h.sym_count; ++j)
        {
            if (i == skip_obj)
                continue;

            if (o->syms[j].section == NWOBJ_SEC_UNDEF)
                continue;

            if (o->syms[j].binding != NWOBJ_BIND_GLOBAL)
                continue;

            if (strcmp(o->names[j], name) != 0)
                continue;

            *obj_index = i;
            *sym_index = (int)j;
            return 1;
        }
    }

    return 0;
}

static int find_any_global(Object *objects, int count, const char *name,
                           int *obj_index, int *sym_index)
{
    return find_global(objects, count, name, obj_index, sym_index, -1);
}

static void resolve_symbols(Object *objects, int count)
{
    for (int i = 0; i < count; ++i)
    {
        Object *o = &objects[i];

        for (uint32_t j = 0; j < o->h.sym_count; ++j)
        {
            if (o->syms[j].section == NWOBJ_SEC_UNDEF)
                continue;

            if (o->syms[j].binding == NWOBJ_BIND_GLOBAL)
            {
                int other_obj;
                int other_sym;

                if (find_global(objects, count, o->names[j],
                                &other_obj, &other_sym, i))
                {
                    fail("duplicate global symbol '%s' in %s and %s",
                         o->names[j], o->path, objects[other_obj].path);
                }
            }
        }
    }

    for (int i = 0; i < count; ++i)
    {
        Object *o = &objects[i];

        for (uint32_t j = 0; j < o->h.sym_count; ++j)
        {
            NwObjSymbol *s = &o->syms[j];

            if (s->section == NWOBJ_SEC_UNDEF)
                continue;

            if (s->section == NWOBJ_SEC_ABS)
            {
                o->values[j] = s->offset;
                o->is_abs[j] = 1;
            }
            else if (s->section == NWOBJ_SEC_CODE)
            {
                o->values[j] = o->code_base + s->offset;
            }
            else if (s->section == NWOBJ_SEC_DATA)
            {
                o->values[j] = o->data_base + s->offset;
            }
            else if (s->section == NWOBJ_SEC_BSS)
            {
                o->values[j] = o->bss_base + s->offset;
            }
            else
            {
                fail("%s: symbol '%s' has invalid section %u",
                     o->path, o->names[j], s->section);
            }
        }
    }

    for (int i = 0; i < count; ++i)
    {
        Object *o = &objects[i];

        for (uint32_t j = 0; j < o->h.sym_count; ++j)
        {
            NwObjSymbol *s = &o->syms[j];
            int obj_index;
            int sym_index;

            if (s->section != NWOBJ_SEC_UNDEF)
                continue;

            if (!find_any_global(objects, count, o->names[j],
                                 &obj_index, &sym_index))
                fail("%s: undefined symbol '%s'", o->path, o->names[j]);

            o->values[j] = objects[obj_index].values[sym_index];
            o->is_abs[j] = objects[obj_index].is_abs[sym_index];
        }
    }
}

static uint8_t *build_image(Object *objects, int count, uint64_t image_size)
{
    uint8_t *image = xcalloc(1, (size_t)image_size);

    for (int i = 0; i < count; ++i)
    {
        Object *o = &objects[i];

        if (o->h.code_size)
            memcpy(image + o->code_base, o->code, o->h.code_size);

        if (o->h.data_size)
            memcpy(image + o->data_base, o->data, o->h.data_size);
    }

    return image;
}

static void write_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_le64(uint8_t *p, uint64_t value)
{
    for (int i = 0; i < 8; ++i)
        p[i] = (uint8_t)(value >> (i * 8));
}

static void apply_relocations(Object *objects, int count, uint8_t *image,
                              uint64_t image_size, int target,
                              uint32_t **nwo_relocs_out,
                              uint32_t *nwo_reloc_count_out)
{
    uint32_t *nwo = NULL;
    uint32_t nwo_count = 0;
    uint32_t nwo_cap = 0;

    for (int i = 0; i < count; ++i)
    {
        Object *o = &objects[i];

        for (uint32_t r = 0; r < o->h.reloc_count; ++r)
        {
            NwObjReloc *rel = &o->relocs[r];
            uint64_t section_base;
            uint64_t section_size;
            uint64_t place;
            uint64_t sym_value;
            int is_abs;

            if (rel->section == NWOBJ_SEC_CODE)
            {
                section_base = o->code_base;
                section_size = o->h.code_size;
            }
            else if (rel->section == NWOBJ_SEC_DATA)
            {
                section_base = o->data_base;
                section_size = o->h.data_size;
            }
            else
            {
                fail("%s: invalid relocation section %u",
                     o->path, rel->section);
            }

            if (rel->symbol >= o->h.sym_count)
                fail("%s: relocation references invalid symbol %u",
                     o->path, rel->symbol);

            if (rel->type != NWOBJ_RELOC_ABS64 &&
                rel->type != NWOBJ_RELOC_ABS32 &&
                rel->type != NWOBJ_RELOC_PC32)
                fail("%s: invalid relocation type %u",
                     o->path, rel->type);

            place = section_base + rel->offset;
            sym_value = o->values[rel->symbol] + (uint64_t)rel->addend;
            is_abs = o->is_abs[rel->symbol];

            if (rel->type == NWOBJ_RELOC_PC32)
            {
                int64_t disp = (int64_t)sym_value -
                               (int64_t)(place + 4);
                int32_t value = (int32_t)disp;

                if (place + 4 > image_size || rel->offset + 4 > section_size)
                    fail("%s: PC32 relocation is out of range", o->path);

                if ((int64_t)value != disp)
                    fail("%s: PC32 relocation overflow", o->path);

                write_le32(image + place, (uint32_t)value);
            }
            else
            {
                int width = rel->type == NWOBJ_RELOC_ABS64 ? 8 : 4;
                uint64_t value = sym_value;

                if (target == NWOBJ_TARGET_ELF64)
                    value += ELF_IMAGE;

                if (place + (uint64_t)width > image_size ||
                    rel->offset + (uint64_t)width > section_size)
                    fail("%s: absolute relocation is out of range", o->path);

                if (width == 8)
                {
                    write_le64(image + place, value);
                }
                else
                {
                    if (target == NWOBJ_TARGET_ELF64 &&
                        value > UINT32_MAX)
                        fail("%s: ABS32 relocation overflow", o->path);

                    write_le32(image + place, (uint32_t)value);
                }

                if (target == NWOBJ_TARGET_NWO64 && !is_abs)
                {
                    if (nwo_count == nwo_cap)
                    {
                        nwo_cap = nwo_cap ? nwo_cap * 2 : 16;
                        nwo = realloc(nwo, nwo_cap * sizeof(uint32_t));

                        if (!nwo)
                            fail("out of memory");
                    }

                    if (place > NWO64_RELOC_OFFSET_MASK)
                        fail("%s: relocation offset is too large", o->path);

                    nwo[nwo_count++] = ((uint32_t)(width == 8
                                                   ? NWO64_RELOC_ABS64
                                                   : NWO64_RELOC_ABS32)
                                        << NWO64_RELOC_TYPE_SHIFT) |
                                       (uint32_t)place;
                }
            }
        }
    }

    *nwo_relocs_out = nwo;
    *nwo_reloc_count_out = nwo_count;
}

static void write_nwo64(const char *path, Object *objects, int count,
                        uint8_t *image, uint64_t image_size,
                        uint64_t data_off, uint64_t code_size,
                        uint64_t data_size, uint64_t bss_size,
                        uint64_t entry, uint32_t *relocs,
                        uint32_t reloc_count)
{
    Nwo64Header h;
    FILE *f;
    uint64_t image_off = align_up(sizeof(h) +
                                  (uint64_t)reloc_count * sizeof(uint32_t), 16);

    (void)objects;
    (void)count;

    memset(&h, 0, sizeof(h));
    memcpy(h.magic, NWO64_MAGIC, NWO64_MAGIC_SIZE);
    h.version = NWO64_VERSION;
    h.flags = 0;
    h.code_size = (uint32_t)code_size;
    h.data_size = (uint32_t)data_size;
    h.bss_size = (uint32_t)bss_size;
    h.entry = (uint32_t)entry;
    h.image_off = (uint32_t)image_off;
    h.image_size = (uint32_t)image_size;
    h.data_off = (uint32_t)data_off;
    h.reloc_count = reloc_count;
    h.reloc_off = (uint32_t)sizeof(h);

    f = fopen(path, "wb");

    if (!f)
        fail("cannot create '%s': %s", path, strerror(errno));

    if (fwrite(&h, sizeof(h), 1, f) != 1)
        fail("cannot write '%s'", path);

    for (uint32_t i = 0; i < reloc_count; ++i)
        write_u32(f, relocs[i]);

    uint64_t cursor = sizeof(h) + (uint64_t)reloc_count * sizeof(uint32_t);

    while (cursor < image_off)
    {
        fputc(0, f);
        ++cursor;
    }

    if (image_size &&
        fwrite(image, 1, (size_t)image_size, f) != (size_t)image_size)
        fail("cannot write '%s'", path);

    if (fclose(f) != 0)
        fail("cannot close '%s'", path);
}

static void write_elf64(const char *path, uint8_t *image, uint64_t image_size,
                        uint64_t bss_size, uint64_t entry)
{
    FILE *f;
    uint64_t filesz = ELF_OFFSET + image_size;
    uint64_t memsz = filesz + bss_size;
    uint64_t cursor;

    f = fopen(path, "wb");

    if (!f)
        fail("cannot create '%s': %s", path, strerror(errno));

    uint8_t ident[16] = {
        0x7F, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    fwrite(ident, 1, sizeof(ident), f);
    write_u16(f, 2);
    write_u16(f, 62);
    write_u32(f, 1);
    write_u64(f, ELF_IMAGE + entry);
    write_u64(f, 64);
    write_u64(f, 0);
    write_u32(f, 0);
    write_u16(f, 64);
    write_u16(f, 56);
    write_u16(f, 1);
    write_u16(f, 64);
    write_u16(f, 0);
    write_u16(f, 0);

    write_u32(f, 1);
    write_u32(f, 7);
    write_u64(f, 0);
    write_u64(f, ELF_VADDR);
    write_u64(f, ELF_VADDR);
    write_u64(f, filesz);
    write_u64(f, memsz);
    write_u64(f, 0x1000);

    cursor = 64 + 56;

    while (cursor < ELF_OFFSET)
    {
        fputc(0, f);
        ++cursor;
    }

    if (image_size &&
        fwrite(image, 1, (size_t)image_size, f) != (size_t)image_size)
        fail("cannot write '%s'", path);

    if (fclose(f) != 0)
        fail("cannot close '%s'", path);

    if (chmod(path, 0755) != 0)
        fail("cannot chmod '%s': %s", path, strerror(errno));
}

static int has_suffix(const char *text, const char *suffix)
{
    size_t a = strlen(text);
    size_t b = strlen(suffix);

    return a >= b && strcmp(text + a - b, suffix) == 0;
}

static void usage(void)
{
    printf("NwLink 0.1 - NwOS linker\n\n");
    printf("Usage:\n");
    printf("  link <input.o> [input.o ...] -o <output>\n");
    printf("  link [-f elf64|nwo64] <input.o> -o <output>\n\n");
    printf("An output ending in .nwo produces NWO64, otherwise ELF64.\n");
}

int main(int argc, char **argv)
{
    Object objects[MAX_OBJECTS];
    int object_count = 0;
    const char *output = NULL;
    int target = 0;
    int target_set = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            usage();
            return 0;
        }
        else if (!strcmp(argv[i], "-o"))
        {
            if (++i >= argc)
                fail("-o needs a file name");
            output = argv[i];
        }
        else if (!strcmp(argv[i], "-f"))
        {
            if (++i >= argc)
                fail("-f needs a format");

            if (!strcmp(argv[i], "elf64"))
                target = NWOBJ_TARGET_ELF64;
            else if (!strcmp(argv[i], "nwo64"))
                target = NWOBJ_TARGET_NWO64;
            else
                fail("unknown format '%s'", argv[i]);

            target_set = 1;
        }
        else if (argv[i][0] == '-')
        {
            fail("unknown option '%s'", argv[i]);
        }
        else
        {
            if (object_count >= MAX_OBJECTS)
                fail("too many input objects");
            load_object(&objects[object_count++], argv[i]);
        }
    }

    if (object_count == 0)
    {
        usage();
        return 1;
    }

    if (!output)
        fail("no output file (use -o)");

    if (!target_set)
        target = has_suffix(output, ".nwo")
                 ? NWOBJ_TARGET_NWO64
                 : NWOBJ_TARGET_ELF64;

    for (int i = 0; i < object_count; ++i)
    {
        if (objects[i].h.target != (uint32_t)target)
            fail("%s was assembled for %s but output requires %s",
                 objects[i].path,
                 objects[i].h.target == NWOBJ_TARGET_NWO64 ? "nwo64" : "elf64",
                 target == NWOBJ_TARGET_NWO64 ? "nwo64" : "elf64");
    }

    uint64_t code_cursor = 0;
    uint64_t data_cursor;

    for (int i = 0; i < object_count; ++i)
    {
        objects[i].code_base = align_up(code_cursor, 16);
        code_cursor = objects[i].code_base + objects[i].h.code_size;
    }

    data_cursor = align_up(code_cursor, 16);

    for (int i = 0; i < object_count; ++i)
    {
        objects[i].data_off = align_up(data_cursor, 16);
        objects[i].data_base = objects[i].data_off;
        data_cursor = objects[i].data_base + objects[i].h.data_size;
    }

    uint64_t image_size = align_up(data_cursor, 16);
    uint64_t bss_cursor = image_size;

    for (int i = 0; i < object_count; ++i)
    {
        objects[i].bss_base = align_up(bss_cursor, 16);
        bss_cursor = objects[i].bss_base + objects[i].h.bss_size;
    }

    uint64_t bss_size = align_up(bss_cursor - image_size, 16);

    resolve_symbols(objects, object_count);

    uint8_t *image = build_image(objects, object_count, image_size);
    uint32_t *nwo_relocs = NULL;
    uint32_t nwo_reloc_count = 0;

    apply_relocations(objects, object_count, image, image_size, target,
                      &nwo_relocs, &nwo_reloc_count);

    uint64_t entry = 0;
    int entry_found = 0;

    for (int i = 0; i < object_count; ++i)
    {
        Object *o = &objects[i];

        if (o->h.entry_sym == UINT32_MAX)
            continue;

        if (o->h.entry_sym >= o->h.sym_count)
            fail("%s: invalid entry symbol", o->path);

        if (o->syms[o->h.entry_sym].section == NWOBJ_SEC_UNDEF)
            fail("%s: entry symbol is undefined", o->path);

        if (entry_found)
            fail("multiple entry symbols found");

        entry = o->values[o->h.entry_sym];
        entry_found = 1;
    }

    if (!entry_found)
        fail("no entry symbol found");

    if (target == NWOBJ_TARGET_NWO64)
    {
        write_nwo64(output, objects, object_count, image, image_size,
                    align_up(code_cursor, 16), code_cursor,
                    data_cursor - align_up(code_cursor, 16), bss_size,
                    entry, nwo_relocs, nwo_reloc_count);
        printf("NwLink: %d object(s) -> %s (nwo64)\n", object_count, output);
        printf("  image: %llu bytes, bss: %llu bytes, relocs: %u\n",
               (unsigned long long)image_size,
               (unsigned long long)bss_size, nwo_reloc_count);
    }
    else
    {
        write_elf64(output, image, image_size, bss_size, entry);
        printf("NwLink: %d object(s) -> %s (elf64)\n", object_count, output);
        printf("  image: %llu bytes, bss: %llu bytes\n",
               (unsigned long long)image_size,
               (unsigned long long)bss_size);
    }

    return 0;
}
