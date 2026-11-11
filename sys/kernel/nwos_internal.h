#ifndef NWOS_INTERNAL_H
#define NWOS_INTERNAL_H

#include "../../drivers/keyboard.h"
#include "../../drivers/mouse.h"
#include "../../drivers/disk.h"
#include "../../include/kernelpanic.h"
#include "Apps/wordgenerator/wordgenerator.h"
#include "Apps/textredactor/textredactor.h"
#include "nwoloader/nwo_vm.h"
#include "nwoloader/nwo_runner.h"
#include "installer.h"
#include "Apps/Games/slot.h"
#include "time.h"
#include "Apps/Games/game_memory.h"
#include "Apps/Games/snake.h"
#include "kernel.h"
#include "../../compiler/nwc_kernel.h"
#include "memory/pmm.h"
#include "random/word_module.h"
#include "random/entropy.h"
#include "random/passage_module.h"
#undef COLOR_BLACK
#undef COLOR_BLUE
#undef COLOR_GREEN
#undef COLOR_CYAN
#undef COLOR_RED
#undef COLOR_MAGENTA
#undef COLOR_BROWN
#undef COLOR_LIGHT_GRAY
#undef COLOR_DARK_GRAY
#undef COLOR_LIGHT_BLUE
#undef COLOR_LIGHT_GREEN
#undef COLOR_LIGHT_CYAN
#undef COLOR_LIGHT_RED
#undef COLOR_LIGHT_MAGENTA
#undef COLOR_YELLOW
#undef COLOR_WHITE
#include "graphics.h"
#include "memory/mem.h"
#include "Apps/fileStats/filestats.h"
#include "../../drivers/usb/xhci.h"
#include "Apps/baseconverter/baseconverter.h"
#ifdef NWOS_INCLUDE_ROOTFS_DATA_H
#include "rootfs_data.h"
#endif
#include "package.h"
#ifdef NWOS_INCLUDE_WORDS_DATA_H
#include "words_data.h"
#endif
#include "network/net.h"
#include "nwo_loader.h"
#include "network/timer.h"
#ifdef NWOS_INCLUDE_ZONES_H
#include "zones.h"
#endif
#ifdef NWOS_INCLUDE_LOGONWOS_H
#include "../../include/LogoNwOS.h"
#endif
#ifdef NWOS_INCLUDE_FONT_H
#include "font.h"
#endif
#ifdef NWOS_INCLUDE_CHAR_SYMBOL_H
#include "char_symbol.h"
#endif
#ifdef NWOS_INCLUDE_UTF8_H
#include "utf8.h"
#endif
#include "fb.h"
#include "video_config.h"
#include <stdint.h>

typedef unsigned short uint16_t;

#define WIDTH 80
#define HEIGHT 60

enum {
    TERM_COLOR_BLACK = 0x00,
    TERM_COLOR_BLUE = 0x01,
    TERM_COLOR_GREEN = 0x02,
    TERM_COLOR_CYAN = 0x03,
    TERM_COLOR_RED = 0x04,
    TERM_COLOR_MAGENTA = 0x05,
    TERM_COLOR_BROWN = 0x06,
    TERM_COLOR_LIGHT_GRAY = 0x07,
    TERM_COLOR_DARK_GRAY = 0x08,
    TERM_COLOR_LIGHT_BLUE = 0x09,
    TERM_COLOR_LIGHT_GREEN = 0x0A,
    TERM_COLOR_LIGHT_CYAN = 0x0B,
    TERM_COLOR_LIGHT_RED = 0x0C,
    TERM_COLOR_LIGHT_MAGENTA = 0x0D,
    TERM_COLOR_YELLOW = 0x0E,
    TERM_COLOR_WHITE = 0x0F
};

#define MAX_FILES 64
#define MAX_FILENAME 32
#define MAX_FILE_SIZE 20480

#define FS_TYPE_FILE 0
#define FS_TYPE_DIR  1

#define MAX_HISTORY_COMMANDS 256
#define MAX_CMD_LENGTH 1024
#define HISTORY_MAX_BYTES 10240

#define CP_H2   0xCD
#define CP_V2   0xBA
#define CP_TL2  0xC9
#define CP_TR2  0xBB
#define CP_BL2  0xC8
#define CP_BR2  0xBC
#define CP_ML2  0xCC
#define CP_MR2  0xB9
#define CP_BLK  0xDB
#define CP_RGT  0x10
#define CP_UP   0x18
#define CP_DN   0x19

#define UI_LEFT 1
#define UI_W    76

typedef struct __attribute__((packed)) {
    uint8_t used;
    uint8_t type;
    uint16_t parent;
    char name[MAX_FILENAME];
    uint32_t size;
    uint32_t first;
} DirEntry;

typedef struct {
    uint32_t seq;
    uint32_t total;
    uint32_t csec;
    uint32_t fat_lba;
    uint32_t fat_secs;
    uint32_t data_lba;
    uint32_t clusters;
    uint32_t dir_first;
    uint32_t dir_size;
    uint32_t hint;
} fs_sb_t;

typedef struct {
    unsigned long long volume_bytes;
    unsigned long long capacity_bytes;
    unsigned long long used_bytes;
    unsigned long long free_bytes;
    unsigned long long file_bytes;
    unsigned int used_percent;
    unsigned int files;
    unsigned int entries;
} fs_usage_t;

typedef struct {
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_char)(int x, int y, char c, uint8_t color);
    int  (*getkey)(void);
    void (*sleep)(unsigned int ms);
} gui_api_t;

#define FS_FILE_SECTORS 40
#define FS_MAGIC_LBA   199
#define FS_MAGIC_VALUE 0x4E774653
#define FS_SEQ_OFFSET  4
#define FS_VER_OFFSET  8
#define FS_FORMAT_VER  3

#define FS_FAT_LBA              200u
#define FS_FAT_EOC              0xFFFFFFFFu
#define FS_FAT_PER_SECTOR       128u
#define FS_CLUSTER_MIN_SECTORS  8u
#define FS_MAX_CLUSTERS         0x00100000u
#define FS_MIN_SECTORS          4096u

#define SETTINGS_FILE "sys.conf"
#define SETTINGS_MAX  2048

#define SCROLLBACK_LINES 300

#define SIZE_KB 1024ULL
#define SIZE_MB (1024ULL * 1024ULL)
#define SIZE_GB (1024ULL * 1024ULL * 1024ULL)

#define BOOT_ACTION_SYSTEM 0
#define BOOT_ACTION_LIVE   1
#define BOOT_ACTION_SETUP  2

typedef struct
{
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} nw_sha256_ctx_t;

#define MENU_INNER 34

#define FS_RAM_LBAS 8192
#define SCRIPT_VAR_NAME       32
#define SCRIPT_VAR_VALUE      128
#define SCRIPT_LINE_MAX       256
#define SCRIPT_MAX_STEPS      10000
#define SCRIPT_MODE_BASH      0
#define SCRIPT_MODE_BAT       1

extern uint32_t multiboot_info_ptr;
uint8_t keyboard_get_last_event_modifiers(void);
void disk_death(void);
void game_snake(void);
void mouse_init(void);
int mouse_is_touchpad(void);
const char* mouse_device_name(void);
void putchar_os(char c);
unsigned int nwf_ram_mb(void);
unsigned long nwf_ram_total_kb(void);
unsigned long nwf_ram_used_kb(void);
unsigned long nwf_ram_free_kb(void);
int           nwf_ram_used_percent(void);
void print(const char* text);
void fs_mkdir(const char* name);
void fs_rmdir(const char* name);
void fs_rm(const char* args);
void clear(void);
void unknow_panic(unsigned int code);
void nwo_run(const char* filename);
void nwf_cpu_name(char* out);
unsigned int nwf_ram_mb(void);
unsigned int nwf_disk_used_kb(void);
unsigned int nwf_disk_files(void);
const char* nwf_random_word(void);
void read_line(char* buffer, int max);
void sleep(unsigned int ms);
unsigned int rand_simple(void);
const char* fs_current_path(void);
void set_p_shell(void);
void print_shell_p(void);
void uname_p(void);
void uname_o(void);
void uname_a(void);
void change_p_color(void);
void print_title(const char* text);
void settings_load(void);
void settings_save(void);
void cmd_settings(void);
void fs_write(const char* name, const char* text);
void fs_load(void);
void fs_save_directory(void);
int random_range(int min, int max);
void run_gui(void);
void fs_cd(const char* name);
void print_error(const char* text);
void print_success(const char* text);
void *memcpy(void *dest, const void *src, unsigned int n);
void fs_set_drive(int idx);
int  fs_get_drive(void);
void fs_format_disk(int drive_idx);
void installer(void);
void app_nwo_info(const char* filename);
void app_hexview(const char* filename);
void fs_create(const char* name);
void print_int(int number);
void game_guess(void);
void game_rps(void);
void game_word(void);
void game_coin(void);
void game_dice(void);
void game_higher_lower(void);
void game_math(void);
void game_hangman(void);
void game_tictactoe(void);
void reboot(void);
void generate_num(void);
void games_menu(void);
int strncmp(const char* a, const char* b, int n);
void nwo_run(const char* filename);
int fs_read_text(
    const char* name,
    char* buffer,
    unsigned int max_size
);
int fs_find(const char* name);
int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size);
void reboot(void);
void fs_load(void);
void fs_save_directory(void);
void uname(void);
void uname_r(void);
unsigned char rtc_read(unsigned char reg);
unsigned char bcd_to_bin(unsigned char value);
void get_time(int* hour, int* minute, int* second);
void get_date(int* day, int* month, int* year);
void print_two_digits(int value);
void render_view(void);
int fs_read_binary(
    const char* name,
    unsigned char* buffer,
    unsigned int max_size
);

extern DirEntry* directory;
extern int fs_dir_cap;
extern uint16_t current_dir;
extern unsigned char text_color;
extern unsigned char base_color;
extern unsigned char prompt_color;
extern int console_f7_enabled;
extern volatile unsigned long long idle_us_accum;
extern int shell_p;
extern char invite_user[28];
extern int cmd_sudo;
extern char cfg_user[32];
extern char cfg_os[32];
extern char cfg_hostname[64];
extern char cfg_root_salt[17];
extern char cfg_root_hash[65];
extern int cfg_setup_complete;
extern int cfg_require_login;
extern int cfg_disk_history;
extern uint16_t line_buffer[SCROLLBACK_LINES][WIDTH];
extern char cmd_history[MAX_HISTORY_COMMANDS][MAX_CMD_LENGTH];
extern int fs_current_drive;
extern int fs_quiet;
extern int fs_ram_mode;
extern uint16_t fs_removed[MAX_FILES];
extern int script_command_failed;

void terminal_new_line(void);
void *memcpy(void *dest, const void *src, unsigned int n);
void terminal_set_cell(int col, char c);
char* strcpy(char* dest, const char* src);
void render_view(void);
void cmd_mem(void);
void cmd_cpu(void);
void cmd_uptime(void);
void cmd_zones(void);
void cmd_timezone(const char* args);
void cmd_date_set(const char* args);
void cmd_time_cmd(void);
void graphics_begin(void);
void graphics_end(void);
void clear(void);
void scroll_view_up(void);
void scroll_view_down(void);
void putchar_os(char c);
void print(const char* text);
void set_color(unsigned char color);
void set_base_color(unsigned char color);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, int n);
int starts_with(const char* text, const char* prefix);
int atoi_simple(const char* text);
void cfg_strcpy(char* dest, const char* src, int max);
int normalize_username(char* s);
int normalize_hostname(char* s);
void nw_password_hash(const char* password, const char* salt, char output[65]);
void make_root_salt(char output[17]);
int root_password_is_set(void);
int verify_root_password(const char* password);
void wipe_secret(char* secret, int size);
int authenticate_root_password(void);
void wait_key(void);
void history_load(void);
void history_save(void);
void history_add_command(const char* cmd);
void read_line(char* buffer, int max);
void print_error(const char* text);
void print_success(const char* text);
void print_title(const char* text);
void delay(unsigned int count);
int random_range(int min, int max);
unsigned int rand_simple(void);
uint32_t nw_god_random_bounded(uint32_t upper_bound);
const char* nwf_random_word(void);
void random_init(void);
void game_guess(void);
void game_guessh(void);
void game_dice(void);
void game_higher_lower(void);
void game_math(void);
char to_upper_letter(char c);
void game_hangman(void);
void ttt_print_board(char board[9]);
int ttt_check_win(char board[9], char player);
int ttt_board_full(char board[9]);
void game_tictactoe(void);
void games(void);
void testH(void);
int read_int(void);
void print_int(int number);
void print_uint64(unsigned long long value);
void print_size_unit( unsigned long long bytes, unsigned long long unit, int decimals, const char* suffix );
void print_size_human(unsigned long long bytes);
void dateCr();
void game_word(void);
unsigned char rtc_read(unsigned char reg);
unsigned char bcd_to_bin(unsigned char value);
void get_time(int* hour, int* minute, int* second);
void get_date(int* day, int* month, int* year);
void print_two_digits(int value);
void nwf_cpu_name(char* out);
unsigned int nwf_ram_mb(void);
void nwfetch(void);
void reboot(void);
void poweroff(void);
int fs_read_lba(unsigned int lba, unsigned char* buf);
int fs_write_lba(unsigned int lba, const unsigned char* buf);
int fs_drive_has_fs_at(int idx, unsigned int base);
void fs_set_drive(int idx);
int fs_get_drive(void);
void fs_load(void);
void fs_save_directory(void);
int fs_sb_load(void);
void fs_sb_save(void);
void fs_fat_invalidate(void);
unsigned int fs_fat_get(unsigned int cluster);
unsigned int fs_alloc_cluster(void);
unsigned int fs_count_used_clusters(void);
void fs_free_chain(unsigned int first);
int fs_chain_read(unsigned int first, unsigned int offset,
                  unsigned char* out, unsigned int length);
int fs_chain_write(unsigned int* first, const unsigned char* data,
                   unsigned int size);
int fs_format_layout(unsigned int total_sectors);
unsigned int fs_total_sectors_here(void);
void* fs_mem_alloc(unsigned long size);
void fs_mem_free(void* ptr, unsigned long size);
int fs_dir_reserve(int needed);
int fs_dir_alloc_slot(void);
void fs_entry_clear(int index);
int fs_file_write(int index, const unsigned char* data, unsigned int size);
int fs_file_read(int index, unsigned int offset,
                 unsigned char* out, unsigned int length);
void fs_file_release(int index);
int fs_drive_has_fs(int idx);
const char* disk_position_name(ata_drive_t* d);
int fs_drive_index_by_letter(char letter);
char fs_get_drive_letter(void);
int fs_change_drive(int idx);
void change_p_color(void);
void print_shell_p(void);
void shell(void);
int fs_change_drive_letter(char letter);
unsigned long long fs_drive_used_bytes(int idx, unsigned int* file_count);
unsigned int fs_drive_entry_count(int idx);
int fs_drive_usage(int idx, fs_usage_t* usage);
int fs_usage_current(fs_usage_t* usage);
unsigned int nwf_disk_used_kb(void);
unsigned int nwf_disk_files(void);
int nwf_tz_get(void);
void nwf_tz_set(int hours);
int nwf_cpu_load(void);
int nwf_gpu_load(void);
int nwf_fs_create(const char* name);
int nwf_fs_mkdir(const char* name);
int nwf_fs_delete(const char* name);
int nwf_fs_write(const char* name, const char* text);
int nwf_fs_read(const char* name, char* buffer, unsigned int max_size);
int fs_load_status(void);
int fs_prepare_rootfs_path( const char *path, char leaf[MAX_FILENAME] );
int fs_build_home_path(char* out, int capacity);
int fs_create_user_home(void);
void fs_format_disk(int drive_idx);
int fs_install_format_partition(int drive_idx, unsigned int partition_lba);
void fs_format(void);
unsigned int fs_layout_sectors(void);
void fs_live_init(void);
int fs_find(const char* name);
int fs_resolve_dir_path(const char* path);
unsigned int fs_get_file_size(const char* name);
void print_hex_byte(unsigned char b);
void app_hexview(const char* filename);
void app_nwo_info(const char* filename);
void generate_num(void);
void games_menu(void);
int authorize_sensitive_action( const char* description, const char* confirmation );
void fs_rmdir(const char* name);
void fs_rm(const char* args);
int fs_read_text( const char* name, char* buffer, unsigned int max_size );
void init_print(const char *str);
void pre_boot_init();
void startingNWOS();
int fs_read_binary( const char* name, unsigned char* buffer, unsigned int max_size);
void t_colorgreen(void);
void t_colorred(void);
void t_colorgray(void);
void t_colorblue(void);
void t_colorcyan(void);
void t_colorsetter(void);
void fs_store_data(int index, const char* text, const char* success_message);
void fs_write(const char* name, const char* text);
int fs_write_binary(const char* name, const unsigned char* data, unsigned int size);
int fs_path_is_file(const char* path);
int fs_write_binary_path(const char* path, const unsigned char* data, unsigned int size);
int fs_delete_file_path(const char* path);
int fs_run_nwo_path(const char* path);
void fs_read(const char* name);
void fs_edit(const char* name, const char* text);
void fs_delete(const char* name);
void fs_create(const char* name);
void fs_mkdir(const char* name);
void fs_ls(void);
void fs_cd(const char* name);
int fs_dir_count(void);
int fs_dir_entry(int index, const char** name, int* type, unsigned int* size);
const char* fs_current_path(void);
void cmd_passwd(void);
void fs_enter_user_home(void);
unsigned long kernel_static_memory_kb(void);
void death_screen(void);
void disk_death(void);
void run_calc(void);
void script_trim(char* text);
void cmd_bash(const char* args);
void cmd_bat(const char* args);
void format_size_buf(unsigned long long bytes, char* buf, int max_len);
int shell_execute(const char* command);
void set_p_shell(void);
int cfg_find_key(const char* buffer, const char* key, char* out, int out_max);
void settings_load(void);
void settings_save(void);
void prompt_append( char* out, int capacity, int* pos, const char* text );
const char* shell_prompt_text(void);
void cmd_settings(void);
void run_gui(void);
void print_size_bytes(unsigned long long bytes);
int installer_collect_setup(void);
void installer_profile_take(installer_profile_snapshot_t* snapshot);
void installer_profile_restore(installer_profile_snapshot_t* snapshot);
int installer_confirm_preflight(void);
int installer_verify_saved_account(void);
void cmd_install(void);
int login_prompt(void);
void kernel_main(void);

extern int fb_graphics_mode;
extern int line_start;
extern int line_count;
extern int cursor_col;
extern int view_offset;
extern int terminal_display_initialized;
extern unsigned int random_seed;
extern int history_count;
extern int history_pos;
extern int timezone_offset;
extern int custom_time_offset;
extern int use_custom_time;
extern int custom_day;
extern int custom_month;
extern int custom_year;
extern unsigned int fs_part_lba;
extern int fs_live_build;
extern unsigned char* fs_ram_disk;
extern unsigned int fs_ram_lbas;
extern fs_sb_t fs_sb;
extern int fs_sb_ok;
extern int fs_sb_committed;
extern int fs_removed_count;
extern int script_depth;
extern int script_last_status;
extern const char* const color_names[16];

void keyboard_input_test(void);
int boot_menu(int has_installed);
void boot_system(void);
void term_draw_cell(int cx, int cy, char c, unsigned char color);
int terminal_cursor_row(void);
int terminal_current_line_index(void);
void term_draw_cell_mode(int cx, int cy, char c, unsigned char color);
void terminal_draw_stored_cell(int physical_line, int col, int screen_row);
void terminal_draw_row(int logical_line, int screen_row);
void terminal_draw_cursor(void);
void terminal_prepare_output(void);
int read_password(char* buffer, int max);
void nw_sha256_transform(nw_sha256_ctx_t* ctx, const uint8_t data[64]);
void menu_border(void);
void menu_line(const char* text);
void port_outb(unsigned short port, unsigned char val);
void port_outw(unsigned short port, unsigned short val);
unsigned char port_inb(unsigned short port);
unsigned short port_inw(unsigned short port);
unsigned int acpi_u32(const unsigned char *p);
unsigned long long acpi_u64(const unsigned char *p);
int acpi_checksum_valid(const unsigned char *p, unsigned int length);
int acpi_rsdp_valid(const unsigned char *rsdp, unsigned int size);
unsigned char *acpi_phys_ptr(unsigned long long address, unsigned int minimum_length);
unsigned char *acpi_find_fadt(void);
int acpi_aml_integer(const unsigned char *aml, unsigned int limit, unsigned int *position, unsigned long long *value);
int acpi_find_s5(unsigned char *dsdt, unsigned int length, unsigned char *sleep_a, unsigned char *sleep_b);
unsigned short acpi_gas_read16(const unsigned char *gas);
int acpi_gas_write16(const unsigned char *gas, unsigned short value);
int acpi_gas_write8(const unsigned char *gas, unsigned char value);
int acpi_shutdown_from_tables(void);
void nwf_print_gpu(void);
unsigned int fs_drive_fs_base(int idx);
unsigned int fs_max_seq(void);
void fs_mark_formatted(unsigned int seq);
int fs_ensure_dir_path(const char* path);
void fs_entry_release(int index);
void fs_entry_release_tree(int index, int depth);
void fs_release_finish(uint16_t fallback);
int fs_name_copy(const char* name, char* buf, int max);
int fs_entry_in_tree(int index, const char* root_name);
int fs_entry_in_sys_apps_tree(int index);
int fs_name_ends_with(const char* name, const char* suffix);
int fs_entry_requires_admin(int index);
int fs_split_file_path(const char* path, char* dir_out, int dir_capacity, char* leaf_out);
int script_is_space(char c);
int script_is_name_start(char c);
int script_is_name_char(char c);
int script_has_prefix(const char* text, const char* prefix);
void script_clear_vars(void);
const char* script_get_var(const char* name);
int script_set_var(const char* name, const char* value);
int script_unset_var(const char* name);
int script_expand_vars(const char* input, char* output, unsigned int output_size, int mode);
void script_strip_bash_comment(char* line);
int script_parse_assignment(char* line, int mode);
void script_echo(char* text);
int script_run(const char* filename, int mode);
void cmd_cd_path(const char* args);
void run_nwo_from_dir( const char* dir_path, const char* filename );
void cmd_disks(void);
void cmd_listen(const char* args);
void cmd_audio(void);
int resolve_drive_arg(const char* args);
void cmd_drive(const char* args);
void cmd_drive_status(void);
void cmd_disk_info(const char* args);
void cmd_format(void);
void cmd_pwd(void);
void cmd_more_help(void);
void cmd_about(void);
void cmd_echo(const char* args);
void cmd_touch(const char* args);
void cmd_write(const char* args);
int nwc_next_token(const char* args, int* position, char* output, int output_capacity);
void cmd_nwc(const char* args);
void cmd_panic_safe(void);
void cmd_panic_fatal(void);
int command_name_length(const char* s);
int execute_single_command(char* command);
unsigned char color_from_name(const char* name);
void cfg_append(char* out, int* pos, int max, const char* key, const char* value);
int installer_read_setup_field(const char* title, const char* prompt, char* output, int capacity, const char* hint);
int installer_read_secret_field(const char* title, const char* prompt, char* output, int capacity, const char* hint);
void installer_setup_error(const char* message);
int installer_choose_login_requirement(void);

#endif
