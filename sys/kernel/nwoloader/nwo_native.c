#include "nwo_native.h"

#include "../nwos_internal.h"
#include "../usermode.h"
#include "../memory/pmm.h"

#define NWO64_HEADER_SIZE 64
#define NWO64_MAX_FILE    (1024u * 1024u)
#define NWO64_RELOC_MASK  0x3FFFFFFFu

static const unsigned char nwo64_magic[8] =
{
    'N', 'W', 'O', '6', '4', 0, 0, 0
};

static unsigned int nwo64_u32(const unsigned char* p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static int nwo64_magic_ok(const unsigned char* buffer)
{
    for (int i = 0; i < 8; ++i)
    {
        if (buffer[i] != nwo64_magic[i])
            return 0;
    }

    return 1;
}

int nwo_native_try_run(const char* filename)
{
    unsigned int file_size;
    uint64_t phys = 0;
    unsigned char* buffer;
    unsigned int version;
    unsigned int code_size;
    unsigned int data_size;
    unsigned int bss_size;
    unsigned int entry;
    unsigned int image_off;
    unsigned int image_size;
    unsigned int reloc_count;
    unsigned int reloc_off;
    uint64_t exit_code = 0;
    int result;

    if (filename == 0)
        return 0;

    file_size = fs_get_file_size(filename);

    if (file_size < 8 || file_size > NWO64_MAX_FILE)
        return 0;

    buffer = (unsigned char*)pmm_alloc_dma32(file_size, 4096, &phys);

    if (buffer == 0)
        return 0;

    if (!fs_read_binary(filename, buffer, file_size))
    {
        pmm_free_pages((uintptr_t)buffer,
                       (file_size + 4095u) / 4096u);
        return 0;
    }

    if (!nwo64_magic_ok(buffer))
    {
        pmm_free_pages((uintptr_t)buffer,
                       (file_size + 4095u) / 4096u);
        return 0;
    }

    if (file_size < NWO64_HEADER_SIZE)
    {
        print_error("NWO64: file is too small.\n");
        pmm_free_pages((uintptr_t)buffer,
                       (file_size + 4095u) / 4096u);
        return 1;
    }

    version = nwo64_u32(buffer + 8);

    if (version != 1)
    {
        print_error("NWO64: unsupported version.\n");
        pmm_free_pages((uintptr_t)buffer,
                       (file_size + 4095u) / 4096u);
        return 1;
    }

    code_size   = nwo64_u32(buffer + 16);
    data_size   = nwo64_u32(buffer + 20);
    bss_size    = nwo64_u32(buffer + 24);
    entry       = nwo64_u32(buffer + 28);
    image_off   = nwo64_u32(buffer + 32);
    image_size  = nwo64_u32(buffer + 36);
    reloc_count = nwo64_u32(buffer + 44);
    reloc_off   = nwo64_u32(buffer + 48);

    if (image_size == 0 || entry >= image_size ||
        image_off > file_size ||
        image_size > file_size - image_off ||
        reloc_off > file_size ||
        reloc_count > (file_size - reloc_off) / 4 ||
        (uint64_t)image_size + bss_size + 0x10000u > 0x200000u)
    {
        print_error("NWO64: invalid image header.\n");
        pmm_free_pages((uintptr_t)buffer,
                       (file_size + 4095u) / 4096u);
        return 1;
    }

    (void)code_size;
    (void)data_size;

    print("NWO64: executing...\n\n");

    result = usermode_run_native(
        buffer + image_off,
        image_size,
        bss_size,
        entry,
        (const uint32_t*)(uintptr_t)(buffer + reloc_off),
        reloc_count,
        &exit_code);

    pmm_free_pages((uintptr_t)buffer,
                   (file_size + 4095u) / 4096u);

    if (!result)
    {
        print_error("NWO64: cannot start program.\n");
        return 1;
    }

    print("\n");
    print_success("Program finished. Return code: ");
    print_int((int)exit_code);
    putchar_os('\n');

    return 1;
}
