#include "nwos_internal.h"

typedef struct
{
    const char* name;
    void (*run)(void);
    void (*run_arg)(const char* args);
    const char* help;
} Command;

#define CMD(name, fn, help)     { name, fn, 0, help }
#define CMD_ARG(name, fn, help) { name, 0, fn, help }

#define COMMAND_COUNT (sizeof(commands) / sizeof(commands[0]))

static void cmd_help(void);

static const Command commands[] =
{
    CMD    ("help",         cmd_help,        "help   - show commands"),
    CMD_ARG("echo",         cmd_echo,        "echo <text> - print text"),
    CMD    ("clear",        clear,           "clear  - clear screen"),
    CMD    ("about",        cmd_about,       "about  - OS information"),
    CMD    ("nwfetch",      nwfetch,         "nwfetch - system info"),
    CMD    ("games",        games_menu,      "games  - list games"),
    CMD    ("calc",         run_calc,        "calc   - Calculator"),
    CMD    ("color",        t_colorsetter,   "color  - change color"),
    CMD    ("format",       cmd_format,       "format  - erase all files"),
    CMD    ("panic safe",   cmd_panic_safe,  "panic safe|fatal - test kernel panic"),
    CMD    ("panic fatal",  cmd_panic_fatal, 0),
    CMD    ("passwd",       cmd_passwd,       "passwd - set or change the root password"),
    CMD_ARG("touch",       cmd_touch,       "touch <file1> [file2 ...] - create files"),
    CMD_ARG("bash",        cmd_bash,        "bash <file.sh> - run NwSH script"),
    CMD_ARG("bat",         cmd_bat,         "bat <file.bat> - run NwBAT script"),
    CMD_ARG("nwc",         cmd_nwc,        "nwc <file.nw> -o <file.nwo> - compile NwC source"),
    CMD_ARG("nw",          package_handler, "nw list|install|remove|update|run - local package manager"),
    CMD_ARG("cat",         fs_read,         "cat <file> - read file"),
    CMD    ("pwd",         cmd_pwd,           "pwd - print the current directory"),
    CMD_ARG("write",        cmd_write,       "write <file> <text> - write text to file"),
    CMD_ARG("edit",         redactor,        "edit <file> - edit a file"),

    CMD    ("rm -rf /",     death_screen,    0),
    CMD    ("rm /",         death_screen,    0),
    CMD    ("rm disk/",     disk_death,      0),
    CMD_ARG("rm",           fs_rm,           "rm [-r] <file1> [file2 ...] - remove files (-r = directories)"),
    CMD_ARG("rmdir",        fs_rmdir,        "rmdir <dir> - remove directory with its content"),
    CMD    ("reboot",       reboot,          "reboot - restart " OS_NAME),
    CMD_ARG("run",          nwo_run,         "run <file.nwo> - run NWO application"),
    CMD    ("apps",         apps_list,       "apps - List of apps"),
    CMD    ("generate num", generate_num,    "generate num - generate num"),
    CMD_ARG("hexview",      app_hexview,     "hexview <file> - hex byte map"),
    CMD_ARG("nwocheck",     app_nwo_info,    "nwocheck <file> - check OPcodes"),
    CMD    ("random word",  say,             "random word - generate word"),
    CMD    ("chat",         chat,            "chat - chat with computer"),
    CMD    ("change invite",set_p_shell,     "change invite - Change invite ('" OS_NAME ">')"),
    CMD    ("change color", change_p_color,  "change color - Change prompt color"),
    CMD    ("settings",     cmd_settings,    "settings - show configuration"),
    CMD    ("mem",          cmd_mem,         "mem    - memory info"),
    CMD    ("cpu",          cmd_cpu,         0                        ),
    CMD    ("uptime",       cmd_uptime,      0                        ),
    CMD    ("time",         cmd_time_cmd,    "time   - show time/date"),
    CMD    ("zones",        cmd_zones,       "zones  - list all time zones"),
    CMD_ARG("timezone",    cmd_timezone,    "timezone <num> - set timezone"),
    CMD_ARG("date",        cmd_date_set,    "date   - set custom date"),
    CMD    ("more",         cmd_more_help,   "more   - additional commands"),
    CMD    ("disks",        cmd_disks,       "disks  - list all ATA disks"),
    CMD    ("drive",        cmd_drive_status, 0),
    CMD_ARG("drive",        cmd_drive,       "drive <C|D|n> - switch the active disk"),
    CMD_ARG("chdrive",      cmd_drive,       0),
    CMD_ARG("disk",         cmd_disk_info,   "disk <C|D|n> - detailed size of one disk"),
    CMD    ("install",      cmd_install,      "install - run the installation wizard"),
    CMD    ("ls",           fs_ls,           "ls     - list files in current dir"),
    CMD_ARG("mkdir",        fs_mkdir,        "mkdir <dir> - create directory"),
    CMD_ARG("cd",           cmd_cd_path,               "cd <path> - change directory; supports ~, .. and nested paths"),
    CMD    ("gui",          run_gui,         "gui    - launch graphical shell"),
    CMD    ("word",         word_new,        "word   - generate random word"),

    CMD_ARG("ping",         net_ping,        "ping <host or IP> - e.g. ping www.google.com"),
    CMD    ("net",          net_info,        "net    - network status (MAC, IP, link)"),
    CMD    ("lspci",        net_lspci,       "lspci  - list PCI devices"),

    CMD    ("datec",        dateCr,          0),
    CMD    ("games old",    games,           0),
    CMD    ("testH",        testH,           0),
    CMD    ("guessh",       game_guessh,     0),
    CMD    ("game coin",    game_coin,       0),
    CMD    ("game dice",    game_dice,       0),
    CMD    ("game highlow", game_higher_lower, 0),
    CMD    ("game math",    game_math,       0),
    CMD    ("word",         game_word,       0),
    CMD    ("guess",        game_guess,      0),
    CMD    ("rps",          game_rps,        0),
    CMD    ("hangman",      game_hangman,    0),
    CMD    ("tictactoe",    game_tictactoe, 0),
};

static void cmd_help(void)
{
    set_color(TERM_COLOR_GREEN);
    print("Commands:\n");
    int lines_on_screen = 1;

    for (unsigned int i = 0; i < COMMAND_COUNT; i++)
    {
        if (commands[i].help)
        {
            if (lines_on_screen >= 52)
            {
                set_color(TERM_COLOR_YELLOW);
                print("--- MORE --- (ENTER: line, SPACE: page, ESC: quit)\n");
                set_color(TERM_COLOR_GREEN);

                int advance = 0;
                while (!advance)
                {
                    int k = keyboard_getkey();

                    if (k == 27 || k == KEY_ESC)
                    {
                        set_color(base_color);
                        return;
                    }
                    else if (k == KEY_ENTER)
                    {
                        lines_on_screen = 22;
                        advance = 1;
                    }
                    else if (k == ' ')
                    {
                        lines_on_screen = 0;
                        advance = 1;
                    }
                }

                set_color(TERM_COLOR_DARK_GRAY);
                print("--- continued ---\n");
                set_color(TERM_COLOR_GREEN);
                lines_on_screen++;
            }

            print("  ");
            print(commands[i].help);
            putchar_os('\n');
            lines_on_screen++;
        }
    }
    set_color(base_color);
    print("\nType 'more' for additional commands.\n");
}

int execute_single_command(char* command)
{
    if (command[0] == '\0')
        return 1;

    if (strcmp(command, "sudo") == 0)
    {
        print_error("Usage: sudo <command>\n");
        return 0;
    }

    if (strncmp(command, "sudo ", 5) == 0)
    {
        if (cmd_sudo)
        {
            print_error("Nested sudo is not supported.\n");
            return 0;
        }

        char nested[128];
        const char* src = command + 5;

        while (*src == ' ' || *src == '\t')
            src++;

        int length = 0;

        while (*src && length < (int)sizeof(nested) - 1)
            nested[length++] = *src++;

        while (length > 0 &&
            (nested[length - 1] == ' ' ||
                nested[length - 1] == '\t'))
        {
            length--;
        }

        nested[length] = '\0';

        if (length == 0)
        {
            print_error("Usage: sudo <command>\n");
            return 0;
        }

        if (!authenticate_root_password())
            return 0;

        int previous_sudo = cmd_sudo;

        cmd_sudo = 1;

        int result = execute_single_command(nested);

        cmd_sudo = previous_sudo;

        return result;
    }

    for (unsigned int i = 0; i < COMMAND_COUNT; i++)
    {
        const Command* c = &commands[i];

        if (c->run != 0)
        {

            if (strcmp(command, c->name) == 0)
            {
                script_command_failed = 0;
                c->run();
                return script_command_failed ? 0 : 1;
            }

            continue;
        }

        int n = command_name_length(c->name);

        if (strncmp(command, c->name, n) != 0)
            continue;

        if (command[n] == ' ' || command[n] == '\t')
        {
            const char* args = command + n;
            while (*args == ' ' || *args == '\t') args++;
            script_command_failed = 0;
            c->run_arg(args);
            return script_command_failed ? 0 : 1;
        }

        if (command[n] == '\0')
        {
            print_error("Usage: ");
            print_error(c->help ? c->help : c->name);
            putchar_os('\n');
            return 0;
        }
    }

    if (package_run_command(command))
        return 1;

    print_error("Unknown command: '");
    print_error(command);
    print_error("'. Type 'help'.\n");
    return 0;
}
