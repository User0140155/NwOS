#include "package.h"
#include "kernel.h"
#include "packages_data.h"
#include "../../compiler/nwc_kernel.h"

#define PKG_DIR             "/home/pkg"
#define PKG_EXTENSION       ".nwo"
#define PKG_MAX_NWO_SIZE    (40u * 512u)
#define PKG_NAME_MAX        64

static int pkg_string_compare(const char* left, const char* right)
{
    while (*left && (*left == *right))
    {
        left++;
        right++;
    }
    return *(const unsigned char*)left - *(const unsigned char*)right;
}

static int pkg_name_valid(const char* name)
{
    int length = 0;

    if (name == 0 || name[0] == '\0')
        return 0;

    while (name[length] != '\0')
    {
        char c = name[length];

        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '-' || c == '_'))
            return 0;

        if (length >= 31)
            return 0;

        length++;
    }

    return length > 0;
}

static int pkg_next_token(const char* text, int* position,
                          char* output, int capacity)
{
    int i = *position;
    int length = 0;

    while (text[i] == ' ' || text[i] == '\t')
        i++;

    if (text[i] == '\0')
    {
        *position = i;
        return 0;
    }

    while (text[i] != '\0' && text[i] != ' ' && text[i] != '\t')
    {
        if (length >= capacity - 1)
            return -1;

        output[length++] = text[i++];
    }

    output[length] = '\0';

    while (text[i] == ' ' || text[i] == '\t')
        i++;

    *position = i;
    return 1;
}

static int pkg_installed_path(const char* name, char* output, int capacity)
{
    const char* prefix = PKG_DIR "/";
    const char* suffix = PKG_EXTENSION;
    int position = 0;

    if (name == 0 || output == 0 || capacity <= 0)
        return 0;

    while (*prefix)
    {
        if (position >= capacity - 1)
            return 0;
        output[position++] = *prefix++;
    }

    while (*name)
    {
        if (position >= capacity - 1)
            return 0;
        output[position++] = *name++;
    }

    while (*suffix)
    {
        if (position >= capacity - 1)
            return 0;
        output[position++] = *suffix++;
    }

    output[position] = '\0';
    return 1;
}

static const nw_package_t* pkg_find(const char* name)
{
    for (unsigned int i = 0; i < NW_PACKAGE_COUNT; i++)
    {
        if (pkg_string_compare(nw_packages[i].name, name) == 0)
            return &nw_packages[i];
    }

    return 0;
}

static int pkg_is_installed(const nw_package_t* package)
{
    char path[96];

    if (package == 0 ||
        !pkg_installed_path(package->name, path, sizeof(path)))
        return 0;

    return fs_path_is_file(path);
}

static int pkg_install_one(const nw_package_t* package, int quiet)
{
    static unsigned char nwo_buffer[PKG_MAX_NWO_SIZE];
    unsigned int nwo_size = 0;
    char path[96];

    if (package == 0)
        return 0;

    if (!pkg_installed_path(package->name, path, sizeof(path)))
    {
        print_error("Package path is too long.\n");
        return 0;
    }

    if (!quiet)
    {
        print("Compiling package '");
        print(package->name);
        print("' ");
        print(package->version);
        print("...\n");
    }

    if (!nwc_compile_source(package->source, package->source_size,
                            nwo_buffer, sizeof(nwo_buffer), &nwo_size))
    {
        print_error("Package compilation failed.\n");
        return 0;
    }

    if (nwo_size == 0 || nwo_size > sizeof(nwo_buffer))
    {
        print_error("Compiler returned an invalid NWO size.\n");
        return 0;
    }

    if (!fs_write_binary_path(path, nwo_buffer, nwo_size))
    {
        print_error("Could not write installed package to " PKG_DIR ".\n");
        return 0;
    }

    if (!quiet)
    {
        print_success("Installed ");
        print(package->name);
        print(" -> ");
        print(path);
        print(" (");
        print_int((int)nwo_size);
        print(" bytes).\n");
    }

    return 1;
}

static void pkg_print_help(void)
{
    print("\n=== Local package manager (nw) ===\n");
    print("  nw list                 - list packages in the local repository\n");
    print("  nw install <name>       - compile and install a package\n");
    print("  nw remove <name>        - remove an installed package\n");
    print("  nw update               - rebuild all installed packages\n");
    print("  nw run <name>           - run /home/pkg/<name>.nwo\n");
    print("  nw --help               - show this help message\n");
    print("  Installed packages are stored in " PKG_DIR "/<name>" PKG_EXTENSION ".\n");
    print("  Local sources live in the repository's local/ directory.\n\n");
}

static void pkg_list(void)
{
    print("\n=== Local package repository ===\n\n");

    if (NW_PACKAGE_COUNT == 0)
    {
        print("  (no packages in local/)\n\n");
        return;
    }

    for (unsigned int i = 0; i < NW_PACKAGE_COUNT; i++)
    {
        const nw_package_t* package = &nw_packages[i];

        print("  ");
        print(package->name);
        print(" ");
        print(package->version);

        if (pkg_is_installed(package))
            print("  [installed]");

        print("\n    ");
        print(package->description);
        print("\n    command: ");
        print(package->command);
        putchar_os('\n');
    }

    print("\n");
}

static void pkg_update(void)
{
    int installed = 0;
    int updated = 0;

    print("Updating packages from the local repository...\n");

    for (unsigned int i = 0; i < NW_PACKAGE_COUNT; i++)
    {
        const nw_package_t* package = &nw_packages[i];

        if (!pkg_is_installed(package))
            continue;

        installed++;

        if (pkg_install_one(package, 0))
            updated++;
    }

    if (installed == 0)
    {
        print("No installed packages to update.\n");
        return;
    }

    print_success("Package update complete: ");
    print_int(updated);
    print("/");
    print_int(installed);
    print(" packages rebuilt.\n");
}

void package_handler(const char* args)
{
    char operation[32];
    char name[PKG_NAME_MAX];
    int position = 0;
    int token_result;

    if (args == 0)
        args = "";

    token_result = pkg_next_token(args, &position,
                                  operation, sizeof(operation));

    if (token_result != 1 ||
        pkg_string_compare(operation, "--help") == 0 ||
        pkg_string_compare(operation, "help") == 0)
    {
        pkg_print_help();
        return;
    }

    if (pkg_string_compare(operation, "list") == 0 ||
        pkg_string_compare(operation, "available") == 0 ||
        pkg_string_compare(operation, "-lc") == 0)
    {
        pkg_list();
        return;
    }

    if (pkg_string_compare(operation, "update") == 0)
    {
        pkg_update();
        return;
    }

    if (pkg_string_compare(operation, "install") == 0 ||
        pkg_string_compare(operation, "-S") == 0 ||
        pkg_string_compare(operation, "add") == 0)
    {
        const nw_package_t* package;

        token_result = pkg_next_token(args, &position,
                                      name, sizeof(name));

        if (token_result != 1)
        {
            print_error("Usage: nw install <package>\n");
            return;
        }

        package = pkg_find(name);

        if (package == 0)
        {
            print_error("Package not found in local repository: ");
            print(name);
            print("\nRun 'nw list' to see available packages.\n");
            return;
        }

        pkg_install_one(package, 0);
        return;
    }

    if (pkg_string_compare(operation, "remove") == 0 ||
        pkg_string_compare(operation, "uninstall") == 0 ||
        pkg_string_compare(operation, "-R") == 0)
    {
        char path[96];

        token_result = pkg_next_token(args, &position,
                                      name, sizeof(name));

        if (token_result != 1)
        {
            print_error("Usage: nw remove <package>\n");
            return;
        }

        if (!pkg_name_valid(name) ||
            !pkg_installed_path(name, path, sizeof(path)))
        {
            print_error("Invalid package name.\n");
            return;
        }

        if (!fs_path_is_file(path))
        {
            print_error("Package is not installed: ");
            print(name);
            putchar_os('\n');
            return;
        }

        if (!fs_delete_file_path(path))
        {
            print_error("Could not remove package: ");
            print(name);
            putchar_os('\n');
            return;
        }

        print_success("Package removed: ");
        print(name);
        putchar_os('\n');
        return;
    }

    if (pkg_string_compare(operation, "run") == 0)
    {
        char path[96];

        token_result = pkg_next_token(args, &position,
                                      name, sizeof(name));

        if (token_result != 1)
        {
            print_error("Usage: nw run <package>\n");
            return;
        }

        if (!pkg_name_valid(name) ||
            !pkg_installed_path(name, path, sizeof(path)) ||
            !fs_path_is_file(path))
        {
            print_error("Package is not installed: ");
            print(name);
            putchar_os('\n');
            return;
        }

        print("Running ");
        print(path);
        print("...\n");
        fs_run_nwo_path(path);
        return;
    }

    print_error("Unknown package command. Type 'nw --help' for usage.\n");
}

int package_run_command(const char* command)
{
    char name[PKG_NAME_MAX];
    char path[96];
    int position = 0;

    if (command == 0 ||
        pkg_next_token(command, &position, name, sizeof(name)) != 1)
        return 0;

    while (command[position] == ' ' || command[position] == '\t')
        position++;

    if (command[position] != '\0' || !pkg_name_valid(name))
        return 0;

    if (pkg_installed_path(name, path, sizeof(path)) &&
        fs_path_is_file(path))
        return fs_run_nwo_path(path);

    for (unsigned int i = 0; i < NW_PACKAGE_COUNT; i++)
    {
        const nw_package_t* package = &nw_packages[i];

        if (pkg_string_compare(package->command, name) != 0 ||
            !pkg_is_installed(package))
            continue;

        if (!pkg_installed_path(package->name, path, sizeof(path)))
            return 0;

        return fs_run_nwo_path(path);
    }

    return 0;
}
