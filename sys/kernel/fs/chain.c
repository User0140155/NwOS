#include "nwos_internal.h"

/*
 * NwFS v3: cluster chains.
 *
 *   LBA 199            superblock (magic, version, geometry, directory chain)
 *   LBA 200 ..         FAT: one 32-bit entry per cluster (0 = free,
 *                      0xFFFFFFFF = end of chain, otherwise next cluster)
 *   after the FAT      data clusters (cluster numbers start at 1)
 *
 * File data and the directory itself are stored as cluster chains, so the
 * number of files and the size of a file are limited only by free space.
 */

fs_sb_t fs_sb;
int fs_sb_ok = 0;
int fs_sb_committed = 0;

static unsigned int fat_buf[128];
static unsigned int fat_lba_cached = 0xFFFFFFFFu;

void fs_fat_invalidate(void)
{
    fat_lba_cached = 0xFFFFFFFFu;
}

static int fat_load(unsigned int sector_index)
{
    unsigned int lba = fs_sb.fat_lba + sector_index;

    if (fat_lba_cached == lba)
        return 0;

    if (fs_read_lba(lba, (unsigned char*)fat_buf) != 0)
    {
        fat_lba_cached = 0xFFFFFFFFu;
        return -1;
    }

    fat_lba_cached = lba;
    return 0;
}

static int cluster_ok(unsigned int c)
{
    return c >= 1 && c <= fs_sb.clusters;
}

unsigned int fs_fat_get(unsigned int c)
{
    unsigned int idx;

    if (!cluster_ok(c))
        return FS_FAT_EOC;

    idx = c - 1;

    if (fat_load(idx / 128) != 0)
        return FS_FAT_EOC;

    return fat_buf[idx % 128];
}

static int fat_set(unsigned int c, unsigned int value)
{
    unsigned int idx;

    if (!cluster_ok(c))
        return -1;

    idx = c - 1;

    if (fat_load(idx / 128) != 0)
        return -1;

    fat_buf[idx % 128] = value;

    if (fs_write_lba(fs_sb.fat_lba + idx / 128, (unsigned char*)fat_buf) != 0)
    {
        fat_lba_cached = 0xFFFFFFFFu;
        return -1;
    }

    return 0;
}

unsigned int fs_alloc_cluster(void)
{
    unsigned int n = fs_sb.clusters;
    unsigned int c = fs_sb.hint;

    if (n == 0)
        return 0;

    if (c < 1 || c > n)
        c = 1;

    for (unsigned int k = 0; k < n; k++)
    {
        unsigned int idx = c - 1;

        if (fat_load(idx / 128) != 0)
            return 0;

        if (fat_buf[idx % 128] == 0)
        {
            if (fat_set(c, FS_FAT_EOC) != 0)
                return 0;

            fs_sb.hint = (c < n) ? c + 1 : 1;
            return c;
        }

        c = (c < n) ? c + 1 : 1;
    }

    return 0;
}

unsigned int fs_count_used_clusters(void)
{
    unsigned int sector[128];
    unsigned int used = 0;
    unsigned int done = 0;
    unsigned int sectors = (fs_sb.clusters + 127u) / 128u;

    for (unsigned int s = 0; s < sectors; s++)
    {
        unsigned int n = fs_sb.clusters - done;

        if (n > 128u)
            n = 128u;

        if (fs_read_lba(fs_sb.fat_lba + s, (unsigned char*)sector) != 0)
            break;

        for (unsigned int i = 0; i < n; i++)
        {
            if (sector[i] != 0)
                used++;
        }

        done += n;
    }

    return used;
}

void fs_free_chain(unsigned int first)
{
    unsigned int c = first;
    unsigned int guard = 0;

    while (cluster_ok(c) && guard++ <= fs_sb.clusters)
    {
        unsigned int next = fs_fat_get(c);

        if (fat_set(c, 0) != 0)
            return;

        if (c < fs_sb.hint)
            fs_sb.hint = c;

        c = next;
    }
}

static unsigned int cluster_lba(unsigned int c)
{
    return fs_sb.data_lba + (c - 1) * fs_sb.csec;
}

int fs_chain_read(unsigned int first, unsigned int offset,
                  unsigned char* out, unsigned int length)
{
    unsigned char tmp[512];
    unsigned int csz = fs_sb.csec * 512u;
    unsigned int c = first;
    unsigned int skip = offset / csz;
    unsigned int within = offset % csz;
    unsigned int done = 0;

    if (length == 0)
        return 0;

    while (skip > 0)
    {
        if (!cluster_ok(c))
            return -1;

        c = fs_fat_get(c);
        skip--;
    }

    while (done < length)
    {
        unsigned int sec = within / 512u;
        unsigned int so = within % 512u;

        if (!cluster_ok(c))
            return -1;

        for (; sec < fs_sb.csec && done < length; sec++, so = 0)
        {
            unsigned int chunk = 512u - so;

            if (chunk > length - done)
                chunk = length - done;

            if (fs_read_lba(cluster_lba(c) + sec, tmp) != 0)
                return -1;

            for (unsigned int i = 0; i < chunk; i++)
                out[done + i] = tmp[so + i];

            done += chunk;
        }

        within = 0;

        if (done < length)
            c = fs_fat_get(c);
    }

    return 0;
}

int fs_chain_write(unsigned int* first, const unsigned char* data,
                   unsigned int size)
{
    unsigned char tmp[512];
    unsigned int csz = fs_sb.csec * 512u;
    unsigned int need = (size == 0) ? 0 : (unsigned int)(((unsigned long long)size + csz - 1) / csz);
    unsigned int cur = *first;
    unsigned int prev = 0;
    unsigned int have = 0;
    unsigned int new_head = 0;
    unsigned int old_last = 0;
    unsigned int new_first = *first;
    unsigned int guard = 0;

    if (!cluster_ok(cur))
    {
        cur = 0;
        new_first = 0;
    }

    /* Phase 1: make the chain exactly `need` clusters long. */
    while (have < need)
    {
        if (cur != 0 && guard++ <= fs_sb.clusters)
        {
            prev = cur;
            cur = fs_fat_get(prev);

            if (!cluster_ok(cur))
                cur = 0;

            old_last = prev;
            have++;
        }
        else
        {
            unsigned int nc = fs_alloc_cluster();

            if (nc == 0)
            {
                /* Disk full: undo everything allocated in this call. */
                if (new_head != 0)
                {
                    if (old_last != 0)
                        fat_set(old_last, FS_FAT_EOC);

                    fs_free_chain(new_head);
                }

                return -1;
            }

            if (new_head == 0)
            {
                new_head = nc;
                if (prev == 0)
                    new_first = nc;
            }

            if (prev != 0 && fat_set(prev, nc) != 0)
            {
                fs_free_chain(nc);
                return -1;
            }

            prev = nc;
            cur = 0;
            have++;
        }
    }

    if (need == 0)
    {
        if (new_first != 0)
            fs_free_chain(new_first);

        *first = 0;
        return 0;
    }

    /* Trim a longer old chain. */
    if (cur != 0)
    {
        fat_set(prev, FS_FAT_EOC);
        fs_free_chain(cur);
    }

    *first = new_first;

    /* Phase 2: write the data. */
    {
        unsigned int c = new_first;
        unsigned int written = 0;
        unsigned int guard2 = 0;

        while (written < size && cluster_ok(c) && guard2++ <= fs_sb.clusters)
        {
            for (unsigned int sec = 0; sec < fs_sb.csec && written < size; sec++)
            {
                unsigned int chunk = size - written;

                if (chunk > 512u)
                    chunk = 512u;

                for (unsigned int i = 0; i < chunk; i++)
                    tmp[i] = data[written + i];

                for (unsigned int i = chunk; i < 512u; i++)
                    tmp[i] = 0;

                if (fs_write_lba(cluster_lba(c) + sec, tmp) != 0)
                    return -1;

                written += chunk;
            }

            c = fs_fat_get(c);
        }

        if (written < size)
            return -1;
    }

    return 0;
}

/* ---------------------------------------------------------------------- */
/* Superblock                                                             */
/* ---------------------------------------------------------------------- */

static unsigned int rd32(const unsigned char* p, int off)
{
    return *(const unsigned int*)(p + off);
}

static void wr32(unsigned char* p, int off, unsigned int v)
{
    *(unsigned int*)(p + off) = v;
}

int fs_sb_load(void)
{
    unsigned char sector[512];
    fs_sb_t sb;

    fs_sb_ok = 0;
    fs_fat_invalidate();

    if (fs_read_lba(FS_MAGIC_LBA, sector) != 0)
        return -1;

    if (rd32(sector, 0) != FS_MAGIC_VALUE ||
        rd32(sector, FS_VER_OFFSET) != FS_FORMAT_VER)
        return -1;

    sb.seq      = rd32(sector, FS_SEQ_OFFSET);
    sb.total    = rd32(sector, 12);
    sb.csec     = rd32(sector, 16);
    sb.fat_lba  = rd32(sector, 20);
    sb.fat_secs = rd32(sector, 24);
    sb.data_lba = rd32(sector, 28);
    sb.clusters = rd32(sector, 32);
    sb.dir_first = rd32(sector, 36);
    sb.dir_size = rd32(sector, 40);
    sb.hint     = rd32(sector, 44);

    if (sb.csec == 0 || sb.csec > 1024 || (sb.csec & (sb.csec - 1)) != 0)
        return -1;
    if (sb.fat_lba != FS_FAT_LBA || sb.data_lba <= sb.fat_lba)
        return -1;
    if (sb.clusters == 0 || sb.fat_secs < (sb.clusters + 127) / 128)
        return -1;
    if (sb.fat_lba + sb.fat_secs > sb.data_lba)
        return -1;
    if (sb.dir_first > sb.clusters)
        return -1;

    fs_sb = sb;
    fs_sb_ok = 1;
    fs_sb_committed = 1;
    return 0;
}

void fs_sb_save(void)
{
    unsigned char sector[512];

    if (!fs_sb_committed)
        return;

    for (int k = 0; k < 512; k++)
        sector[k] = 0;

    wr32(sector, 0, FS_MAGIC_VALUE);
    wr32(sector, FS_SEQ_OFFSET, fs_sb.seq);
    wr32(sector, FS_VER_OFFSET, FS_FORMAT_VER);
    wr32(sector, 12, fs_sb.total);
    wr32(sector, 16, fs_sb.csec);
    wr32(sector, 20, fs_sb.fat_lba);
    wr32(sector, 24, fs_sb.fat_secs);
    wr32(sector, 28, fs_sb.data_lba);
    wr32(sector, 32, fs_sb.clusters);
    wr32(sector, 36, fs_sb.dir_first);
    wr32(sector, 40, fs_sb.dir_size);
    wr32(sector, 44, fs_sb.hint);

    fs_write_lba(FS_MAGIC_LBA, sector);
}

/* Size of the volume that starts at fs_part_lba. */
unsigned int fs_total_sectors_here(void)
{
    if (fs_ram_mode)
        return fs_ram_lbas - fs_part_lba;

    ata_drive_t* d = ata_get_drive(fs_current_drive);
    unsigned char mbr[512];

    if (d == 0)
        return 0;

    if (fs_part_lba != 0 &&
        ata_read_sector_d(fs_current_drive, 0, mbr) == 0 &&
        mbr[510] == 0x55 && mbr[511] == 0xAA)
    {
        for (int p = 0; p < 4; p++)
        {
            unsigned char* e = mbr + 446 + p * 16;
            unsigned int start = rd32(e, 8);
            unsigned int count = rd32(e, 12);

            if (e[4] != 0 && start == fs_part_lba && count != 0)
                return count;
        }
    }

    if (d->sectors > fs_part_lba)
        return d->sectors - fs_part_lba;

    return 0;
}

int fs_format_layout(unsigned int total)
{
    unsigned char zero[512];
    unsigned int csec = FS_CLUSTER_MIN_SECTORS;
    unsigned int max_clusters;
    unsigned int fat_secs;
    unsigned int data_lba;

    if (total < FS_MIN_SECTORS)
        return -1;

    while (total / csec > FS_MAX_CLUSTERS)
        csec *= 2;

    max_clusters = total / csec;
    fat_secs = (max_clusters + 127) / 128;
    data_lba = FS_FAT_LBA + fat_secs;

    if (data_lba + csec >= total)
        return -1;

    for (int k = 0; k < 512; k++)
        zero[k] = 0;

    /* An interrupted format must not leave a valid-looking old volume. */
    fs_sb_committed = 0;
    fs_sb_ok = 0;
    fs_write_lba(FS_MAGIC_LBA, zero);

    for (unsigned int s = 0; s < fat_secs; s++)
    {
        if (fs_write_lba(FS_FAT_LBA + s, zero) != 0)
            return -1;
    }

    fs_sb.seq = 0;
    fs_sb.total = total;
    fs_sb.csec = csec;
    fs_sb.fat_lba = FS_FAT_LBA;
    fs_sb.fat_secs = fat_secs;
    fs_sb.data_lba = data_lba;
    fs_sb.clusters = (total - data_lba) / csec;
    fs_sb.dir_first = 0;
    fs_sb.dir_size = 0;
    fs_sb.hint = 1;

    fs_fat_invalidate();
    fs_sb_ok = 1;
    fs_sb_committed = 1;
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Memory for growing structures (page allocator, with graceful failure)  */
/* ---------------------------------------------------------------------- */

void* fs_mem_alloc(unsigned long size)
{
    uint64_t phys;

    if (size == 0 || !pmm_is_ready())
        return 0;

    return pmm_alloc_dma32((size_t)size, 4096, &phys);
}

void fs_mem_free(void* ptr, unsigned long size)
{
    if (ptr == 0 || size == 0 || !pmm_is_ready())
        return;

    pmm_free_pages((uintptr_t)ptr, (size_t)((size + 4095) / 4096));
}

/* ---------------------------------------------------------------------- */
/* Directory table                                                        */
/* ---------------------------------------------------------------------- */

#define FS_DIR_BOOT_ENTRIES 256

static DirEntry fs_dir_boot[FS_DIR_BOOT_ENTRIES];

DirEntry* directory = fs_dir_boot;
int fs_dir_cap = FS_DIR_BOOT_ENTRIES;

static void fs_entry_clear_at(DirEntry* entry)
{
    entry->used = 0;
    entry->type = 0;
    entry->parent = 0;
    entry->name[0] = '\0';
    entry->size = 0;
    entry->first = 0;
}

int fs_dir_reserve(int needed)
{
    int new_cap = fs_dir_cap;
    DirEntry* fresh;

    if (needed <= fs_dir_cap)
        return 0;

    while (new_cap < needed)
        new_cap *= 2;

    fresh = (DirEntry*)fs_mem_alloc((unsigned long)new_cap * sizeof(DirEntry));

    if (fresh == 0)
        return -1;

    for (int i = 0; i < fs_dir_cap; i++)
        fresh[i] = directory[i];

    for (int i = fs_dir_cap; i < new_cap; i++)
        fs_entry_clear_at(&fresh[i]);

    if (directory != fs_dir_boot)
        fs_mem_free(directory, (unsigned long)fs_dir_cap * sizeof(DirEntry));

    directory = fresh;
    fs_dir_cap = new_cap;
    return 0;
}

/* First free slot (never slot 0, which is the root); grows the table. */
int fs_dir_alloc_slot(void)
{
    for (int i = 1; i < fs_dir_cap; i++)
    {
        if (!directory[i].used)
            return i;
    }

    int old = fs_dir_cap;

    if (fs_dir_reserve(old + 1) != 0)
        return -1;

    return old;
}

void fs_entry_clear(int index)
{
    if (index < 0 || index >= fs_dir_cap)
        return;

    fs_entry_clear_at(&directory[index]);
}

/* ---------------------------------------------------------------------- */
/* File data                                                              */
/* ---------------------------------------------------------------------- */

int fs_file_write(int index, const unsigned char* data, unsigned int size)
{
    unsigned int first;

    if (index < 0 || index >= fs_dir_cap || !directory[index].used)
        return 0;

    first = directory[index].first;

    if (fs_chain_write(&first, data, size) != 0)
        return 0;

    directory[index].first = first;
    directory[index].size = size;
    return 1;
}

int fs_file_read(int index, unsigned int offset,
                 unsigned char* out, unsigned int length)
{
    unsigned int size;

    if (index < 0 || index >= fs_dir_cap || !directory[index].used)
        return 0;

    size = directory[index].size;

    if (offset >= size)
        return 0;

    if (length > size - offset)
        length = size - offset;

    if (length == 0)
        return 1;

    return fs_chain_read(directory[index].first, offset, out, length) == 0;
}

void fs_file_release(int index)
{
    if (index < 0 || index >= fs_dir_cap)
        return;

    if (directory[index].first != 0)
        fs_free_chain(directory[index].first);

    directory[index].first = 0;
}