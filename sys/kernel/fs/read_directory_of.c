#include "nwos_internal.h"

typedef struct {
    int ram_mode;
    int drive;
    unsigned int part_lba;
    fs_sb_t sb;
    int sb_ok;
    int sb_committed;
} fs_scan_context_t;

static void fs_scan_context_save(fs_scan_context_t* ctx)
{
    ctx->ram_mode = fs_ram_mode;
    ctx->drive = fs_current_drive;
    ctx->part_lba = fs_part_lba;
    ctx->sb = fs_sb;
    ctx->sb_ok = fs_sb_ok;
    ctx->sb_committed = fs_sb_committed;
}

static void fs_scan_context_restore(const fs_scan_context_t* ctx)
{
    fs_ram_mode = ctx->ram_mode;
    fs_current_drive = ctx->drive;
    fs_part_lba = ctx->part_lba;
    fs_sb = ctx->sb;
    fs_sb_ok = ctx->sb_ok;
    fs_sb_committed = ctx->sb_committed;
    fs_fat_invalidate();
}

int fs_change_drive(int idx)
{
    if (idx < 0 || idx >= ata_drive_count()) return -1;

    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return -2;

    if (idx != fs_current_drive)
    {
        fs_current_drive = idx;
        fs_part_lba = base;
        current_dir = 0;

        fs_load();
    }
    else
    {
        fs_part_lba = base;
    }

    return 0;
}

int fs_change_drive_letter(char letter)
{
    int idx = fs_drive_index_by_letter(letter);

    if (idx < 0) return -1;

    return fs_change_drive(idx);
}

static void fs_usage_from_sb(fs_usage_t* usage)
{
    unsigned long long cluster_bytes =
        (unsigned long long)fs_sb.csec * 512ULL;

    *usage = (fs_usage_t){0};
    usage->volume_bytes = (unsigned long long)fs_sb.total * 512ULL;
    usage->capacity_bytes = (unsigned long long)fs_sb.clusters * cluster_bytes;
    usage->used_bytes =
        (unsigned long long)fs_count_used_clusters() * cluster_bytes;
    usage->free_bytes = usage->capacity_bytes > usage->used_bytes
        ? usage->capacity_bytes - usage->used_bytes : 0;

    if (usage->capacity_bytes != 0)
    {
        usage->used_percent = (unsigned int)(
            (usage->used_bytes * 100ULL + usage->capacity_bytes / 2ULL) /
            usage->capacity_bytes);
    }
}

static int fs_scan_drive(int idx, unsigned int* files,
                         unsigned long long* bytes, unsigned int* entries,
                         fs_usage_t* usage)
{
    fs_scan_context_t saved;
    unsigned int count;
    unsigned int pos;

    if (idx < 0 || idx >= ata_drive_count()) return -1;

    unsigned int base = fs_drive_fs_base(idx);

    if (base == 0xFFFFFFFFu) return -1;

    fs_scan_context_save(&saved);

    fs_ram_mode = 0;
    fs_current_drive = idx;
    fs_part_lba = base;

    if (fs_sb_load() != 0)
    {
        fs_scan_context_restore(&saved);
        return -1;
    }

    if (usage)
        fs_usage_from_sb(usage);

    count = fs_sb.dir_size / (unsigned int)sizeof(DirEntry);

    for (pos = 0; pos < count; pos += 64)
    {
        DirEntry buffer[64];
        unsigned int n = count - pos;

        if (n > 64)
            n = 64;

        if (fs_chain_read(fs_sb.dir_first, pos * (unsigned int)sizeof(DirEntry),
                          (unsigned char*)buffer,
                          n * (unsigned int)sizeof(DirEntry)) != 0)
        {
            fs_scan_context_restore(&saved);
            return -1;
        }

        for (unsigned int i = 0; i < n; i++)
        {
            unsigned int index = pos + i;

            if (!buffer[i].used) continue;

            if (index != 0)
            {
                if (entries) (*entries)++;
                if (usage) usage->entries++;
            }

            if (buffer[i].type == FS_TYPE_FILE)
            {
                if (files) (*files)++;
                if (bytes) *bytes += buffer[i].size;
                if (usage)
                {
                    usage->files++;
                    usage->file_bytes += buffer[i].size;
                }
            }
        }
    }

    fs_scan_context_restore(&saved);
    return 0;
}

unsigned long long fs_drive_used_bytes(int idx, unsigned int* file_count)
{
    unsigned long long total = 0;
    unsigned int files = 0;

    if (file_count) *file_count = 0;

    if (fs_scan_drive(idx, &files, &total, 0, 0) != 0)
        return 0;

    if (file_count) *file_count = files;

    return total;
}

unsigned int fs_drive_entry_count(int idx)
{
    unsigned int entries = 0;

    if (fs_scan_drive(idx, 0, 0, &entries, 0) != 0)
        return 0;

    return entries;
}

int fs_drive_usage(int idx, fs_usage_t* usage)
{
    if (usage == 0)
        return 0;

    return fs_scan_drive(idx, 0, 0, 0, usage) == 0;
}

int fs_usage_current(fs_usage_t* usage)
{
    if (usage == 0 || !fs_sb_ok)
        return 0;

    fs_usage_from_sb(usage);

    for (int i = 0; i < fs_dir_cap; i++)
    {
        if (!directory[i].used || i == 0)
            continue;

        usage->entries++;

        if (directory[i].type == FS_TYPE_FILE)
        {
            usage->files++;
            usage->file_bytes += directory[i].size;
        }
    }

    return 1;
}
