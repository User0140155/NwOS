#include "nwo_loader.h"
#include "kernel.h"
#include "memory/pmm.h"
int fs_find(const char* name);
int fs_read_binary(const char* name, unsigned char* buffer, unsigned int max_size);
unsigned int fs_get_file_size(const char* name);

void ata_read_sector_d(
    int drive_idx,
    unsigned int lba,
    unsigned char* buffer);

#define NWO_FS_SECTOR_SIZE   512

#define NWO_HEADER_SIZE      20

#define NWO_MAGIC_0          'N'
#define NWO_MAGIC_1          'W'
#define NWO_MAGIC_2          'O'
#define NWO_MAGIC_3          'B'

#define NWO_SHELL_MAGIC_0    'N'
#define NWO_SHELL_MAGIC_1    'W'
#define NWO_SHELL_MAGIC_2    'S'
#define NWO_SHELL_MAGIC_3    'C'

#define NWO_VERSION_1        1
#define NWO_VERSION_2        2
#define NWO_VERSION_3        3

#define NWO_MAX_FILE_SIZE    (16u * 1024u * 1024u)
#define NWO_MAX_CODE_SIZE    NWO_MAX_FILE_SIZE
#define NWO_STATIC_SIZE      65536u

static unsigned char nwo_static_buffer[NWO_STATIC_SIZE];
static unsigned char* nwo_file_buffer = nwo_static_buffer;
static unsigned char* nwo_heap_buffer = 0;
static unsigned int nwo_heap_pages = 0;
static const char* nwo_fail_reason = "";

const char* nwo_load_error(void)
{
    return nwo_fail_reason;
}

static void nwo_release_buffer(void)
{
    if (nwo_heap_buffer != 0)
        pmm_free_pages((uintptr_t)nwo_heap_buffer, nwo_heap_pages);

    nwo_heap_buffer = 0;
    nwo_heap_pages = 0;
    nwo_file_buffer = nwo_static_buffer;
}

static unsigned int nwo_read_u32(
    const unsigned char* p)
{
    return
        ((unsigned int)p[0]) |
        ((unsigned int)p[1] << 8) |
        ((unsigned int)p[2] << 16) |
        ((unsigned int)p[3] << 24);
}

int nwo_validate(
    const unsigned char* buffer,
    unsigned int size)
{
    unsigned int code_size;
    unsigned int data_size;
    unsigned int entry;
    unsigned int total_required;

    if (buffer == 0)
        return 0;

    if (size < NWO_HEADER_SIZE)
        return 0;
    if (buffer[0] != NWO_MAGIC_0 ||
        buffer[1] != NWO_MAGIC_1 ||
        buffer[2] != NWO_MAGIC_2 ||
        buffer[3] != NWO_MAGIC_3)
    {
        return 0;
    }
    if (buffer[4] != NWO_VERSION_1 &&
        buffer[4] != NWO_VERSION_2 &&
        buffer[4] != NWO_VERSION_3)
        return 0;
    code_size = nwo_read_u32(buffer + 8);
    data_size = nwo_read_u32(buffer + 12);
    entry     = nwo_read_u32(buffer + 16);
    if (code_size > NWO_MAX_FILE_SIZE)
        return 0;

    if (data_size > NWO_MAX_FILE_SIZE)
        return 0;

    total_required =
        NWO_HEADER_SIZE +
        code_size +
        data_size;
    if (total_required > size)
        return 0;
    if (code_size == 0)
    {
        if (entry != 0)
            return 0;
    }
    else
    {
        if (entry >= code_size)
            return 0;
    }

    return 1;
}
int nwo_load(
    const char* filename,
    NwoProgram* program)
{
    int index;
    unsigned int file_size;
    unsigned int actual_size;
    unsigned int code_size;
    unsigned int data_size;
    unsigned int entry;

    if (filename == 0 || program == 0)
        return 0;
    program->version    = 0;
    program->flags      = 0;
    program->code_size  = 0;
    program->data_size  = 0;
    program->entry      = 0;
    program->code       = 0;
    program->data       = 0;
    program->total_size = 0;
    program->export_count = 0;

    nwo_release_buffer();
    nwo_fail_reason = "";

    index = fs_find(filename);

    if (index == -1)
    {
        nwo_fail_reason = "file not found";
        return 0;
    }

    file_size = fs_get_file_size(filename);
    actual_size = file_size;

    if (file_size == 0 || file_size > NWO_MAX_FILE_SIZE)
    {
        nwo_fail_reason = "file is empty or larger than 16 MB";
        return 0;
    }

    if (file_size + 16u <= NWO_STATIC_SIZE)
    {
        nwo_file_buffer = nwo_static_buffer;
        for (unsigned int z = 0; z < file_size + 16u; z++)
            nwo_file_buffer[z] = 0;
    }
    else
    {
        unsigned int bytes = file_size + 16u;
        uint64_t phys = 0;
        unsigned char* mem = (unsigned char*)pmm_alloc_dma32(
            bytes, 4096, &phys);

        if (mem == 0)
        {
            nwo_fail_reason = "out of memory for the program";
            return 0;
        }

        nwo_heap_buffer = mem;
        nwo_heap_pages = (bytes + 4095u) / 4096u;
        nwo_file_buffer = mem;
        for (unsigned int z = 0; z < bytes; z++)
            mem[z] = 0;
    }

    if (!fs_read_binary(
            filename,
            nwo_file_buffer,
            file_size))
    {
        nwo_fail_reason = "disk read failed";
        nwo_release_buffer();
        return 0;
    }

    if (!nwo_validate(
            nwo_file_buffer,
            file_size))
    {
        nwo_fail_reason = "invalid NWO header";
        nwo_release_buffer();
        return 0;
    }

    program->version = nwo_file_buffer[4];
    program->flags   = nwo_file_buffer[5];

    code_size = nwo_read_u32(
        nwo_file_buffer + 8);

    data_size = nwo_read_u32(
        nwo_file_buffer + 12);

    entry = nwo_read_u32(
        nwo_file_buffer + 16);

    file_size =
        NWO_HEADER_SIZE +
        code_size +
        data_size;

    if (file_size > actual_size)
    {
        nwo_fail_reason = "header sizes exceed the file";
        nwo_release_buffer();
        return 0;
    }

    program->code =
        nwo_file_buffer + NWO_HEADER_SIZE;

    program->data =
        program->code + code_size;

    program->code_size  = code_size;
    program->data_size  = data_size;
    program->entry      = entry;
    program->total_size = file_size;

    if (file_size + 7 < actual_size &&
        nwo_file_buffer[file_size] == NWO_SHELL_MAGIC_0 &&
        nwo_file_buffer[file_size + 1] == NWO_SHELL_MAGIC_1 &&
        nwo_file_buffer[file_size + 2] == NWO_SHELL_MAGIC_2 &&
        nwo_file_buffer[file_size + 3] == NWO_SHELL_MAGIC_3)
    {
        unsigned int off = file_size + 4;
        unsigned int count = (unsigned int)nwo_file_buffer[off++];
        program->export_count = 0;

        for (unsigned int i = 0;
             i < count && i < NWO_MAX_SHELL_EXPORTS && off + 2 <= actual_size;
             i++)
        {
            unsigned int nlen = (unsigned int)nwo_file_buffer[off++];
            unsigned int flen = (unsigned int)nwo_file_buffer[off++];

            if (off + nlen + flen > actual_size)
                break;

            for (unsigned int j = 0;
                 j < nlen && j < NWO_SHELL_NAME_MAX - 1;
                 j++)
                program->exports[i].name[j] =
                    (char)nwo_file_buffer[off + j];
            program->exports[i].name[nlen < NWO_SHELL_NAME_MAX - 1
                                          ? nlen
                                          : NWO_SHELL_NAME_MAX - 1] = '\0';
            off += nlen;

            for (unsigned int j = 0;
                 j < flen && j < NWO_SHELL_NAME_MAX - 1;
                 j++)
                program->exports[i].func[j] =
                    (char)nwo_file_buffer[off + j];
            program->exports[i].func[flen < NWO_SHELL_NAME_MAX - 1
                                         ? flen
                                         : NWO_SHELL_NAME_MAX - 1] = '\0';
            off += flen;

            program->export_count++;
        }
    }

    return 1;
}

void nwo_unload(
    NwoProgram* program)
{
    if (program == 0)
        return;

    program->version    = 0;
    program->flags      = 0;
    program->code_size  = 0;
    program->data_size  = 0;
    program->entry      = 0;
    program->code       = 0;
    program->data       = 0;
    program->total_size = 0;
    program->export_count = 0;
    nwo_release_buffer();
}