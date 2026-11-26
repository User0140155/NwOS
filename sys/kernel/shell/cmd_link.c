#include "nwos_internal.h"

#define LINK_ARG_COUNT (NWC_KMAX_UNITS + NWC_KMAX_SETS + 8)

static int link_has_suffix(const char* name, const char* suffix)
{
    int name_len = 0;
    int suffix_len = 0;

    while (name[name_len] != '\0') name_len++;
    while (suffix[suffix_len] != '\0') suffix_len++;

    if (name_len < suffix_len) return 0;
    return strcmp(name + name_len - suffix_len, suffix) == 0;
}

static void link_usage(void)
{
    print_error("Usage: link <file.set> [...] <file.nw> [...] -o <file.nwo>\n");
}

void cmd_link(const char* args)
{
    char tokens[LINK_ARG_COUNT][MAX_FILENAME];
    const char* set_names[NWC_KMAX_SETS];
    const char* source_names[NWC_KMAX_UNITS];
    NwcKernelSet sets[NWC_KMAX_SETS];
    NwcKernelSource sources[NWC_KMAX_UNITS];
    const char* output_name = 0;
    int token_count = 0;
    int set_count = 0;
    int source_count = 0;
    int position = 0;
    int i;

    static char source_buffers[NWC_KMAX_UNITS][NWC_MAX_SOURCE_SIZE + 1];
    static unsigned char set_buffers[NWC_KMAX_SETS][NWC_KMAX_SET_SIZE];
    static unsigned char nwo_buffer[20480];

    script_command_failed = 1;

    if (!args) args = "";

    while (token_count < LINK_ARG_COUNT)
    {
        int result = nwc_next_token(args, &position,
                                    tokens[token_count], MAX_FILENAME);

        if (result == 0)
            break;

        if (result < 0)
        {
            print_error("NwLink: argument is too long or unterminated.\n");
            return;
        }

        token_count++;
    }

    if (token_count == 2 &&
        (strcmp(tokens[1], "-h") == 0 || strcmp(tokens[1], "--help") == 0))
    {
        link_usage();
        return;
    }

    for (i = 0; i < token_count; i++)
    {
        if (strcmp(tokens[i], "-o") == 0)
        {
            if (++i >= token_count)
            {
                print_error("NwLink: -o needs an output file.\n");
                return;
            }

            output_name = tokens[i];
        }
        else if (strcmp(tokens[i], "-I") == 0)
        {
            if (++i >= token_count)
            {
                print_error("NwLink: -I needs a directory.\n");
                return;
            }
        }
        else if (tokens[i][0] == '-')
        {
            print_error("NwLink: unknown option.\n");
            link_usage();
            return;
        }
        else if (link_has_suffix(tokens[i], ".set"))
        {
            if (set_count >= NWC_KMAX_SETS)
            {
                print_error("NwLink: too many .set archives.\n");
                return;
            }

            set_names[set_count++] = tokens[i];
        }
        else if (link_has_suffix(tokens[i], ".nw"))
        {
            if (source_count >= NWC_KMAX_UNITS)
            {
                print_error("NwLink: too many .nw sources.\n");
                return;
            }

            source_names[source_count++] = tokens[i];
        }
        else
        {
            print_error("NwLink: expected a .set or .nw input.\n");
            return;
        }
    }

    if (output_name == 0 || source_count == 0)
    {
        link_usage();
        return;
    }

    if (!link_has_suffix(output_name, ".nwo"))
    {
        print_error("NwLink expects a .nwo output file.\n");
        return;
    }

    for (i = 0; i < set_count; i++)
    {
        int index = fs_find(set_names[i]);

        if (index < 0 || directory[index].type != FS_TYPE_FILE ||
            directory[index].size > NWC_KMAX_SET_SIZE ||
            !fs_read_binary(set_names[i], set_buffers[i],
                            sizeof(set_buffers[i])))
        {
            print_error("NwLink: could not read .set archive.\n");
            return;
        }

        sets[i].path = set_names[i];
        sets[i].data = set_buffers[i];
        sets[i].size = directory[index].size;
    }

    for (i = 0; i < source_count; i++)
    {
        int index = fs_find(source_names[i]);

        if (index < 0 || directory[index].type != FS_TYPE_FILE ||
            directory[index].size > NWC_MAX_SOURCE_SIZE ||
            !fs_read_text(source_names[i], source_buffers[i],
                          sizeof(source_buffers[i])))
        {
            print_error("NwLink: could not read .nw source.\n");
            return;
        }

        sources[i].path = source_names[i];
        sources[i].source = source_buffers[i];
        sources[i].size = directory[index].size;
    }

    print("NwLink: linking ");

    for (i = 0; i < set_count; i++)
    {
        if (i > 0)
            print(", ");
        print(set_names[i]);
    }

    print(" with ");

    for (i = 0; i < source_count; i++)
    {
        if (i > 0)
            print(", ");
        print(source_names[i]);
    }

    print("...\n");

    {
        unsigned int nwo_size = 0;

        if (!nwc_link_sets(sets, set_count,
                           sources, source_count,
                           nwo_buffer, sizeof(nwo_buffer), &nwo_size))
        {
            print_error("NwLink: link failed.\n");
            return;
        }

        if (nwo_size == 0 || nwo_size > sizeof(nwo_buffer))
        {
            print_error("NwLink returned an invalid output size.\n");
            return;
        }

        if (!fs_write_binary(output_name, nwo_buffer, nwo_size))
        {
            print_error("Could not save linked NWO file. Check filename and disk space.\n");
            return;
        }

        print_success("NwLink: created ");
        print(output_name);
        print(" (");
        print_int((int)nwo_size);
        print(" bytes).\n");
        script_command_failed = 0;
    }
}
