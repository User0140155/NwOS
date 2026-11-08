#include "nwos_internal.h"

void cmd_format(void)
{
    if (!cmd_sudo && !authenticate_root_password())
        return;

    print_error("This will erase the active NwOS filesystem.\n");
    print("Type format to continue: ");

    char answer[16];
    read_line(answer, sizeof(answer));

    if (strcmp(answer, "format") != 0)
    {
        print("Formatting cancelled.\n");
        return;
    }

    fs_format_disk(fs_current_drive);
}

void death_screen(void) {
    if (!authorize_sensitive_action(
            "Deleting everything inside the root directory",
            "/"))
    {
        return;
    }
    print_error("Deleting '/' directory..\n");
    sleep(1000);

    for (int i = 1; i < MAX_FILES; i++) {
        directory[i].used = 0;
        directory[i].name[0] = '\0';
        directory[i].size = 0;
    }

    unsigned char zero[512];
    for (int k = 0; k < 512; k++) zero[k] = 0;
    for (int i = 0; i < MAX_FILES * FS_FILE_SECTORS; i++) {
        fs_write_lba(FS_DATA_LBA + i, zero);
    }

    directory[1].used = 1;
    directory[1].type = FS_TYPE_FILE;
    directory[1].parent = 0;
    strcpy(directory[1].name, "death.bsod");
    const char* msg = "Root filesystem destroyed.";
    directory[1].size = 45;

    unsigned char sector[512];
    for (int k = 0; k < 512; k++) sector[k] = 0;
    for (int k = 0; msg[k] != '\0' && k < 511; k++) sector[k] = msg[k];
    fs_write_lba(FS_DATA_LBA + 1 * FS_FILE_SECTORS, sector);

    fs_save_directory();

    KPANIC_FATAL(ERR_DEATH, "Root filesystem destroyed.");
}

void disk_death(void) {
    print("Unknown error.\n");
    unknow_panic(DISK_DEATH);
}

void cmd_pwd(void)
{
    print(fs_current_path());
    putchar_os('\n');
}

void run_calc(void) {
    run_nwo_from_dir("/sys/apps", "calculator.nwo");
}

void cmd_more_help(void)
{
    set_color(TERM_COLOR_GREEN);
    print("More commands:\n");
    print("  mem      - show memory info\n");
    print("  cpu      - show CPU info\n");
    print("  uptime   - show uptime\n");
    print("  time     - show current time/date\n");
    print("  zones    - list all time zones\n");
    print("  timezone <num> - set timezone\n");
    print("  date <DD> <MM> <YYYY> - set custom date\n");
    print("  date reset - reset date\n");
    print("  nwfetch  - system info\n");
    print("  reboot   - reboot system\n");
    print("  clear    - clear screen\n");
    print("  disks    - list all ATA disks\n");
    print("  drive <C|D|n> - switch the active disk\n");
    print("  disk <C|D|n>  - detailed size of one disk\n");
    putchar_os('\n');
    set_color(base_color);
}

void cmd_about(void)
{
    set_color(TERM_COLOR_WHITE);
    print("====" OS_NAME " " OS_VERSION_STR "====\n");
    print("Name: " OS_NAME "\n");
    print("Version: v" OS_VERSION_STR "\n");
    print("Arch: x86-64\n");
    print("Display: VGA text mode\n");
    print("PS/2 keyboard\n");
    print("Network: e1000 (ping)\n");
    print("================\n");
    set_color(base_color);
}
