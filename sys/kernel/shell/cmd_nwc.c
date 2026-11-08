#include "nwos_internal.h"

static int nwc_has_suffix(const char* name, const char* suffix);

static int nwc_has_suffix(const char* name, const char* suffix)
{
    int name_len = 0;
    int suffix_len = 0;
    while (name[name_len] != '\0') name_len++;
    while (suffix[suffix_len] != '\0') suffix_len++;
    if (name_len < suffix_len) return 0;
    return strcmp(name + name_len - suffix_len, suffix) == 0;
}

void cmd_nwc(const char* args)
{
    char input_name[MAX_FILENAME];
    char option[8];
    char output_name[MAX_FILENAME];
    int position = 0;
    int input_index;
    int token_result;
    unsigned int source_size;
    unsigned int nwo_size = 0;

    script_command_failed = 1;

    static char source_buffer[NWC_MAX_SOURCE_SIZE + 1];
    static unsigned char nwo_buffer[FS_FILE_SECTORS * 512u];

    if (!args) args = "";

    token_result = nwc_next_token(args, &position,
                                  input_name, sizeof(input_name));
    if (token_result != 1 ||
        nwc_next_token(args, &position, option, sizeof(option)) != 1 ||
        nwc_next_token(args, &position, output_name, sizeof(output_name)) != 1 ||
        args[position] != '\0' || strcmp(option, "-o") != 0)
    {
        print_error("Usage: nwc <file.nw> -o <file.nwo>\n");
        return;
    }

    if (!nwc_has_suffix(input_name, ".nw") ||
        !nwc_has_suffix(output_name, ".nwo"))
    {
        print_error("NwC expects an input .nw file and an output .nwo file.\n");
        return;
    }

    if (strcmp(input_name, output_name) == 0)
    {
        print_error("Input and output filenames must be different.\n");
        return;
    }

    input_index = fs_find(input_name);
    if (input_index < 0 || directory[input_index].type != FS_TYPE_FILE)
    {
        print_error("NwC source file not found.\n");
        return;
    }

    source_size = directory[input_index].size;
    if (source_size > NWC_MAX_SOURCE_SIZE)
    {
        print_error("NwC source exceeds the 16 KiB compiler limit.\n");
        return;
    }

    if (!fs_read_text(input_name, source_buffer, sizeof(source_buffer)))
    {
        print_error("Could not read NwC source file.\n");
        return;
    }

    print("NwC: compiling ");
    print(input_name);
    print("...\n");

    if (!nwc_compile_source(source_buffer, source_size,
                            nwo_buffer, sizeof(nwo_buffer), &nwo_size))
    {
        print_error("NwC compilation failed.\n");
        return;
    }

    if (nwo_size == 0 || nwo_size > sizeof(nwo_buffer))
    {
        print_error("NwC returned an invalid output size.\n");
        return;
    }

    if (!fs_write_binary(output_name, nwo_buffer, nwo_size))
    {
        print_error("Could not save NWO output file. Check filename and disk space.\n");
        return;
    }

    print_success("NwC: created ");
    print(output_name);
    print(" (");
    print_int((int)nwo_size);
    print(" bytes).\n");
    script_command_failed = 0;
}
