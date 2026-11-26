#include "nwos_internal.h"

#define NWC_ARG_COUNT (NWC_KMAX_UNITS + 8)

static int nwc_has_suffix(const char* name, const char* suffix)
{
    int name_len = 0;
    int suffix_len = 0;

    while (name[name_len] != '\0') name_len++;
    while (suffix[suffix_len] != '\0') suffix_len++;

    if (name_len < suffix_len) return 0;
    return strcmp(name + name_len - suffix_len, suffix) == 0;
}

static void nwc_usage(void)
{
    print_error("Usage: nwc <file.nw> [...] -o <file.nwo>\n");
    print_error("       nwc -h <file.nwh> [...] -o <file.set>\n");
    print_error("       nwc [-I <dir>] ...   (-I is accepted for compatibility)\n");
}

void cmd_nwc(const char* args)
{
    char tokens[NWC_ARG_COUNT][MAX_FILENAME];
    const char* inputs[NWC_KMAX_UNITS];
    NwcKernelSource sources[NWC_KMAX_HEADERS];
    const char* output_name = 0;
    int token_count = 0;
    int input_count = 0;
    int position = 0;
    int header_mode = 0;
    int max_inputs = NWC_KMAX_UNITS;
    int i;

    static char source_buffers[NWC_KMAX_HEADERS][NWC_MAX_SOURCE_SIZE + 1];
    static unsigned char nwo_buffer[20480];
    static unsigned char set_buffer[NWC_KMAX_SET_SIZE];

    script_command_failed = 1;

    if (!args) args = "";

    while (token_count < NWC_ARG_COUNT)
    {
        int result = nwc_next_token(args, &position,
                                    tokens[token_count], MAX_FILENAME);

        if (result == 0)
            break;

        if (result < 0)
        {
            print_error("NwC: argument is too long or unterminated.\n");
            return;
        }

        token_count++;
    }

    if (token_count == 2 &&
        (strcmp(tokens[1], "-h") == 0 || strcmp(tokens[1], "--help") == 0))
    {
        nwc_usage();
        return;
    }

    i = 0;

    if (token_count > 0 && strcmp(tokens[0], "-h") == 0)
    {
        header_mode = 1;
        max_inputs = NWC_KMAX_HEADERS;
        i = 1;
    }

    for (; i < token_count; i++)
    {
        if (strcmp(tokens[i], "-o") == 0)
        {
            if (++i >= token_count)
            {
                print_error("NwC: -o needs an output file.\n");
                return;
            }

            output_name = tokens[i];
        }
        else if (strcmp(tokens[i], "-I") == 0)
        {
            /* The in-OS filesystem has no include search paths; quoted
               .nwh includes are loaded by the header loader instead. */
            if (++i >= token_count)
            {
                print_error("NwC: -I needs a directory.\n");
                return;
            }
        }
        else if (tokens[i][0] == '-')
        {
            print_error("NwC: unknown option.\n");
            nwc_usage();
            return;
        }
        else
        {
            if (input_count >= max_inputs)
            {
                print_error("NwC: too many input files.\n");
                return;
            }

            inputs[input_count++] = tokens[i];
        }
    }

    if (output_name == 0 || input_count == 0)
    {
        nwc_usage();
        return;
    }

    if (!nwc_has_suffix(output_name, header_mode ? ".set" : ".nwo"))
    {
        print_error(header_mode
                        ? "NwC -h expects a .set output file.\n"
                        : "NwC expects a .nwo output file.\n");
        return;
    }

    for (i = 0; i < input_count; i++)
    {
        int index;
        unsigned int size;

        if (!nwc_has_suffix(inputs[i], header_mode ? ".nwh" : ".nw"))
        {
            print_error(header_mode
                            ? "NwC -h expects .nwh header files.\n"
                            : "NwC expects .nw source files.\n");
            return;
        }

        index = fs_find(inputs[i]);

        if (index < 0 || directory[index].type != FS_TYPE_FILE)
        {
            print_error("NwC source file not found.\n");
            return;
        }

        size = directory[index].size;

        if (size > NWC_MAX_SOURCE_SIZE)
        {
            print_error("NwC source exceeds the 16 KiB compiler limit.\n");
            return;
        }

        if (!fs_read_text(inputs[i], source_buffers[i],
                          sizeof(source_buffers[i])))
        {
            print_error("Could not read NwC source file.\n");
            return;
        }

        sources[i].path = inputs[i];
        sources[i].source = source_buffers[i];
        sources[i].size = size;
    }

    print("NwC: ");
    print(header_mode ? "building .set from " : "compiling ");

    for (i = 0; i < input_count; i++)
    {
        if (i > 0)
            print(", ");
        print(inputs[i]);
    }

    print("...\n");

    if (header_mode)
    {
        unsigned int set_size = 0;

        if (!nwc_build_set(sources, input_count,
                           set_buffer, sizeof(set_buffer), &set_size))
        {
            print_error("NwC: could not build the .set archive.\n");
            return;
        }

        if (set_size == 0 || set_size > sizeof(set_buffer))
        {
            print_error("NwC returned an invalid .set size.\n");
            return;
        }

        if (!fs_write_binary(output_name, set_buffer, set_size))
        {
            print_error("Could not save .set archive. Check filename and disk space.\n");
            return;
        }

        print_success("NwC: created ");
        print(output_name);
        print(" (");
        print_int((int)set_size);
        print(" bytes).\n");
        script_command_failed = 0;
        return;
    }

    {
        unsigned int nwo_size = 0;

        if (!nwc_compile_sources(sources, input_count,
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
}
