#include "kernel.h"
#include "graphics.h"
#include "fb.h"
#include "installer.h"
#include "../../drivers/keyboard.h"
#include "time.h"
#include "../../drivers/disk.h"


static void installer_bytes_zero(unsigned char* p, unsigned int n)
{
    for (unsigned int i = 0; i < n; ++i)
        p[i] = 0;
}

#define NWOS_MBR_PARTITION_TYPE 0x7Fu
#define NWOS_MBR_PARTITION_COUNT 4
#define NWOS_MBR_ALIGNMENT 2048u

typedef struct
{
    unsigned int start_lba;
    unsigned int sector_count;
} installer_free_region_t;

typedef struct
{
    unsigned char mbr[512];
    int blank_mbr;
    int empty_slot;
    int region_count;
    installer_free_region_t regions[5];
} installer_mbr_layout_t;

static unsigned int installer_mbr_u32(const unsigned char* p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static void installer_mbr_put_u32(unsigned char* p, unsigned int value)
{
    p[0] = (unsigned char)(value & 0xFFu);
    p[1] = (unsigned char)((value >> 8) & 0xFFu);
    p[2] = (unsigned char)((value >> 16) & 0xFFu);
    p[3] = (unsigned char)((value >> 24) & 0xFFu);
}

static int installer_sector_is_zero(const unsigned char* sector)
{
    for (int i = 0; i < 512; ++i)
        if (sector[i] != 0)
            return 0;
    return 1;
}

static int installer_scan_mbr_layout(int drive_idx,
                                     unsigned long long disk_sectors,
                                     unsigned int required_sectors,
                                     installer_mbr_layout_t* layout)
{
    unsigned char sector[512];
    unsigned int starts[4];
    unsigned int counts[4];
    int count = 0;
    int has_signature;
    unsigned long long disk_limit;

    if (!layout || drive_idx < 0 || drive_idx >= ata_drive_count() ||
        required_sectors == 0 || disk_sectors < NWOS_MBR_ALIGNMENT)
        return -1;

    installer_bytes_zero((unsigned char*)layout, sizeof(*layout));
    layout->empty_slot = -1;

    if (ata_read_sector_d(drive_idx, 0, layout->mbr) != 0)
        return -1;

    /* A failed LBA 1 read is not evidence that this is an MBR disk. */
    if (ata_read_sector_d(drive_idx, 1, sector) != 0)
        return -1;
    if (sector[0] == 'E' && sector[1] == 'F' &&
        sector[2] == 'I' && sector[3] == ' ' &&
        sector[4] == 'P' && sector[5] == 'A' &&
        sector[6] == 'R' && sector[7] == 'T')
        return -2;
    /* Also detect a backup GPT header at the final LBA. */
    if (disk_sectors > 1 && disk_sectors <= 0x100000000ULL)
    {
        if (ata_read_sector_d(drive_idx, (unsigned int)(disk_sectors - 1ULL), sector) != 0)
            return -1;
        if (sector[0] == 'E' && sector[1] == 'F' &&
            sector[2] == 'I' && sector[3] == ' ' &&
            sector[4] == 'P' && sector[5] == 'A' &&
            sector[6] == 'R' && sector[7] == 'T')
            return -2;
    }

    has_signature = layout->mbr[510] == 0x55 && layout->mbr[511] == 0xAA;
    if (!has_signature)
    {
        if (!installer_sector_is_zero(layout->mbr))
            return -5;

        /* Do not mistake a superfloppy/raw NwFS volume for a blank disk. */
        if (fs_drive_has_fs_at(drive_idx, 0))
            return -5;

        /* A disk without an MBR is accepted only when its first sectors are blank. */
        for (unsigned int lba = 1; lba < 64; ++lba)
        {
            if (ata_read_sector_d(drive_idx, lba, sector) != 0 ||
                !installer_sector_is_zero(sector))
                return -5;
        }
        if (disk_sectors > NWOS_MBR_ALIGNMENT &&
            (ata_read_sector_d(drive_idx, NWOS_MBR_ALIGNMENT, sector) != 0 ||
             !installer_sector_is_zero(sector)))
            return -5;

        layout->blank_mbr = 1;
        layout->empty_slot = 0;
    }
    else
    {
        for (int i = 0; i < NWOS_MBR_PARTITION_COUNT; ++i)
        {
            unsigned char* entry = layout->mbr + 446 + i * 16;
            unsigned char boot = entry[0];
            unsigned char type = entry[4];
            unsigned int start = installer_mbr_u32(entry + 8);
            unsigned int sectors = installer_mbr_u32(entry + 12);
            int entry_is_zero = 1;

            for (int j = 0; j < 16; ++j)
                if (entry[j] != 0) { entry_is_zero = 0; break; }

            if (entry_is_zero)
            {
                if (layout->empty_slot < 0)
                    layout->empty_slot = i;
                continue;
            }

            if (boot != 0x00 && boot != 0x80)
                return -1;
            if (type == 0xEE)
                return -2;
            if (type == 0 || start == 0 || sectors == 0)
                return -1;

            if ((unsigned long long)start + sectors > disk_sectors ||
                (unsigned long long)start + sectors > 0x100000000ULL)
                return -1;

            starts[count] = start;
            counts[count] = sectors;
            count++;
        }

        if (layout->empty_slot < 0)
            return -3;

        /* An MBR with boot code but no partition records may be a raw-volume
           boot sector. Do not reinterpret it as free space. */
        if (count == 0)
        {
            for (int i = 0; i < 446; ++i)
                if (layout->mbr[i] != 0)
                    return -5;
            for (unsigned int lba = 1; lba < 64; ++lba)
            {
                if (ata_read_sector_d(drive_idx, lba, sector) != 0 ||
                    !installer_sector_is_zero(sector))
                    return -5;
            }
            if (ata_read_sector_d(drive_idx, NWOS_MBR_ALIGNMENT, sector) != 0 ||
                !installer_sector_is_zero(sector))
                return -5;
            layout->blank_mbr = 1;
        }
    }

    disk_limit = disk_sectors;
    if (disk_limit > 0x100000000ULL)
        disk_limit = 0x100000000ULL;

    /* Sort the four MBR extents by start LBA. */
    for (int i = 0; i < count; ++i)
    {
        for (int j = i + 1; j < count; ++j)
        {
            if (starts[j] < starts[i])
            {
                unsigned int t = starts[i]; starts[i] = starts[j]; starts[j] = t;
                t = counts[i]; counts[i] = counts[j]; counts[j] = t;
            }
        }
    }

    unsigned long long cursor = NWOS_MBR_ALIGNMENT;
    unsigned long long previous_part_end = 0;
    for (int i = 0; i < count; ++i)
    {
        unsigned long long part_start = starts[i];
        unsigned long long part_end = part_start + counts[i];

        if (i > 0 && part_start < previous_part_end)
            return -1; /* overlapping or corrupt MBR partition records */
        previous_part_end = part_end;

        if (part_start < cursor && part_end > cursor)
        {
            cursor = part_end;
            continue;
        }
        if (part_end <= cursor)
            continue;

        if (part_start > cursor)
        {
            unsigned long long aligned =
                (cursor + NWOS_MBR_ALIGNMENT - 1u) &
                ~((unsigned long long)NWOS_MBR_ALIGNMENT - 1u);
            if (aligned < part_start &&
                part_start - aligned >= required_sectors &&
                layout->region_count < 5)
            {
                layout->regions[layout->region_count].start_lba = (unsigned int)aligned;
                layout->regions[layout->region_count].sector_count =
                    (unsigned int)(part_start - aligned);
                layout->region_count++;
            }
        }
        cursor = part_end;
    }

    if (cursor < disk_limit)
    {
        unsigned long long aligned =
            (cursor + NWOS_MBR_ALIGNMENT - 1u) &
            ~((unsigned long long)NWOS_MBR_ALIGNMENT - 1u);
        if (aligned < disk_limit && disk_limit - aligned >= required_sectors &&
            layout->region_count < 5)
        {
            unsigned long long available = disk_limit - aligned;
            if (available > 0xFFFFFFFFULL) available = 0xFFFFFFFFULL;
            layout->regions[layout->region_count].start_lba = (unsigned int)aligned;
            layout->regions[layout->region_count].sector_count = (unsigned int)available;
            layout->region_count++;
        }
    }

    if (layout->region_count == 0)
        return -4;
    return 1;
}

/* For a disk with no partition map, verify every sector NwOS will claim. */
static int installer_verify_blank_region(int drive_idx,
                                         unsigned int start_lba,
                                         unsigned int sector_count)
{
    unsigned char sector[512];
    unsigned long long end = (unsigned long long)start_lba + sector_count;

    if (sector_count == 0 || end > 0x100000000ULL)
        return 0;

    for (unsigned long long lba = start_lba; lba < end; ++lba)
    {
        if (ata_read_sector_d(drive_idx, (unsigned int)lba, sector) != 0 ||
            !installer_sector_is_zero(sector))
            return 0;
    }
    return 1;
}

static int installer_add_mbr_partition(int drive_idx,
                                       unsigned long long disk_sectors,
                                       unsigned int start_lba,
                                       unsigned int sector_count)
{
    installer_mbr_layout_t layout;
    unsigned char expected[512];
    unsigned char verify[512];
    unsigned int required = fs_layout_sectors();
    int status;
    int fits = 0;

    if (required == 0 || sector_count < required)
        return -1;
    status = installer_scan_mbr_layout(drive_idx, disk_sectors, required, &layout);
    if (status != 1 || layout.empty_slot < 0)
        return -1;

    for (int i = 0; i < layout.region_count; ++i)
    {
        unsigned long long region_start = layout.regions[i].start_lba;
        unsigned long long region_end = region_start + layout.regions[i].sector_count;
        unsigned long long chosen_end = (unsigned long long)start_lba + sector_count;
        if (start_lba >= region_start && chosen_end <= region_end)
        {
            fits = 1;
            break;
        }
    }
    if (!fits || (unsigned long long)start_lba + sector_count > disk_sectors ||
        (unsigned long long)start_lba + sector_count > 0x100000000ULL)
        return -1;

    for (int i = 0; i < 512; ++i)
        expected[i] = layout.mbr[i];

    unsigned char* entry = expected + 446 + layout.empty_slot * 16;
    installer_bytes_zero(entry, 16);
    entry[0] = 0x00;
    entry[1] = 0xFE; entry[2] = 0xFF; entry[3] = 0xFF;
    entry[4] = NWOS_MBR_PARTITION_TYPE;
    entry[5] = 0xFE; entry[6] = 0xFF; entry[7] = 0xFF;
    installer_mbr_put_u32(entry + 8, start_lba);
    installer_mbr_put_u32(entry + 12, sector_count);
    expected[510] = 0x55;
    expected[511] = 0xAA;

    if (ata_write_sector_d(drive_idx, 0, expected) != 0 ||
        ata_read_sector_d(drive_idx, 0, verify) != 0)
        return -1;

    /* Prove the entire MBR matches the intended edit; existing boot code,
       disk signature, and all other partition entries must be preserved. */
    for (int i = 0; i < 512; ++i)
        if (verify[i] != expected[i])
            return -1;
    return 0;
}


static void installer_u64_to_text(unsigned long long value, char* out, int capacity)
{
    char reverse[24];
    int n = 0, pos = 0;
    if (!out || capacity < 2) return;
    if (value == 0) reverse[n++] = '0';
    while (value > 0 && n < (int)sizeof(reverse))
    {
        reverse[n++] = (char)('0' + (value % 10ULL));
        value /= 10ULL;
    }
    while (n > 0 && pos < capacity - 1)
        out[pos++] = reverse[--n];
    out[pos] = '\0';
}

static int installer_choose_free_region(const installer_mbr_layout_t* layout,
                                         unsigned int required,
                                         int manual,
                                         unsigned int* start_out)
{
    int selected = 0;
    if (!layout || !start_out || layout->region_count <= 0)
        return 0;

    if (!manual)
    {
        for (int i = 1; i < layout->region_count; ++i)
        {
            if (layout->regions[i].sector_count >
                layout->regions[selected].sector_count)
                selected = i;
        }
        *start_out = layout->regions[selected].start_lba;
        return 1;
    }

    while (1)
    {
        int w = 700, h = 500;
        int x = ((int)fb_get_width() - w) / 2;
        int y = ((int)fb_get_height() - h) / 2;
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Manual partition selection");
        gui_draw_text(x + 20, y + 48,
                      "Choose an unallocated MBR region. Existing partitions are untouched.",
                      COLOR_BLACK);
        gui_draw_text(x + 20, y + 72,
                      "NwOS uses its current fixed filesystem size; unused space stays free.",
                      COLOR_DARK_GRAY);

        for (int i = 0; i < layout->region_count; ++i)
        {
            int row_y = y + 115 + i * 48;
            char start_buf[32], free_buf[32], line[128];
            format_size_buf((unsigned long long)layout->regions[i].sector_count * 512ULL,
                            free_buf, sizeof(free_buf));
            /* Render numbers without relying on libc formatting. */
            unsigned int value = layout->regions[i].start_lba;
            char rev[16]; int n = 0, pos = 0;
            if (value == 0) rev[n++] = '0';
            while (value > 0 && n < (int)sizeof(rev))
            {
                rev[n++] = (char)('0' + value % 10u);
                value /= 10u;
            }
            while (n > 0) start_buf[pos++] = rev[--n];
            start_buf[pos] = '\0';

            int q = 0;
            const char* prefix = "Start LBA ";
            while (*prefix) line[q++] = *prefix++;
            for (int j = 0; start_buf[j]; ++j) line[q++] = start_buf[j];
            const char* middle = "  |  free: ";
            while (*middle) line[q++] = *middle++;
            for (int j = 0; free_buf[j] && q < (int)sizeof(line)-1; ++j) line[q++] = free_buf[j];
            line[q] = '\0';

            if (i == selected)
            {
                gui_rect(x + 12, row_y - 6, w - 24, 34, COLOR_BLUE);
                gui_draw_text(x + 24, row_y, ">", COLOR_WHITE);
                gui_draw_text(x + 46, row_y, line, COLOR_WHITE);
            }
            else
                gui_draw_text(x + 46, row_y, line, COLOR_BLACK);
        }

        gui_draw_text(x + 20, y + h - 42,
                      "Up/Down: select   Enter: choose   Esc: cancel", COLOR_DARK_GRAY);
        int key = keyboard_getkey();
        if (key == KEY_UP)
            selected = (selected - 1 + layout->region_count) % layout->region_count;
        else if (key == KEY_DOWN)
            selected = (selected + 1) % layout->region_count;
        else if (key == KEY_ENTER)
        {
            if (layout->regions[selected].sector_count < required)
                continue;
            *start_out = layout->regions[selected].start_lba;
            return 1;
        }
        else if (key == KEY_ESC || key == 27)
            return 0;
    }
}

static int installer_read_typed_word(const char* title,
                                     const char* explanation,
                                     const char* required_word)
{
    char input[24];
    int length = 0;

    if (!required_word || !required_word[0])
        return 0;
    input[0] = '\0';

    while (1)
    {
        int w = 700, h = 500;
        int x = ((int)fb_get_width() - w) / 2;
        int y = ((int)fb_get_height() - h) / 2;

        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, title);
        gui_draw_text(x + 24, y + 72, explanation, COLOR_BLACK);
        gui_draw_text(x + 24, y + 140, required_word, COLOR_RED);
        gui_rect(x + 20, y + 190, w - 40, 42, COLOR_WHITE);
        gui_draw_text(x + 32, y + 203, input, COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42,
                      "Type the word exactly   Enter: confirm   Esc: cancel",
                      COLOR_DARK_GRAY);

        int key = keyboard_getkey();
        if (key == KEY_ENTER)
        {
            input[length] = '\0';
            int match = strcmp(input, required_word) == 0;
            wipe_secret(input, sizeof(input));
            return match;
        }
        if (key == KEY_ESC || key == 27)
        {
            wipe_secret(input, sizeof(input));
            return 0;
        }
        if (key == KEY_BACKSPACE)
        {
            if (length > 0) input[--length] = '\0';
            continue;
        }
        if (key >= 32 && key <= 126 && length < (int)sizeof(input) - 1)
        {
            input[length++] = (char)key;
            input[length] = '\0';
        }
    }
}

static int installer_read_typed_confirmation(char* output, int capacity)
{
    int length = 0;
    if (!output || capacity < 2) return 0;
    output[0] = '\0';
    while (1)
    {
        int w = 700, h = 500;
        int x = ((int)fb_get_width() - w) / 2;
        int y = ((int)fb_get_height() - h) / 2;
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Final confirmation");
        gui_draw_text(x + 24, y + 70,
                      "NwOS will create one partition in the selected free region.", COLOR_BLACK);
        gui_draw_text(x + 24, y + 105,
                      "No existing partition will be formatted or moved.", COLOR_GREEN);
        gui_draw_text(x + 24, y + 160,
                      "To continue, type INSTALL exactly:", COLOR_RED);
        gui_rect(x + 20, y + 190, w - 40, 42, COLOR_WHITE);
        gui_draw_text(x + 32, y + 203, output, COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42,
                      "Enter: confirm   Backspace: delete   Esc: cancel", COLOR_DARK_GRAY);

        int key = keyboard_getkey();
        if (key == KEY_ENTER)
        {
            output[length] = '\0';
            return 1;
        }
        if (key == KEY_ESC || key == 27)
        {
            output[0] = '\0';
            return 0;
        }
        if (key == KEY_BACKSPACE)
        {
            if (length > 0) output[--length] = '\0';
            continue;
        }
        if (key >= 32 && key <= 126 && length < capacity - 1)
        {
            output[length++] = (char)key;
            output[length] = '\0';
        }
    }
}


int installer_cancelled = 0;
int installer_completed = 0;

void installer_profile_discard(installer_profile_snapshot_t* snapshot)
{
    if (!snapshot) return;
    wipe_secret(snapshot->root_salt, sizeof(snapshot->root_salt));
    wipe_secret(snapshot->root_hash, sizeof(snapshot->root_hash));
}


void installer_draw_preflight_warning(void)
{
    int w = 700, h = 500;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;
    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Important - disk safety");
    gui_draw_text(x + 24, y + 70,
                  "This installer will NOT resize or format existing partitions.", COLOR_GREEN);
    gui_draw_text(x + 24, y + 110,
                  "It can only add NwOS to unallocated MBR disk space.", COLOR_BLACK);
    gui_draw_text(x + 24, y + 150,
                  "GPT / UEFI disks are not supported by this installer yet.", COLOR_RED);
    gui_draw_text(x + 24, y + 190,
                  "Back up important files before changing any partition table.", COLOR_BLACK);
    gui_draw_text(x + 24, y + 230,
                  "If no free space exists, cancel and create unallocated space", COLOR_DARK_GRAY);
    gui_draw_text(x + 24, y + 250,
                  "with a trusted partition manager in your current operating system.", COLOR_DARK_GRAY);
    gui_draw_text(x + 24, y + h - 42,
                  "Enter: continue   Esc: return without writing to disk", COLOR_DARK_GRAY);
}

void installer(void)
{
    installer_profile_snapshot_t previous_profile;
    int profile_snapshot_ready = 0;
    int n;
    int sel = 0;
    int confirmed = 0;
    unsigned int required_sectors = fs_layout_sectors();
    unsigned int selected_start = 0;
    installer_mbr_layout_t layout;
    char confirmation[24];

    installer_cancelled = 0;
    installer_completed = 0;
    clear();
    if (required_sectors == 0)
    {
        print_error("Invalid NwFS layout size; refusing to install.\n");
        installer_cancelled = 1;
        return;
    }
    ata_init();
    n = ata_drive_count();

    int w = 700;
    int h = 500;
    int x = ((int)fb_get_width() - w) / 2;
    int y = ((int)fb_get_height() - h) / 2;

    if (!installer_confirm_preflight())
    {
        installer_cancelled = 1;
        clear();
        return;
    }

    if (n == 0)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Setup Error");
        gui_draw_text(x + 20, y + 50, "No ATA disks were detected.", COLOR_RED);
        gui_draw_text(x + 20, y + 80, "Add one in QEMU, for example:", COLOR_BLACK);
        gui_draw_text(x + 20, y + 100,
                      "-drive file=disk.img,format=raw,if=ide,index=0", COLOR_DARK_GRAY);
        sleep(3000);
        installer_cancelled = 1;
        clear();
        return;
    }

    installer_profile_take(&previous_profile);
    profile_snapshot_ready = 1;

    if (!installer_collect_setup())
    {
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    while (!confirmed)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Step 1/3 - Select target disk");
        gui_draw_text(x + 20, y + 40,
                      "Detected disks (arrows select, Enter confirms):", COLOR_BLACK);
        for (int i = 0; i < n; ++i)
        {
            ata_drive_t* d = ata_get_drive(i);
            if (!d) continue;
            int item_y = y + 80 + i * 38;
            char line[128];
            int q = 0;
            line[q++] = d->letter; line[q++] = ':'; line[q++] = ' ';
            for (int j = 0; d->model[j] && q < 85; ++j) line[q++] = d->model[j];
            line[q] = '\0';
            if (i == sel) gui_rect(x + 10, item_y - 6, w - 20, 32, COLOR_BLUE);
            gui_draw_text(x + 24, item_y, line, i == sel ? COLOR_WHITE : COLOR_BLACK);
            char size_buf[32];
            format_size_buf((unsigned long long)d->sectors * 512ULL, size_buf, sizeof(size_buf));
            gui_draw_text(x + 500, item_y, size_buf, i == sel ? COLOR_WHITE : COLOR_DARK_GRAY);
            if (fs_drive_has_fs(i))
                gui_draw_text(x + 600, item_y, "[NwOS]", i == sel ? COLOR_WHITE : COLOR_RED);
        }
        gui_draw_text(x + 20, y + h - 42, "Arrows: select   Enter: confirm   Esc: cancel", COLOR_DARK_GRAY);
        int key = keyboard_getkey();
        if (key == KEY_UP) sel = (sel - 1 + n) % n;
        else if (key == KEY_DOWN) sel = (sel + 1) % n;
        else if (key == KEY_ENTER) confirmed = 1;
        else if (key == KEY_ESC || key == 27)
        {
            installer_cancelled = 1;
            installer_profile_restore(&previous_profile);
            installer_profile_discard(&previous_profile);
            clear();
            return;
        }
    }

    ata_drive_t* target = ata_get_drive(sel);
    if (!target)
    {
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    int scan_status = installer_scan_mbr_layout(sel, target->sectors,
                                                  required_sectors, &layout);
    if (scan_status != 1)
    {
        const char* message = "Disk layout is not safe for automatic modification.";
        if (scan_status == -2)
            message = "GPT / UEFI disks are not supported yet. No changes were made.";
        else if (scan_status == -3)
            message = "All 4 MBR primary partition slots are occupied.";
        else if (scan_status == -4)
            message = "No large enough unallocated MBR region was found.";
        else if (scan_status == -5)
            message = "Unpartitioned disk contains data; refusing to overwrite it.";
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Cannot safely install on this disk");
        gui_draw_text(x + 24, y + 90, message, COLOR_RED);
        gui_draw_text(x + 24, y + 135,
                      "Create unallocated space with a partition manager, then retry.",
                      COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return. No disk sectors were written.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    int install_method = 0;
    while (1)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Step 2/3 - Partition method");
        gui_draw_text(x + 20, y + 55,
                      "Choose where NwOS should be placed:", COLOR_BLACK);
        gui_draw_text(x + 36, y + 115,
                      install_method == 0 ? "> Automatic - largest free region" :
                                           "  Automatic - largest free region",
                      install_method == 0 ? COLOR_WHITE : COLOR_BLACK);
        gui_draw_text(x + 36, y + 175,
                      install_method == 1 ? "> Manual - choose a free region" :
                                           "  Manual - choose a free region",
                      install_method == 1 ? COLOR_WHITE : COLOR_BLACK);
        gui_draw_text(x + 36, y + 235,
                      "Existing partitions will not be resized or formatted.", COLOR_GREEN);
        gui_draw_text(x + 36, y + 265,
                      "Only unallocated MBR space can be used in this version.", COLOR_DARK_GRAY);
        gui_draw_text(x + 20, y + h - 42,
                      "Up/Down: select   Enter: continue   Esc: cancel", COLOR_DARK_GRAY);
        int key = keyboard_getkey();
        if (key == KEY_UP || key == KEY_DOWN) install_method = !install_method;
        else if (key == KEY_ENTER) break;
        else if (key == KEY_ESC || key == 27)
        {
            installer_cancelled = 1;
            installer_profile_restore(&previous_profile);
            installer_profile_discard(&previous_profile);
            clear();
            return;
        }
    }

    if (!installer_choose_free_region(&layout, required_sectors,
                                      install_method, &selected_start))
    {
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    /* Use only the NwFS layout size; all remaining free space stays unallocated. */
    unsigned int selected_sectors = required_sectors;
    unsigned long long selected_end = (unsigned long long)selected_start + selected_sectors - 1u;

    if (layout.blank_mbr)
    {
        if (!installer_verify_blank_region(sel, selected_start, selected_sectors))
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Unpartitioned disk contains data");
            gui_draw_text(x + 24, y + 90,
                          "The selected NwOS region is not completely empty.", COLOR_RED);
            gui_draw_text(x + 24, y + 125,
                          "No partition table or filesystem sectors were written.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            installer_cancelled = 1;
            installer_profile_restore(&previous_profile);
            installer_profile_discard(&previous_profile);
            clear();
            return;
        }
        if (!installer_read_typed_word(
                "Blank disk confirmation",
                "No partition table was found. The selected area was scanned;\n"
                "\ndata elsewhere is not verified. Type BLANK:",
                "BLANK"))
        {
            installer_cancelled = 1;
            installer_profile_restore(&previous_profile);
            installer_profile_discard(&previous_profile);
            clear();
            return;
        }
    }

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Step 3/3 - Review installation");
    gui_draw_text(x + 24, y + 55, "Target disk:", COLOR_DARK_GRAY);
    gui_draw_text(x + 170, y + 55, target->model, COLOR_BLACK);
    gui_draw_text(x + 24, y + 100, "NwOS partition start LBA:", COLOR_DARK_GRAY);
    {
        char number[24];
        installer_u64_to_text(selected_start, number, sizeof(number));
        gui_draw_text(x + 280, y + 100, number, COLOR_BLACK);
    }
    gui_draw_text(x + 24, y + 135, "Partition size:", COLOR_DARK_GRAY);
    {
        char size_buf[32];
        format_size_buf((unsigned long long)selected_sectors * 512ULL, size_buf, sizeof(size_buf));
        gui_draw_text(x + 280, y + 135, size_buf, COLOR_BLACK);
    }
    gui_draw_text(x + 24, y + 175, "Last LBA:", COLOR_DARK_GRAY);
    {
        char number[24];
        installer_u64_to_text(selected_end, number, sizeof(number));
        gui_draw_text(x + 280, y + 175, number, COLOR_BLACK);
    }
    gui_draw_text(x + 24, y + 225,
                  "Existing partitions and their contents will be left untouched.", COLOR_GREEN);
    gui_draw_text(x + 24, y + 255,
                  "A new NwOS entry will be added to the MBR partition table.", COLOR_BLACK);
    gui_draw_text(x + 24, y + h - 42,
                  "Enter: continue to typed confirmation   Esc: cancel", COLOR_DARK_GRAY);
    {
        int key = keyboard_getkey();
        if (key != KEY_ENTER)
        {
            installer_cancelled = 1;
            installer_profile_restore(&previous_profile);
            installer_profile_discard(&previous_profile);
            clear();
            return;
        }
    }

    if (!installer_read_typed_confirmation(confirmation, sizeof(confirmation)) ||
        strcmp(confirmation, "INSTALL") != 0)
    {
        wipe_secret(confirmation, sizeof(confirmation));
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }
    wipe_secret(confirmation, sizeof(confirmation));

    /* Modify only the chosen free MBR slot. The old MBR boot code and other
       partition entries are preserved byte-for-byte. */
    if (installer_add_mbr_partition(sel, target->sectors,
                                    selected_start, selected_sectors) != 0)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Partition table write failed");
        gui_draw_text(x + 24, y + 90,
                      "Could not create the NwOS partition. No filesystem format was started.",
                      COLOR_RED);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Installing " OS_NAME);
    gui_draw_text(x + 20, y + 50, "Creating NwOS filesystem in the new partition...", COLOR_BLACK);
    gui_draw_text(x + 20, y + 80, "Existing partitions are not being formatted.", COLOR_GREEN);
    gui_rect(x + 20, y + 120, w - 40, 20, COLOR_DARK_GRAY);
    for (int i = 0; i <= w - 40; i += 5)
        gui_rect(x + 20, y + 120, i, 20, COLOR_GREEN);

    int format_status = fs_install_format_partition(sel, selected_start);

    if (format_status != 0 || !fs_drive_has_fs_at(sel, selected_start))
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Filesystem verification failed");
        gui_draw_text(x + 24, y + 80,
                      "The NwFS marker could not be read back from the target partition.", COLOR_RED);
        gui_draw_text(x + 24, y + 115,
                      "Check the virtual disk controller and write support.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return without rebooting.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    if (!installer_verify_saved_account())
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Installation verification failed");
        gui_draw_text(x + 24, y + 80,
                      "The account settings could not be read back from disk.", COLOR_RED);
        gui_draw_text(x + 24, y + 115,
                      "Do not assume the password was saved correctly.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return without rebooting.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        installer_cancelled = 1;
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
        clear();
        return;
    }

    if (profile_snapshot_ready)
        installer_profile_discard(&previous_profile);

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Installation complete");
    gui_draw_text(x + 20, y + 50, OS_NAME " was installed in the selected free region.", COLOR_GREEN);
    gui_draw_text(x + 20, y + 80, "Existing MBR partitions were preserved.", COLOR_BLACK);
    gui_draw_text(x + 20, y + 110, "Return to the boot menu and select Boot installed NwOS.", COLOR_DARK_GRAY);
    gui_draw_text(x + 20, y + 140, "Internal-disk Limine installation is a separate pending step.", COLOR_DARK_GRAY);
    gui_draw_text(x + 20, y + h - 42, "Press any key to return to the boot menu.", COLOR_DARK_GRAY);
    (void)keyboard_getkey();
    installer_completed = 1;
    installer_cancelled = 0;
    clear();
}