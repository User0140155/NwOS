#include "nwos_internal.h"
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
#define NWOS_EFI_PARTITION_TYPE 0xEFu
#define NWOS_MBR_PARTITION_COUNT 4
#define NWOS_MBR_ALIGNMENT 2048u

#define NWOS_EFI_PARTITION_SECTORS 14336u
#define NWOS_BIOS_BOOT_AREA_SECTORS 2048u
#define NWOS_EFI_IMAGE_BYTES (NWOS_EFI_PARTITION_SECTORS * 512u)
#define NWOS_BIOS_BOOT_AREA_BYTES (NWOS_BIOS_BOOT_AREA_SECTORS * 512u)
#define NWOS_MB2_INFO_MAX_SIZE (16u * 1024u * 1024u)
#define NWOS_INSTALL_MODULE_MAX_SIZE (64u * 1024u * 1024u)

#define NWOS_EFI_IMAGE_MODULE "nwos-esp-image"
#define NWOS_BIOS_AREA_MODULE "nwos-bios-boot-area"
#define NWOS_KERNEL_MODULE "nwos-kernel-image"

static void installer_u64_to_text(unsigned long long value, char* out, int capacity);

typedef struct
{
    unsigned int start_lba;
    unsigned int sector_count;
} installer_free_region_t;

typedef struct
{
    unsigned char mbr[512];
    int blank_mbr;
    int empty_slots[NWOS_MBR_PARTITION_COUNT];
    int empty_slot_count;
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
    layout->empty_slot_count = 0;

    if (ata_read_sector_d(drive_idx, 0, layout->mbr) != 0)
        return -1;

    if (ata_read_sector_d(drive_idx, 1, sector) != 0)
        return -1;
    if (sector[0] == 'E' && sector[1] == 'F' &&
        sector[2] == 'I' && sector[3] == ' ' &&
        sector[4] == 'P' && sector[5] == 'A' &&
        sector[6] == 'R' && sector[7] == 'T')
        return -2;
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

        if (fs_drive_has_fs_at(drive_idx, 0))
            return -5;

        for (unsigned int lba = 1; lba < NWOS_MBR_ALIGNMENT; ++lba)
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
        for (int i = 0; i < NWOS_MBR_PARTITION_COUNT; ++i)
            layout->empty_slots[layout->empty_slot_count++] = i;
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
                layout->empty_slots[layout->empty_slot_count++] = i;
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
        if (count == 0)
        {
            for (int i = 0; i < 446; ++i)
                if (layout->mbr[i] != 0)
                    return -5;
            for (unsigned int lba = 1; lba < NWOS_MBR_ALIGNMENT; ++lba)
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

    if (layout->empty_slot_count == 0)
        return -3;

    disk_limit = disk_sectors;
    if (disk_limit > 0x100000000ULL)
        disk_limit = 0x100000000ULL;

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
            return -1;
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

static int installer_make_whole_disk_layout(unsigned int disk_sectors,
                                             installer_mbr_layout_t* layout)
{
    if (!layout || disk_sectors <= NWOS_MBR_ALIGNMENT ||
        disk_sectors - NWOS_MBR_ALIGNMENT <
            NWOS_EFI_PARTITION_SECTORS + FS_MIN_SECTORS)
        return 0;

    installer_bytes_zero((unsigned char*)layout, sizeof(*layout));
    layout->blank_mbr = 1;
    layout->empty_slot_count = NWOS_MBR_PARTITION_COUNT;
    for (int i = 0; i < NWOS_MBR_PARTITION_COUNT; ++i)
        layout->empty_slots[i] = i;

    layout->region_count = 1;
    layout->regions[0].start_lba = NWOS_MBR_ALIGNMENT;
    layout->regions[0].sector_count = disk_sectors - NWOS_MBR_ALIGNMENT;
    return 1;
}

static unsigned int installer_region_capacity(const installer_mbr_layout_t* layout,
                                               unsigned int start_lba)
{
    if (!layout)
        return 0;
    for (int i = 0; i < layout->region_count; ++i)
        if (layout->regions[i].start_lba == start_lba)
            return layout->regions[i].sector_count;
    return 0;
}

static int installer_read_partition_size(unsigned int max_mib,
                                         unsigned int* sectors_out)
{
    char input[12];
    int length = 0;
    int invalid = 0;
    char max_text[24];

    if (!sectors_out || max_mib < (FS_MIN_SECTORS / 2048u))
        return 0;
    installer_u64_to_text(max_mib, max_text, sizeof(max_text));
    input[0] = '\0';

    while (1)
    {
        int w = 700, h = 500;
        int x = ((int)fb_get_width() - w) / 2;
        int y = ((int)fb_get_height() - h) / 2;
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "NwOS partition size");
        gui_draw_text(x + 24, y + 65,
                      "Choose how much of the selected free region NwOS may use.", COLOR_BLACK);
        gui_draw_text(x + 24, y + 100,
                      "Minimum: 2 MiB. Maximum available:", COLOR_DARK_GRAY);
        gui_draw_text(x + 330, y + 100, max_text, COLOR_BLACK);
        gui_draw_text(x + 400, y + 100, "MiB", COLOR_BLACK);
        gui_draw_text(x + 24, y + 145,
                      "Type a size in MiB. Leave empty to use all available space.", COLOR_DARK_GRAY);
        gui_rect(x + 20, y + 190, w - 40, 42, COLOR_WHITE);
        gui_draw_text(x + 32, y + 203, input, COLOR_BLACK);
        if (invalid)
            gui_draw_text(x + 24, y + 250,
                          "Invalid size. Enter a whole number within the shown range.", COLOR_RED);
        gui_draw_text(x + 24, y + h - 42,
                      "Enter: accept   Backspace: delete   Esc: cancel", COLOR_DARK_GRAY);

        int key = keyboard_getkey();
        if (key == KEY_ENTER)
        {
            unsigned long long value = 0;
            if (length == 0)
                value = max_mib;
            else
            {
                for (int i = 0; i < length; ++i)
                {
                    if (input[i] < '0' || input[i] > '9')
                    {
                        invalid = 1;
                        break;
                    }
                    value = value * 10ULL + (unsigned int)(input[i] - '0');
                    if (value > max_mib)
                    {
                        invalid = 1;
                        break;
                    }
                }
            }
            if (!invalid && value >= (FS_MIN_SECTORS / 2048u) &&
                value <= max_mib)
            {
                *sectors_out = (unsigned int)(value * 2048ULL);
                return 1;
            }
            invalid = 1;
        }
        else if (key == KEY_ESC || key == 27)
            return 0;
        else if (key == KEY_BACKSPACE)
        {
            if (length > 0)
                input[--length] = '\0';
            invalid = 0;
        }
        else if (key >= '0' && key <= '9' && length < (int)sizeof(input) - 1)
        {
            input[length++] = (char)key;
            input[length] = '\0';
            invalid = 0;
        }
    }
}

static int installer_clear_whole_disk_metadata(int drive_idx,
                                                unsigned int disk_sectors)
{
    unsigned char zero[512];
    installer_bytes_zero(zero, sizeof(zero));

    for (unsigned int lba = 1; lba <= 34u && lba < disk_sectors; ++lba)
        if (ata_write_sector_d(drive_idx, lba, zero) != 0)
            return 0;

    if (disk_sectors > 34u)
    {
        unsigned int start = disk_sectors - 34u;
        if (start <= 34u)
            start = 35u;
        for (unsigned int lba = start; lba < disk_sectors; ++lba)
            if (ata_write_sector_d(drive_idx, lba, zero) != 0)
                return 0;
    }
    return 1;
}

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

static int installer_add_mbr_partitions(int drive_idx,
                                        const installer_mbr_layout_t* layout,
                                        const unsigned int* starts,
                                        const unsigned int* sector_counts,
                                        const unsigned char* types,
                                        int entry_count,
                                        const unsigned char* bios_area)
{
    unsigned char expected[512];
    unsigned char verify[512];

    if (!layout || !starts || !sector_counts || !types ||
        entry_count < 1 || entry_count > NWOS_MBR_PARTITION_COUNT ||
        layout->empty_slot_count < entry_count)
        return -1;

    for (int i = 0; i < entry_count; ++i)
    {
        unsigned long long end = (unsigned long long)starts[i] + sector_counts[i];
        if (starts[i] == 0 || sector_counts[i] == 0 ||
            end > 0x100000000ULL || types[i] == 0)
            return -1;
    }

    for (int i = 0; i < 512; ++i)
        expected[i] = layout->mbr[i];

    if (bios_area)
        for (int i = 0; i < 440; ++i)
            expected[i] = bios_area[i];

    for (int i = 0; i < entry_count; ++i)
    {
        unsigned char* entry = expected + 446 + layout->empty_slots[i] * 16;
        installer_bytes_zero(entry, 16);
        entry[0] = (bios_area && i == 0 && types[i] == NWOS_EFI_PARTITION_TYPE)
                       ? 0x80 : 0x00;
        entry[1] = 0xFE; entry[2] = 0xFF; entry[3] = 0xFF;
        entry[4] = types[i];
        entry[5] = 0xFE; entry[6] = 0xFF; entry[7] = 0xFF;
        installer_mbr_put_u32(entry + 8, starts[i]);
        installer_mbr_put_u32(entry + 12, sector_counts[i]);
    }

    expected[510] = 0x55;
    expected[511] = 0xAA;

    if (ata_write_sector_d(drive_idx, 0, expected) != 0 ||
        ata_read_sector_d(drive_idx, 0, verify) != 0)
        return -1;

    for (int i = 0; i < 512; ++i)
        if (verify[i] != expected[i])
            return -1;
    return 0;
}

static int installer_find_existing_nwfs_partition(int drive_idx,
                                                    unsigned int* start_out,
                                                    unsigned int* sectors_out)
{
    unsigned char mbr[512];
    if (drive_idx < 0 || drive_idx >= ata_drive_count() ||
        !start_out || !sectors_out ||
        ata_read_sector_d(drive_idx, 0, mbr) != 0 ||
        mbr[510] != 0x55 || mbr[511] != 0xAA)
        return 0;

    for (int i = 0; i < NWOS_MBR_PARTITION_COUNT; ++i)
    {
        const unsigned char* entry = mbr + 446 + i * 16;
        unsigned int start = installer_mbr_u32(entry + 8);
        unsigned int count = installer_mbr_u32(entry + 12);
        if (entry[4] == 0 || start == 0 || count < FS_MIN_SECTORS)
            continue;
        if (fs_drive_has_fs_at(drive_idx, start))
        {
            *start_out = start;
            *sectors_out = count;
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    unsigned int partition_start;
    unsigned int partition_sectors;
    unsigned int fat_start;
    unsigned int root_start;
    unsigned int root_sectors;
    unsigned int data_start;
    unsigned int sectors_per_fat;
    unsigned int cluster_count;
    unsigned int root_entries;
    unsigned int reserved_sectors;
    unsigned int fat_count;
    unsigned int sectors_per_cluster;
} installer_fat16_volume_t;

typedef struct
{
    unsigned int first_cluster;
    unsigned int file_size;
    unsigned int directory_lba;
    unsigned int directory_offset;
    unsigned int allocated_sectors;
} installer_fat16_file_t;

static unsigned int installer_read_u16(const unsigned char* p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static void installer_write_u32(unsigned char* p, unsigned int value)
{
    p[0] = (unsigned char)(value & 0xFFu);
    p[1] = (unsigned char)((value >> 8) & 0xFFu);
    p[2] = (unsigned char)((value >> 16) & 0xFFu);
    p[3] = (unsigned char)((value >> 24) & 0xFFu);
}

static int installer_fat16_volume_load(int drive_idx,
                                       unsigned int partition_start,
                                       unsigned int partition_sectors,
                                       installer_fat16_volume_t* volume)
{
    unsigned char boot[512];
    unsigned int bytes_per_sector;
    unsigned int total_sectors;
    unsigned int root_bytes;
    unsigned int data_sectors;
    unsigned int cluster_count;

    if (!volume || partition_start == 0 || partition_sectors == 0 ||
        ata_read_sector_d(drive_idx, partition_start, boot) != 0 ||
        boot[510] != 0x55 || boot[511] != 0xAA)
        return 0;

    bytes_per_sector = installer_read_u16(boot + 11);
    if (bytes_per_sector != 512u || boot[13] == 0 || boot[13] > 64u ||
        (boot[13] & (boot[13] - 1u)) != 0)
        return 0;

    static const unsigned char expected_label[11] =
        {'N','W','O','S','B','O','O','T',' ',' ',' '};
    static const unsigned char expected_type[8] =
        {'F','A','T','1','6',' ',' ',' '};
    for (unsigned int i = 0; i < sizeof(expected_label); ++i)
        if (boot[43u + i] != expected_label[i])
            return 0;
    for (unsigned int i = 0; i < sizeof(expected_type); ++i)
        if (boot[54u + i] != expected_type[i])
            return 0;

    volume->reserved_sectors = installer_read_u16(boot + 14);
    volume->fat_count = boot[16];
    volume->root_entries = installer_read_u16(boot + 17);
    volume->sectors_per_fat = installer_read_u16(boot + 22);
    total_sectors = installer_read_u16(boot + 19);
    if (total_sectors == 0)
        total_sectors = installer_mbr_u32(boot + 32);

    if (volume->reserved_sectors == 0 ||
        volume->fat_count == 0 || volume->fat_count > 2 ||
        volume->root_entries == 0 || volume->sectors_per_fat == 0 ||
        total_sectors == 0 || total_sectors > partition_sectors)
        return 0;

    root_bytes = volume->root_entries * 32u;
    volume->root_sectors = (root_bytes + 511u) / 512u;
    volume->partition_start = partition_start;
    volume->partition_sectors = partition_sectors;
    volume->sectors_per_cluster = boot[13];
    volume->fat_start = partition_start + volume->reserved_sectors;
    volume->root_start = volume->fat_start +
                         volume->fat_count * volume->sectors_per_fat;
    volume->data_start = volume->root_start + volume->root_sectors;

    if (volume->data_start < partition_start ||
        volume->data_start >= partition_start + total_sectors)
        return 0;

    unsigned long long metadata_sectors =
        (unsigned long long)volume->reserved_sectors +
        (unsigned long long)volume->fat_count * volume->sectors_per_fat +
        volume->root_sectors;
    if (metadata_sectors >= total_sectors)
        return 0;
    data_sectors = total_sectors - (unsigned int)metadata_sectors;
    cluster_count = data_sectors / volume->sectors_per_cluster;
    if (cluster_count < 4085u || cluster_count >= 65525u ||
        ((unsigned long long)cluster_count + 2ULL) * 2ULL >
            (unsigned long long)volume->sectors_per_fat * 512ULL)
        return 0;
    volume->cluster_count = cluster_count;
    return 1;
}

static int installer_fat16_next_cluster(int drive_idx,
                                        const installer_fat16_volume_t* volume,
                                        unsigned int cluster,
                                        unsigned int* next_out)
{
    unsigned char fat_sector[512];
    unsigned int byte_offset;
    unsigned int fat_sector_lba;
    unsigned int value_offset;

    if (!volume || !next_out || cluster < 2u ||
        cluster >= volume->cluster_count + 2u)
        return 0;

    byte_offset = cluster * 2u;
    fat_sector_lba = volume->fat_start + byte_offset / 512u;
    value_offset = byte_offset % 512u;
    if (fat_sector_lba >= volume->fat_start + volume->sectors_per_fat ||
        ata_read_sector_d(drive_idx, fat_sector_lba, fat_sector) != 0)
        return 0;

    *next_out = installer_read_u16(fat_sector + value_offset);
    return 1;
}

static int installer_fat16_find_entry_in_sector(const unsigned char* sector,
                                                const char name[11],
                                                int want_directory,
                                                unsigned int* first_cluster,
                                                unsigned int* file_size,
                                                unsigned int* entry_offset)
{
    for (unsigned int offset = 0; offset < 512u; offset += 32u)
    {
        const unsigned char* entry = sector + offset;
        unsigned char attributes = entry[11];
        int is_directory;

        if (entry[0] == 0x00)
            return -1;
        if (entry[0] == 0xE5 || attributes == 0x0F ||
            (attributes & 0x08u))
            continue;

        is_directory = (attributes & 0x10u) != 0;
        if (is_directory != want_directory)
            continue;

        int match = 1;
        for (unsigned int i = 0; i < 11u; ++i)
            if (entry[i] != (unsigned char)name[i])
            {
                match = 0;
                break;
            }
        if (!match)
            continue;

        if (first_cluster)
            *first_cluster = installer_read_u16(entry + 26);
        if (file_size)
            *file_size = installer_mbr_u32(entry + 28);
        if (entry_offset)
            *entry_offset = offset;
        return 1;
    }
    return 0;
}

static int installer_fat16_find_boot_kernel(int drive_idx,
                                            const installer_fat16_volume_t* volume,
                                            installer_fat16_file_t* file)
{
    static const char boot_name[11] =
        {'B','O','O','T',' ',' ',' ',' ',' ',' ',' '};
    static const char kernel_name[11] =
        {'K','E','R','N','E','L',' ',' ','E','L','F'};
    unsigned char sector[512];
    unsigned int boot_cluster = 0;
    unsigned int root_end;

    if (!volume || !file)
        return 0;

    root_end = volume->root_start + volume->root_sectors;
    for (unsigned int lba = volume->root_start; lba < root_end; ++lba)
    {
        int found;
        if (ata_read_sector_d(drive_idx, lba, sector) != 0)
            return 0;
        found = installer_fat16_find_entry_in_sector(
            sector, boot_name, 1, &boot_cluster, 0, 0);
        if (found < 0)
            return 0;
        if (found == 1)
            break;
    }

    if (boot_cluster < 2u || boot_cluster >= volume->cluster_count + 2u)
        return 0;

    unsigned int cluster = boot_cluster;
    for (unsigned int visited = 0; visited < volume->cluster_count; ++visited)
    {
        unsigned int cluster_lba = volume->data_start +
            (cluster - 2u) * volume->sectors_per_cluster;
        for (unsigned int sec = 0; sec < volume->sectors_per_cluster; ++sec)
        {
            int found;
            unsigned int entry_offset = 0;
            if (ata_read_sector_d(drive_idx, cluster_lba + sec, sector) != 0)
                return 0;
            found = installer_fat16_find_entry_in_sector(
                sector, kernel_name, 0, &file->first_cluster,
                &file->file_size, &entry_offset);
            if (found < 0)
                return 0;
            if (found == 1)
            {
                file->directory_lba = cluster_lba + sec;
                file->directory_offset = entry_offset;
                return file->first_cluster >= 2u && file->file_size != 0;
            }
        }

        unsigned int next;
        if (!installer_fat16_next_cluster(drive_idx, volume, cluster, &next))
            return 0;
        if (next >= 0xFFF8u)
            break;
        if (next < 2u || next >= volume->cluster_count + 2u)
            return 0;
        cluster = next;
    }
    return 0;
}

static int installer_fat16_file_capacity(int drive_idx,
                                         const installer_fat16_volume_t* volume,
                                         installer_fat16_file_t* file)
{
    unsigned int cluster;
    unsigned int capacity = 0;

    if (!volume || !file || file->first_cluster < 2u)
        return 0;
    cluster = file->first_cluster;
    for (unsigned int visited = 0; visited < volume->cluster_count; ++visited)
    {
        capacity += volume->sectors_per_cluster;
        if (!installer_fat16_next_cluster(drive_idx, volume, cluster, &cluster))
            return 0;
        if (cluster >= 0xFFF8u)
        {
            file->allocated_sectors = capacity;
            return 1;
        }
        if (cluster < 2u || cluster >= volume->cluster_count + 2u)
            return 0;
    }
    return 0;
}

static int installer_find_existing_nwos_esp(int drive_idx,
                                             unsigned int* start_out,
                                             unsigned int* sectors_out,
                                             installer_fat16_volume_t* volume_out,
                                             installer_fat16_file_t* kernel_file_out)
{
    unsigned char mbr[512];
    int found = 0;

    ata_drive_t* drive = ata_get_drive(drive_idx);
    if (!start_out || !sectors_out || !volume_out || !kernel_file_out ||
        !drive || ata_read_sector_d(drive_idx, 0, mbr) != 0 ||
        mbr[510] != 0x55 || mbr[511] != 0xAA)
        return 0;

    for (int i = 0; i < NWOS_MBR_PARTITION_COUNT; ++i)
    {
        const unsigned char* entry = mbr + 446 + i * 16;
        unsigned int start = installer_mbr_u32(entry + 8);
        unsigned int sectors = installer_mbr_u32(entry + 12);
        installer_fat16_volume_t volume;
        installer_fat16_file_t kernel_file;

        if (entry[4] != NWOS_EFI_PARTITION_TYPE || start == 0 || sectors == 0 ||
            (unsigned long long)start + sectors > drive->sectors)
            continue;
        if (!installer_fat16_volume_load(drive_idx, start, sectors, &volume) ||
            !installer_fat16_find_boot_kernel(drive_idx, &volume, &kernel_file) ||
            !installer_fat16_file_capacity(drive_idx, &volume, &kernel_file))
            continue;

        if (found)
            return 0;
        *start_out = start;
        *sectors_out = sectors;
        *volume_out = volume;
        *kernel_file_out = kernel_file;
        found = 1;
    }
    return found;
}

static int installer_fat16_update_kernel(int drive_idx,
                                         const installer_fat16_volume_t* volume,
                                         const installer_fat16_file_t* file,
                                         const unsigned char* kernel_data,
                                         unsigned int kernel_size)
{
    unsigned int needed_sectors;
    unsigned int cluster;
    unsigned int written = 0;
    unsigned char sector[512];
    unsigned char verify[512];

    if (!volume || !file || !kernel_data || kernel_size == 0)
        return 0;
    needed_sectors = (kernel_size + 511u) / 512u;
    if (needed_sectors > file->allocated_sectors)
        return 0;

    cluster = file->first_cluster;
    while (written < needed_sectors)
    {
        unsigned int cluster_lba = volume->data_start +
            (cluster - 2u) * volume->sectors_per_cluster;
        for (unsigned int sec = 0;
             sec < volume->sectors_per_cluster && written < needed_sectors;
             ++sec, ++written)
        {
            unsigned int src_offset = written * 512u;
            unsigned int remaining = kernel_size - src_offset;
            unsigned int copy_size = remaining < 512u ? remaining : 512u;
            installer_bytes_zero(sector, sizeof(sector));
            for (unsigned int i = 0; i < copy_size; ++i)
                sector[i] = kernel_data[src_offset + i];
            if (ata_write_sector_d(drive_idx, cluster_lba + sec, sector) != 0 ||
                ata_read_sector_d(drive_idx, cluster_lba + sec, verify) != 0)
                return 0;
            for (unsigned int i = 0; i < 512u; ++i)
                if (sector[i] != verify[i])
                    return 0;
        }

        if (written < needed_sectors)
        {
            unsigned int next;
            if (!installer_fat16_next_cluster(drive_idx, volume, cluster, &next) ||
                next < 2u || next >= 0xFFF8u)
                return 0;
            cluster = next;
        }
    }

    if (ata_read_sector_d(drive_idx, file->directory_lba, sector) != 0)
        return 0;
    installer_write_u32(sector + file->directory_offset + 28u, kernel_size);
    if (ata_write_sector_d(drive_idx, file->directory_lba, sector) != 0 ||
        ata_read_sector_d(drive_idx, file->directory_lba, verify) != 0)
        return 0;
    for (unsigned int i = 0; i < 512u; ++i)
        if (sector[i] != verify[i])
            return 0;
    return 1;
}

static int installer_module_command_matches(const unsigned char* text,
                                             unsigned int capacity,
                                             const char* wanted)
{
    unsigned int i = 0;
    if (!text || !wanted) return 0;
    while (wanted[i])
    {
        if (i >= capacity || text[i] != (unsigned char)wanted[i])
            return 0;
        ++i;
    }
    return i < capacity && text[i] == 0;
}

static int installer_find_multiboot_module(const char* wanted,
                                            const unsigned char** data_out,
                                            unsigned int* size_out)
{
    const unsigned char* mbi;
    unsigned int total_size;
    unsigned int offset;
    int found_end = 0;

    if (!wanted || !data_out || !size_out || multiboot_info_ptr == 0 ||
        (multiboot_info_ptr & 7u) != 0)
        return 0;

    mbi = (const unsigned char*)(uintptr_t)multiboot_info_ptr;
    total_size = *(const unsigned int*)mbi;
    if (total_size < 16u || total_size > NWOS_MB2_INFO_MAX_SIZE)
        return 0;

    offset = 8u;
    while (offset <= total_size - 8u)
    {
        const unsigned char* tag = mbi + offset;
        unsigned int type = *(const unsigned int*)tag;
        unsigned int size = *(const unsigned int*)(tag + 4u);
        unsigned int aligned_size;

        if (size < 8u || size > total_size - offset)
            return 0;
        if (type == 0u)
        {
            if (size != 8u) return 0;
            found_end = 1;
            break;
        }

        if (type == 3u && size >= 17u &&
            installer_module_command_matches(tag + 16u, size - 16u, wanted))
        {
            unsigned int module_start = *(const unsigned int*)(tag + 8u);
            unsigned int module_end = *(const unsigned int*)(tag + 12u);
            unsigned int module_size;
            if (module_start == 0u || module_end <= module_start)
                return 0;
            module_size = module_end - module_start;
            if (module_size == 0u || module_size > NWOS_INSTALL_MODULE_MAX_SIZE)
                return 0;
            *data_out = (const unsigned char*)(uintptr_t)module_start;
            *size_out = module_size;
            return 1;
        }

        aligned_size = (size + 7u) & ~7u;
        if (aligned_size < size || aligned_size > total_size - offset)
            return 0;
        offset += aligned_size;
    }

    (void)found_end;
    return 0;
}

static int installer_boot_area_is_blank(int drive_idx,
                                         const installer_mbr_layout_t* layout,
                                         unsigned long long disk_sectors)
{
    unsigned char sector[512];
    if (!layout) return 0;
    for (int i = 0; i < 440; ++i)
        if (layout->mbr[i] != 0)
            return 0;

    if (disk_sectors < NWOS_MBR_ALIGNMENT)
        return 0;
    for (unsigned int lba = 1; lba < NWOS_MBR_ALIGNMENT; ++lba)
    {
        if (ata_read_sector_d(drive_idx, lba, sector) != 0 ||
            !installer_sector_is_zero(sector))
            return 0;
    }
    return 1;
}

static int installer_write_disk_image(int drive_idx,
                                      unsigned int start_lba,
                                      const unsigned char* image,
                                      unsigned int image_size)
{
    unsigned char sector[512];
    if (!image || image_size == 0 || (image_size & 511u) != 0 ||
        (unsigned long long)start_lba + image_size / 512u > 0x100000000ULL)
        return -1;

    for (unsigned int offset = 0; offset < image_size; offset += 512u)
    {
        unsigned char verify[512];
        for (unsigned int i = 0; i < 512u; ++i)
            sector[i] = image[offset + i];
        if (ata_write_sector_d(drive_idx, start_lba + offset / 512u, sector) != 0 ||
            ata_read_sector_d(drive_idx, start_lba + offset / 512u, verify) != 0)
            return -1;
        for (unsigned int i = 0; i < 512u; ++i)
            if (sector[i] != verify[i])
                return -1;
    }
    return 0;
}

static int installer_write_bios_boot_area(int drive_idx,
                                           const unsigned char* bios_area)
{
    if (!bios_area) return -1;
    return installer_write_disk_image(drive_idx, 1, bios_area + 512u,
                                      NWOS_BIOS_BOOT_AREA_BYTES - 512u);
}

static int installer_install_kernel_file(int drive_idx,
                                          unsigned int fs_base,
                                          const unsigned char* kernel_data,
                                          unsigned int kernel_size)
{
    int saved_dir;
    int sys_dir;
    int ok;
    unsigned int stored_size;

    if (!kernel_data || kernel_size == 0)
        return 0;

    fs_ram_mode = 0;
    fs_set_drive(drive_idx);
    fs_part_lba = fs_base;
    fs_load();
    if (!fs_sb_ok)
        return 0;

    fs_quiet = 1;
    ok = fs_write_binary_path("/sys/kernel.elf", kernel_data, kernel_size);
    fs_quiet = 0;
    if (!ok)
        return 0;

    saved_dir = current_dir;
    sys_dir = fs_resolve_dir_path("/sys");
    if (sys_dir < 0)
        return 0;
    current_dir = (uint16_t)sys_dir;
    stored_size = fs_get_file_size("kernel.elf");
    current_dir = (uint16_t)saved_dir;
    return stored_size == kernel_size;
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
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
        gui_draw_text(x + 20, y + 48,
                      "Choose an unallocated MBR region. Existing partitions are untouched.",
                      COLOR_BLACK);
        gui_draw_text(x + 20, y + 72,
                      "Choose a size after selecting this free region; unused space stays free.",
                      COLOR_DARK_GRAY);

        for (int i = 0; i < layout->region_count; ++i)
        {
            int row_y = y + 115 + i * 48;
            char start_buf[32], free_buf[32], line[128];
            format_size_buf((unsigned long long)layout->regions[i].sector_count * 512ULL,
                            free_buf, sizeof(free_buf));
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
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
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
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
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
                  "New installs use a separate FAT EFI boot partition and NwFS.", COLOR_GREEN);
    gui_draw_text(x + 24, y + 110,
                  "Updates replace only /sys/kernel.elf and EFI /BOOT/KERNEL.ELF.", COLOR_BLACK);
    gui_draw_text(x + 24, y + 145,
                  "Existing settings, user files, apps and other EFI files are preserved.", COLOR_GREEN);
    gui_draw_text(x + 24, y + 185,
                  "Whole-disk mode can replace MBR/GPT; part-disk mode needs MBR.", COLOR_RED);
    gui_draw_text(x + 24, y + 225,
                  "Back up important files before changing any partition table.", COLOR_BLACK);
    gui_draw_text(x + 24, y + 265,
                  "If no free space exists, cancel and create unallocated space", COLOR_DARK_GRAY);
    gui_draw_text(x + 24, y + 285,
                  "with a trusted partition manager in your current operating system.", COLOR_DARK_GRAY);
    gui_draw_text(x + 202, y + 580,
                  "copyright (c) User0140155 2026", COLOR_WHITE);
    gui_draw_text(x + 276, y + 595,
                  "NwOS", COLOR_WHITE);
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
    unsigned int required_sectors = FS_MIN_SECTORS;
    unsigned int selected_start = 0;
    unsigned int existing_fs_start = 0;
    unsigned int existing_fs_sectors = 0;
    unsigned int existing_esp_start = 0;
    unsigned int existing_esp_sectors = 0;
    unsigned int fs_start = 0;
    unsigned int region_sectors;
    int update_existing = 0;
    int use_whole_disk = 0;
    int bios_install = 0;
    int previous_fs_drive = fs_current_drive;
    int previous_fs_ram_mode = fs_ram_mode;
    int previous_fs_sb_ok = fs_sb_ok;
    unsigned int previous_fs_part_lba = fs_part_lba;
    uint16_t previous_current_dir = current_dir;
    installer_mbr_layout_t layout;
    installer_fat16_volume_t existing_esp_volume;
    installer_fat16_file_t existing_esp_kernel;
    const unsigned char* esp_image = 0;
    const unsigned char* bios_area = 0;
    const unsigned char* kernel_image = 0;
    unsigned int esp_image_size = 0;
    unsigned int bios_area_size = 0;
    unsigned int kernel_image_size = 0;
    char confirmation[24];

    installer_cancelled = 0;
    installer_completed = 0;
    clear();
    if (required_sectors == 0 ||
        required_sectors > 0xFFFFFFFFu - NWOS_EFI_PARTITION_SECTORS)
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
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
        sleep(3000);
        installer_cancelled = 1;
        clear();
        return;
    }

    installer_profile_take(&previous_profile);
    profile_snapshot_ready = 1;

    while (!confirmed)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Step 1/3 - Select target disk");
        gui_draw_text(x + 20, y + 40,
                      "Detected disks (arrows select, Enter confirms):", COLOR_BLACK);
        gui_draw_text(x + 202, y + 580,
                    "copyright (c) User0140155 2026", COLOR_WHITE);
        gui_draw_text(x + 276, y + 595,
                    "NwOS", COLOR_WHITE);
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
        goto install_cancel;

    update_existing = installer_find_existing_nwfs_partition(
        sel, &existing_fs_start, &existing_fs_sectors);

    if (update_existing)
    {
        int action = 0;
        while (1)
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Existing NwOS installation found");
            gui_draw_text(x + 24, y + 55,
                          "An NwOS filesystem already exists on this disk.", COLOR_BLACK);
            gui_draw_text(x + 202, y + 580,
                          "copyright (c) User0140155 2026", COLOR_WHITE);
            gui_draw_text(x + 276, y + 595, "NwOS", COLOR_WHITE);
            gui_draw_text(x + 36, y + 125,
                          action == 0 ? "> Update existing NwOS (keep files and settings)" :
                                        "  Update existing NwOS (keep files and settings)",
                          action == 0 ? COLOR_WHITE : COLOR_BLACK);
            gui_draw_text(x + 36, y + 205,
                          action == 1 ? "> Fresh installation (choose disk usage)" :
                                        "  Fresh installation (choose disk usage)",
                          action == 1 ? COLOR_RED : COLOR_BLACK);
            gui_draw_text(x + 60, y + 240,
                          "You will choose free space or whole disk on the next screen.", COLOR_DARK_GRAY);
            gui_draw_text(x + 60, y + 270,
                          "Whole-disk mode permanently deletes existing files.", COLOR_RED);
            gui_draw_text(x + 24, y + h - 42,
                          "Up/Down: select   Enter: continue   Esc: cancel", COLOR_DARK_GRAY);
            int key = keyboard_getkey();
            if (key == KEY_UP || key == KEY_DOWN)
                action = !action;
            else if (key == KEY_ENTER)
            {
                if (action == 1)
                    update_existing = 0;
                break;
            }
            else if (key == KEY_ESC || key == 27)
                goto install_cancel;
        }
    }

    if (!installer_find_multiboot_module(NWOS_KERNEL_MODULE,
                                         &kernel_image, &kernel_image_size) ||
        kernel_image_size == 0 ||
        (!update_existing &&
         (!installer_find_multiboot_module(NWOS_EFI_IMAGE_MODULE,
                                           &esp_image, &esp_image_size) ||
          esp_image_size != NWOS_EFI_IMAGE_BYTES)))
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Installer image is incomplete");
        gui_draw_text(x + 24, y + 85,
                      "The ISO is missing a required kernel or EFI image payload.", COLOR_RED);
        gui_draw_text(x + 24, y + 120,
                      "Rebuild the ISO with the updated Makefile and boot it again.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "No target-disk sectors were written. Press any key.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        goto install_cancel;
    }

    if (!update_existing)
    {
        if (!installer_collect_setup())
            goto install_cancel;
    }
    else
    {
        fs_ram_mode = 0;
        fs_set_drive(sel);
        fs_part_lba = existing_fs_start;
        fs_load();
        if (!fs_sb_ok)
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Existing NwFS is not readable");
            gui_draw_text(x + 24, y + 85,
                          "The existing system partition failed validation.", COLOR_RED);
            gui_draw_text(x + 24, y + 120,
                          "Nothing was formatted or overwritten.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        if (!installer_find_existing_nwos_esp(sel,
                                               &existing_esp_start,
                                               &existing_esp_sectors,
                                               &existing_esp_volume,
                                               &existing_esp_kernel) ||
            ((kernel_image_size + 511u) / 512u) >
                existing_esp_kernel.allocated_sectors)
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Existing EFI partition cannot be updated safely");
            gui_draw_text(x + 24, y + 85,
                          "A managed NWOSBOOT FAT16 EFI partition was not found,", COLOR_RED);
            gui_draw_text(x + 24, y + 110,
                          "or its allocated kernel file is too small for this kernel.", COLOR_RED);
            gui_draw_text(x + 24, y + 150,
                          "No files were changed. Do not format the existing NwFS.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Update existing NwOS");
        gui_draw_text(x + 24, y + 65,
                      "The existing NwFS partition will not be formatted.", COLOR_GREEN);
        gui_draw_text(x + 24, y + 100,
                      "Only /sys/kernel.elf and EFI /BOOT/KERNEL.ELF will be replaced.", COLOR_BLACK);
        gui_draw_text(x + 24, y + 135,
                      "Other NwFS files, configs, apps and EFI files will be preserved.", COLOR_GREEN);
        gui_draw_text(x + 24, y + 190,
                      "The installer will not create another partition.", COLOR_DARK_GRAY);
        gui_draw_text(x + 24, y + h - 42,
                      "Enter: continue to typed confirmation   Esc: cancel", COLOR_DARK_GRAY);
        if (keyboard_getkey() != KEY_ENTER)
            goto install_cancel;

        if (!installer_read_typed_confirmation(confirmation, sizeof(confirmation)) ||
            strcmp(confirmation, "INSTALL") != 0)
        {
            wipe_secret(confirmation, sizeof(confirmation));
            goto install_cancel;
        }
        wipe_secret(confirmation, sizeof(confirmation));

        if (!installer_fat16_update_kernel(sel, &existing_esp_volume,
                                            &existing_esp_kernel,
                                            kernel_image, kernel_image_size))
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "EFI kernel update failed");
            gui_draw_text(x + 24, y + 85,
                          "The EFI kernel could not be written and verified.", COLOR_RED);
            gui_draw_text(x + 24, y + 120,
                          "Other EFI files and the NwFS configuration were not changed.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        if (!installer_install_kernel_file(sel, existing_fs_start,
                                           kernel_image, kernel_image_size))
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "NwFS kernel update failed");
            gui_draw_text(x + 24, y + 85,
                          "EFI kernel was updated, but /sys/kernel.elf could not be verified.", COLOR_RED);
            gui_draw_text(x + 24, y + 120,
                          "No NwFS config, app or user file was intentionally removed.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        if (profile_snapshot_ready)
            installer_profile_discard(&previous_profile);
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "NwOS update complete");
        gui_draw_text(x + 24, y + 65,
                      "Both kernel copies were updated successfully.", COLOR_GREEN);
        gui_draw_text(x + 24, y + 100,
                      "NwFS settings, user files, apps and other EFI files were preserved.", COLOR_BLACK);
        gui_draw_text(x + 24, y + 135,
                      "No partition table or filesystem was reformatted.", COLOR_GREEN);
        gui_draw_text(x + 24, y + h - 42,
                      "Press any key to return.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        installer_completed = 1;
        installer_cancelled = 0;
        clear();
        return;
    }
    {
        int choice = 0;
        while (1)
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Step 2/4 - Choose disk usage");
            gui_draw_text(x + 24, y + 55,
                          "How much of this physical disk should NwOS use?", COLOR_BLACK);
            gui_draw_text(x + 202, y + 580,
                          "copyright (c) User0140155 2026", COLOR_WHITE);
            gui_draw_text(x + 276, y + 595, "NwOS", COLOR_WHITE);
            gui_draw_text(x + 36, y + 125,
                          choice == 0 ? "> Use only a free part of the disk" :
                                        "  Use only a free part of the disk",
                          choice == 0 ? COLOR_WHITE : COLOR_BLACK);
            gui_draw_text(x + 60, y + 155,
                          "Keep other partitions and their files untouched.", COLOR_DARK_GRAY);
            gui_draw_text(x + 36, y + 220,
                          choice == 1 ? "> Use the whole disk (ERASE ALL DATA)" :
                                        "  Use the whole disk (ERASE ALL DATA)",
                          choice == 1 ? COLOR_RED : COLOR_BLACK);
            gui_draw_text(x + 60, y + 250,
                          "Deletes the old partition table and all existing volumes.", COLOR_RED);
            gui_draw_text(x + 24, y + h - 42,
                          "Up/Down: select   Enter: continue   Esc: cancel", COLOR_DARK_GRAY);
            int key = keyboard_getkey();
            if (key == KEY_UP || key == KEY_DOWN)
                choice = !choice;
            else if (key == KEY_ENTER)
            {
                use_whole_disk = choice;
                break;
            }
            else if (key == KEY_ESC || key == 27)
                goto install_cancel;
        }
    }

    if (use_whole_disk)
    {
        if (!installer_make_whole_disk_layout(target->sectors, &layout))
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Disk is too small");
            gui_draw_text(x + 24, y + 90,
                          "The disk needs room for a 7 MiB EFI partition and at least 2 MiB of NwFS.",
                          COLOR_RED);
            gui_draw_text(x + 24, y + h - 42,
                          "No disk sectors were written. Press any key.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        selected_start = NWOS_MBR_ALIGNMENT;
        fs_start = selected_start + NWOS_EFI_PARTITION_SECTORS;
        required_sectors = target->sectors - fs_start;
        region_sectors = target->sectors - selected_start;
        if (required_sectors < FS_MIN_SECTORS)
            goto install_cancel;
    }
    else
    {
        region_sectors = NWOS_EFI_PARTITION_SECTORS + FS_MIN_SECTORS;
        int scan_status = installer_scan_mbr_layout(sel, target->sectors,
                                                     region_sectors, &layout);
        if (scan_status != 1)
        {
            const char* message = "Disk layout is not safe for automatic modification.";
            if (scan_status == -2)
                message = "GPT is not supported in part-disk mode. Choose whole disk to replace it.";
            else if (scan_status == -3)
                message = "No free MBR primary partition slot is available.";
            else if (scan_status == -4)
                message = "No free MBR region is large enough for EFI and the minimum NwFS size.";
            else if (scan_status == -5)
                message = "Unpartitioned disk contains data; refusing to overwrite it.";
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Cannot safely install on this disk");
            gui_draw_text(x + 24, y + 90, message, COLOR_RED);
            gui_draw_text(x + 202, y + 580,
                          "copyright (c) User0140155 2026", COLOR_WHITE);
            gui_draw_text(x + 276, y + 595, "NwOS", COLOR_WHITE);
            gui_draw_text(x + 24, y + 145,
                          "Create free MBR space or restart and choose whole-disk installation.",
                          COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42,
                          "No disk sectors were written. Press any key.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        if (layout.empty_slot_count < 2)
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Not enough MBR partition slots");
            gui_draw_text(x + 24, y + 90,
                          "NwOS needs two free MBR slots: one for EFI and one for NwFS.", COLOR_RED);
            gui_draw_text(x + 24, y + 125,
                          "Choose whole-disk mode to replace the existing partition table.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }

        int install_method = 0;
        while (1)
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Step 3/4 - Select free region");
            gui_draw_text(x + 20, y + 55,
                          "Choose where NwOS will be installed:", COLOR_BLACK);
            gui_draw_text(x + 202, y + 580,
                          "copyright (c) User0140155 2026", COLOR_WHITE);
            gui_draw_text(x + 276, y + 595, "NwOS", COLOR_WHITE);
            gui_draw_text(x + 36, y + 125,
                          install_method == 0 ? "> Automatic - largest free region" :
                                                "  Automatic - largest free region",
                          install_method == 0 ? COLOR_WHITE : COLOR_BLACK);
            gui_draw_text(x + 36, y + 185,
                          install_method == 1 ? "> Manual - choose a free region" :
                                                "  Manual - choose a free region",
                          install_method == 1 ? COLOR_WHITE : COLOR_BLACK);
            gui_draw_text(x + 36, y + 250,
                          "Existing partitions and their files will not be changed.", COLOR_GREEN);
            gui_draw_text(x + 36, y + 280,
                          "You can choose a smaller NwFS size in the next step.", COLOR_DARK_GRAY);
            gui_draw_text(x + 20, y + h - 42,
                          "Up/Down: select   Enter: continue   Esc: cancel", COLOR_DARK_GRAY);
            int key = keyboard_getkey();
            if (key == KEY_UP || key == KEY_DOWN) install_method = !install_method;
            else if (key == KEY_ENTER) break;
            else if (key == KEY_ESC || key == 27)
                goto install_cancel;
        }

        if (!installer_choose_free_region(&layout, region_sectors,
                                          install_method, &selected_start))
            goto install_cancel;

        unsigned int region_capacity =
            installer_region_capacity(&layout, selected_start);
        if (region_capacity <= NWOS_EFI_PARTITION_SECTORS + FS_MIN_SECTORS)
            goto install_cancel;

        unsigned int max_fs_sectors = region_capacity - NWOS_EFI_PARTITION_SECTORS;
        unsigned int max_mib = max_fs_sectors / 2048u;
        if (!installer_read_partition_size(max_mib, &required_sectors))
            goto install_cancel;

        region_sectors = NWOS_EFI_PARTITION_SECTORS + required_sectors;
        fs_start = selected_start + NWOS_EFI_PARTITION_SECTORS;

        if (!installer_verify_blank_region(sel, selected_start, region_sectors))
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Selected free region contains data");
            gui_draw_text(x + 24, y + 90,
                          "The selected EFI/NwFS area is not blank.", COLOR_RED);
            gui_draw_text(x + 24, y + 125,
                          "No target sectors were written. Choose another region.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }
    }

    if (layout.blank_mbr && !use_whole_disk)
    {
        if (!installer_read_typed_word(
                "Blank disk confirmation",
                "No partition table was found. Type BLANK to continue:",
                "BLANK"))
            goto install_cancel;
    }

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Step 4/4 - Review installation");
    gui_draw_text(x + 24, y + 55, "Target disk:", COLOR_DARK_GRAY);
    gui_draw_text(x + 202, y + 580,
                  "copyright (c) User0140155 2026", COLOR_WHITE);
    gui_draw_text(x + 276, y + 595,
                  "NwOS", COLOR_WHITE);
    gui_draw_text(x + 170, y + 55, target->model, COLOR_BLACK);
    gui_draw_text(x + 24, y + 100, "EFI partition start LBA:", COLOR_DARK_GRAY);
    {
        char number[24];
        installer_u64_to_text(selected_start, number, sizeof(number));
        gui_draw_text(x + 280, y + 100, number, COLOR_BLACK);
    }
    gui_draw_text(x + 24, y + 135, "EFI partition size:", COLOR_DARK_GRAY);
    {
        char size_buf[32];
        format_size_buf((unsigned long long)NWOS_EFI_PARTITION_SECTORS * 512ULL,
                        size_buf, sizeof(size_buf));
        gui_draw_text(x + 280, y + 135, size_buf, COLOR_BLACK);
    }
    if (!update_existing)
    {
        gui_draw_text(x + 24, y + 175, "NwOS partition start LBA:", COLOR_DARK_GRAY);
        {
            char number[24];
            installer_u64_to_text((unsigned long long)selected_start +
                                  NWOS_EFI_PARTITION_SECTORS,
                                  number, sizeof(number));
            gui_draw_text(x + 280, y + 175, number, COLOR_BLACK);
        }
        gui_draw_text(x + 24, y + 210, "NwFS size:", COLOR_DARK_GRAY);
        {
            char size_buf[32];
            format_size_buf((unsigned long long)required_sectors * 512ULL,
                            size_buf, sizeof(size_buf));
            gui_draw_text(x + 280, y + 210, size_buf, COLOR_BLACK);
        }
    }
    else
    {
        gui_draw_text(x + 24, y + 175,
                      "Existing NwFS will be preserved; only kernel.elf is replaced.", COLOR_GREEN);
    }
    gui_draw_text(x + 24, y + 255,
                  use_whole_disk ? "WARNING: ALL existing partitions and files on this disk will be lost." :
                                   "Other partitions, files and configuration will remain untouched.",
                  use_whole_disk ? COLOR_RED : COLOR_GREEN);
    gui_draw_text(x + 24, y + h - 42,
                  "Enter: continue to typed confirmation   Esc: cancel", COLOR_DARK_GRAY);
    {
        int key = keyboard_getkey();
        if (key != KEY_ENTER)
            goto install_cancel;
    }

    if (use_whole_disk)
    {
        if (!installer_read_typed_word(
                "Whole-disk erase confirmation",
                "This permanently replaces the partition table. Type ERASE to continue:",
                "ERASE"))
            goto install_cancel;
    }
    else
    {
        if (!installer_read_typed_confirmation(confirmation, sizeof(confirmation)) ||
            strcmp(confirmation, "INSTALL") != 0)
        {
            wipe_secret(confirmation, sizeof(confirmation));
            goto install_cancel;
        }
        wipe_secret(confirmation, sizeof(confirmation));
    }

    fs_start = update_existing ? existing_fs_start :
               selected_start + NWOS_EFI_PARTITION_SECTORS;

    if (installer_find_multiboot_module(NWOS_BIOS_AREA_MODULE,
                                        &bios_area, &bios_area_size) &&
        bios_area_size == NWOS_BIOS_BOOT_AREA_BYTES &&
        selected_start == NWOS_MBR_ALIGNMENT &&
        (use_whole_disk || installer_boot_area_is_blank(sel, &layout, target->sectors)))
        bios_install = 1;

    unsigned int partition_starts[2];
    unsigned int partition_sizes[2];
    unsigned char partition_types[2];
    int partition_count = 1;
    partition_starts[0] = selected_start;
    partition_sizes[0] = NWOS_EFI_PARTITION_SECTORS;
    partition_types[0] = NWOS_EFI_PARTITION_TYPE;
    if (!update_existing)
    {
        partition_starts[1] = fs_start;
        partition_sizes[1] = required_sectors;
        partition_types[1] = NWOS_MBR_PARTITION_TYPE;
        partition_count = 2;
    }

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, "Installing " OS_NAME);
    gui_draw_text(x + 20, y + 50, "Writing the standalone EFI boot partition...", COLOR_BLACK);
    gui_draw_text(x + 202, y + 580,
                  "copyright (c) User0140155 2026", COLOR_WHITE);
    gui_draw_text(x + 276, y + 595,
                  "NwOS", COLOR_WHITE);
    gui_draw_text(x + 20, y + 80,
                  update_existing ? "Existing NwFS data and settings are preserved." :
                  (use_whole_disk ? "Replacing the disk layout and using its remaining capacity." :
                                    "Creating NwFS in the selected free region."), COLOR_GREEN);
    gui_rect(x + 20, y + 120, w - 40, 20, COLOR_DARK_GRAY);
    for (int i = 0; i <= w - 40; i += 5)
        gui_rect(x + 20, y + 120, i, 20, COLOR_GREEN);

    if (use_whole_disk &&
        !installer_clear_whole_disk_metadata(sel, target->sectors))
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Disk metadata cleanup failed");
        gui_draw_text(x + 24, y + 90,
                      "Could not clear old GPT metadata. Installation stopped.", COLOR_RED);
        gui_draw_text(x + 24, y + h - 42,
                      "Press any key to return.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        goto install_cancel;
    }

    if (installer_write_disk_image(sel, selected_start,
                                    esp_image, esp_image_size) != 0)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "EFI image write failed");
        gui_draw_text(x + 24, y + 90,
                      "The EFI image could not be written and verified.", COLOR_RED);
        gui_draw_text(x + 24, y + 125,
                      "The NwFS partition was not formatted.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        goto install_cancel;
    }

    if (bios_install && installer_write_bios_boot_area(sel, bios_area) != 0)
        bios_install = 0;

    if (installer_add_mbr_partitions(sel, &layout,
                                     partition_starts, partition_sizes,
                                     partition_types, partition_count,
                                     bios_install ? bios_area : 0) != 0)
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Partition table write failed");
        gui_draw_text(x + 24, y + 90,
                      "Could not register the EFI/NwOS partition(s).", COLOR_RED);
        gui_draw_text(x + 24, y + 125,
                      "The old partition entries were not intentionally removed.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        goto install_cancel;
    }

    if (!update_existing)
    {
        if (fs_install_format_partition(sel, fs_start) != 0 ||
            !fs_drive_has_fs_at(sel, fs_start))
        {
            fb_clear(COLOR_BLUE);
            gui_draw_window(x, y, w, h, "Filesystem verification failed");
            gui_draw_text(x + 24, y + 80,
                          "The new NwFS partition could not be initialized.", COLOR_RED);
            gui_draw_text(x + 24, y + 115,
                          "The EFI boot partition was created, but NwOS is not installed.", COLOR_BLACK);
            gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
            (void)keyboard_getkey();
            goto install_cancel;
        }
    }

    if (!installer_install_kernel_file(sel, fs_start,
                                       kernel_image, kernel_image_size))
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Kernel installation failed");
        gui_draw_text(x + 24, y + 80,
                      "Could not save and verify /sys/kernel.elf on NwFS.", COLOR_RED);
        gui_draw_text(x + 24, y + 115,
                      "Other existing NwFS files were not deliberately changed.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        goto install_cancel;
    }

    if (!update_existing && !installer_verify_saved_account())
    {
        fb_clear(COLOR_BLUE);
        gui_draw_window(x, y, w, h, "Installation verification failed");
        gui_draw_text(x + 24, y + 80,
                      "The account settings could not be read back from disk.", COLOR_RED);
        gui_draw_text(x + 24, y + 115,
                      "Do not assume the password was saved correctly.", COLOR_BLACK);
        gui_draw_text(x + 24, y + h - 42, "Press any key to return without rebooting.", COLOR_DARK_GRAY);
        (void)keyboard_getkey();
        goto install_cancel;
    }

    if (profile_snapshot_ready)
        installer_profile_discard(&previous_profile);

    fb_clear(COLOR_BLUE);
    gui_draw_window(x, y, w, h, update_existing ? "NwOS update complete" : "Installation complete");
    gui_draw_text(x + 202, y + 580,
                  "copyright (c) User0140155 2026", COLOR_WHITE);
    gui_draw_text(x + 276, y + 595,
                  "NwOS", COLOR_WHITE);
    gui_draw_text(x + 20, y + 50,
                  update_existing ? "NwOS boot files were updated successfully." :
                                    OS_NAME " was installed successfully.", COLOR_GREEN);
    gui_draw_text(x + 20, y + 80,
                  "Kernel saved as /sys/kernel.elf on the NwFS partition.", COLOR_BLACK);
    gui_draw_text(x + 20, y + 110,
                  update_existing ? "Existing configs, apps and user files were preserved." :
                  (use_whole_disk ? "The NwFS partition uses the rest of the physical disk." :
                                    "A separate FAT EFI boot partition was created."), COLOR_BLACK);
    gui_draw_text(x + 20, y + 140,
                  bios_install ? "Boot support: Legacy BIOS + UEFI." :
                                 "Boot support: UEFI (enable EFI in VirtualBox settings).",
                  bios_install ? COLOR_GREEN : COLOR_DARK_GRAY);
    gui_draw_text(x + 20, y + 170,
                  "Remove the ISO and boot from the virtual hard disk.", COLOR_DARK_GRAY);
    gui_draw_text(x + 20, y + h - 42, "Press any key to return to the boot menu.", COLOR_DARK_GRAY);
    (void)keyboard_getkey();
    installer_completed = 1;
    installer_cancelled = 0;
    clear();
    return;

install_cancel:
    installer_cancelled = 1;
    if (profile_snapshot_ready)
    {
        installer_profile_restore(&previous_profile);
        installer_profile_discard(&previous_profile);
    }
    fs_ram_mode = previous_fs_ram_mode;
    fs_set_drive(previous_fs_drive);
    fs_part_lba = previous_fs_part_lba;
    if (previous_fs_sb_ok)
    {
        fs_load();
        current_dir = previous_current_dir;
    }
    clear();
}
