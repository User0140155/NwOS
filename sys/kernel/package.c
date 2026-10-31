#include "package.h"
#include "kernel.h"
#include "fb.h"

void print(const char* text);
void print_error(const char* text);
void print_success(const char* text);

static int string_compare(const char* s1, const char* s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void print_help(void)
{
    print("\n=== Local Package Manager (nw) ===\n");
    print("  sudo nw update          - Update system packages from home/pkg/\n");
    print("  sudo nw -S <name>       - Install local package\n");
    print("  sudo nw -R <name>       - Remove package\n");
    print("  sudo nw -lc             - List local available packages\n");
    print("  sudo nw --help          - Show this help message\n\n");
}

void package_handler(const char* args)
{
    while (*args == ' ')
        args++;

    if (*args == '\0' || string_compare(args, "--help") == 0)
    {
        print_help();
        return;
    }

    if (string_compare(args, "update") == 0)
    {
        print("Checking local repository at 'home/pkg/'...\n");
        print_success("System updated successfully from local storage!\n");
    }
    else if (args[0] == '-' && args[1] == 'S')
    {
        const char* pkg_name = args + 2;
        while (*pkg_name == ' ')
            pkg_name++;

        if (*pkg_name == '\0')
        {
            print_error("Error: Specify package name. Example: sudo nw -S editor\n");
            return;
        }

        print("Installing package: ");
        print(pkg_name);
        print("...\n");
        
        print_success("Package installed successfully!\n");
    }
    else if (args[0] == '-' && args[1] == 'R')
    {
        const char* pkg_name = args + 2;
        while (*pkg_name == ' ')
            pkg_name++;

        if (*pkg_name == '\0')
        {
            print_error("Error: Specify package name to remove.\n");
            return;
        }

        print("Removing package: ");
        print(pkg_name);
        print("...\n");
        print_success("Package removed.\n");
    }
    else if (string_compare(args, "-lc") == 0)
    {
        print("\nAvailable local packages in 'home/pkg/':\n");
        print("  - core-utils (installed)\n");
        print("  - text-editor (available)\n");
        print("  - net-tools (available)\n\n");
    }
    else
    {
        print_error("Unknown package command. Type 'sudo nw --help' for usage.\n");
    }
}