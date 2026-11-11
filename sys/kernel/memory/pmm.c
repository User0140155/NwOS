#include "pmm.h"

#define PMM_PAGE_SHIFT       12u
#define PMM_PAGE_SIZE        (1ull << PMM_PAGE_SHIFT)
/* The bootstrap identity map covers 4 TiB (eight PML4 entries). */
#define PMM_PHYS_LIMIT       0x40000000000ull
#define PMM_DMA32_LIMIT      0x100000000ull
#define PMM_DMA16_LIMIT      0x01000000ull
#define PMM_PAGE_COUNT       (PMM_PHYS_LIMIT >> PMM_PAGE_SHIFT)
#define PMM_BITMAP_BYTES     (PMM_PAGE_COUNT / 8ull)
#define PMM_MIN_ALLOC_ADDR   0x00100000ull
#define MB2_INFO_MAX_SIZE    (16u * 1024u * 1024u)

#define MB2_TAG_END          0u
#define MB2_TAG_MODULE       3u
#define MB2_TAG_MMAP         6u
#define MB2_TAG_FRAMEBUFFER  8u
#define MB2_MEMORY_AVAILABLE 1u

static uint8_t pmm_bitmap[PMM_BITMAP_BYTES];
static uint8_t pmm_reserved_bitmap[PMM_BITMAP_BYTES];
static uint64_t pmm_free_page_count_value;
static uint64_t pmm_available_page_count_value;
static uint64_t pmm_search_hint;
static int pmm_ready;

static void pmm_fill_bytes(uint8_t *dst, uint64_t bytes, uint8_t value)
{
    __asm__ volatile("cld\n\trep stosb"
                     : "+D"(dst), "+c"(bytes)
                     : "a"((unsigned int)value)
                     : "memory");
}

extern uint32_t multiboot_info_ptr;
extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];

static uint64_t pmm_min_u64(uint64_t a, uint64_t b)
{
    return a < b ? a : b;
}

static uint64_t pmm_max_u64(uint64_t a, uint64_t b)
{
    return a > b ? a : b;
}

static uint64_t pmm_align_up(uint64_t value, uint64_t alignment)
{
    if (alignment == 0)
        return value;
    if (value > UINT64_MAX - (alignment - 1u))
        return UINT64_MAX;
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static int pmm_bit_is_set(uint64_t page)
{
    return (pmm_bitmap[page >> 3] & (uint8_t)(1u << (page & 7u))) != 0;
}

static void pmm_bit_set(uint64_t page)
{
    pmm_bitmap[page >> 3] |= (uint8_t)(1u << (page & 7u));
}

static void pmm_bit_clear(uint64_t page)
{
    pmm_bitmap[page >> 3] &= (uint8_t)~(1u << (page & 7u));
}

static int pmm_reserved_bit_is_set(uint64_t page)
{
    return (pmm_reserved_bitmap[page >> 3] & (uint8_t)(1u << (page & 7u))) != 0;
}

static void pmm_reserved_bit_set(uint64_t page)
{
    pmm_reserved_bitmap[page >> 3] |= (uint8_t)(1u << (page & 7u));
}

static void pmm_reserved_bit_clear(uint64_t page)
{
    pmm_reserved_bitmap[page >> 3] &= (uint8_t)~(1u << (page & 7u));
}

static void pmm_mark_reserved(uint64_t first_page, uint64_t end_page)
{
    if (first_page >= PMM_PAGE_COUNT)
        return;
    if (end_page > PMM_PAGE_COUNT)
        end_page = PMM_PAGE_COUNT;
    if (end_page <= first_page)
        return;

    for (uint64_t page = first_page; page < end_page; ++page) {
        pmm_bit_set(page);
        pmm_reserved_bit_set(page);
    }
}

static void pmm_mark_available(uint64_t first_page, uint64_t end_page)
{
    if (first_page >= PMM_PAGE_COUNT)
        return;
    if (end_page > PMM_PAGE_COUNT)
        end_page = PMM_PAGE_COUNT;
    if (end_page <= first_page)
        return;

    for (uint64_t page = first_page; page < end_page; ++page) {
        if (pmm_bit_is_set(page)) {
            pmm_bit_clear(page);
            ++pmm_free_page_count_value;
            ++pmm_available_page_count_value;
        }
        pmm_reserved_bit_clear(page);
    }
}

static void pmm_reserve_range_internal(uint64_t base, uint64_t length)
{
    uint64_t end;
    uint64_t first_page;
    uint64_t end_page;

    if (length == 0 || base >= PMM_PHYS_LIMIT)
        return;

    if (length > UINT64_MAX - base)
        end = UINT64_MAX;
    else
        end = base + length;

    end = pmm_min_u64(end, PMM_PHYS_LIMIT);
    first_page = base >> PMM_PAGE_SHIFT;
    end_page = (end + PMM_PAGE_SIZE - 1u) >> PMM_PAGE_SHIFT;
    if (end_page > PMM_PAGE_COUNT)
        end_page = PMM_PAGE_COUNT;

    for (uint64_t page = first_page; page < end_page; ++page) {
        if (!pmm_bit_is_set(page)) {
            pmm_bit_set(page);
            if (pmm_free_page_count_value != 0)
                --pmm_free_page_count_value;
        }
        pmm_reserved_bit_set(page);
    }
}

void pmm_reserve_range(uint64_t base, uint64_t length)
{
    if (!pmm_ready)
        return;
    pmm_reserve_range_internal(base, length);
}

static int pmm_validate_mbi(uint32_t mbi_phys, uint32_t *total_size_out)
{
    const volatile uint32_t *header;
    uint32_t total_size;

    if (mbi_phys == 0 || (mbi_phys & 7u) != 0)
        return 0;
    if ((uint64_t)mbi_phys + 8u > PMM_PHYS_LIMIT)
        return 0;

    header = (const volatile uint32_t *)(uintptr_t)mbi_phys;
    total_size = header[0];

    if (total_size < 16u || total_size > MB2_INFO_MAX_SIZE)
        return 0;
    if ((uint64_t)mbi_phys + total_size > PMM_PHYS_LIMIT)
        return 0;

    *total_size_out = total_size;
    return 1;
}

static int pmm_parse_mmap(uint8_t *mbi, uint32_t total_size)
{
    uint32_t offset = 8u;
    int found = 0;

    while (offset + 8u <= total_size) {
        volatile uint32_t *tag = (volatile uint32_t *)(mbi + offset);
        uint32_t type = tag[0];
        uint32_t size = tag[1];

        if (size < 8u || size > total_size - offset)
            return 0;
        if (type == MB2_TAG_END)
            break;

        if (type == MB2_TAG_MMAP) {
            volatile uint32_t *mmap_tag;
            uint32_t entry_size;
            uint32_t entries_bytes;

            if (size < 16u)
                return 0;

            mmap_tag = (volatile uint32_t *)(mbi + offset);
            entry_size = mmap_tag[2];
            if (entry_size < 24u)
                return 0;

            entries_bytes = size - 16u;
            if ((entries_bytes % entry_size) != 0)
                return 0;

            for (uint32_t pos = 16u; pos < size; pos += entry_size) {
                volatile uint8_t *entry = mbi + offset + pos;
                uint64_t base = *(volatile uint64_t *)(entry + 0u);
                uint64_t length = *(volatile uint64_t *)(entry + 8u);
                uint32_t memory_type = *(volatile uint32_t *)(entry + 16u);
                uint64_t end;
                uint64_t first_page;
                uint64_t end_page;

                if (memory_type != MB2_MEMORY_AVAILABLE || length == 0)
                    continue;
                if (base >= PMM_PHYS_LIMIT)
                    continue;
                if (length > UINT64_MAX - base)
                    end = UINT64_MAX;
                else
                    end = base + length;

                base = pmm_max_u64(base, PMM_MIN_ALLOC_ADDR);
                end = pmm_min_u64(end, PMM_PHYS_LIMIT);
                if (end <= base)
                    continue;

                first_page = pmm_align_up(base, PMM_PAGE_SIZE) >> PMM_PAGE_SHIFT;
                end_page = end >> PMM_PAGE_SHIFT;
                if (end_page > first_page)
                    pmm_mark_available(first_page, end_page);
            }
            found = 1;
        }

        offset = (offset + size + 7u) & ~7u;
    }

    return found;
}

static void pmm_reserve_boot_tags(uint8_t *mbi, uint32_t total_size)
{
    uint32_t offset = 8u;

    while (offset + 8u <= total_size) {
        volatile uint32_t *tag = (volatile uint32_t *)(mbi + offset);
        uint32_t type = tag[0];
        uint32_t size = tag[1];

        if (size < 8u || size > total_size - offset)
            return;
        if (type == MB2_TAG_END)
            break;

        if (type == MB2_TAG_MODULE && size >= 16u) {
            volatile uint32_t *module = (volatile uint32_t *)(mbi + offset);
            uint32_t start = module[2];
            uint32_t end = module[3];
            if (end > start)
                pmm_reserve_range_internal(start, (uint64_t)end - start);
        } else if (type == MB2_TAG_FRAMEBUFFER && size >= 32u) {
            volatile uint8_t *fb = mbi + offset;
            uint64_t address = *(volatile uint64_t *)(fb + 8u);
            uint32_t pitch = *(volatile uint32_t *)(fb + 16u);
            uint32_t height = *(volatile uint32_t *)(fb + 24u);
            uint64_t bytes = (uint64_t)pitch * height;
            if (address != 0 && bytes != 0)
                pmm_reserve_range_internal(address, bytes);
        }

        offset = (offset + size + 7u) & ~7u;
    }
}

int pmm_init(void)
{
    uint32_t mbi_phys = multiboot_info_ptr;
    uint32_t total_size = 0;
    uint8_t *mbi;

    if (pmm_ready)
        return 1;

    pmm_fill_bytes(pmm_bitmap, PMM_BITMAP_BYTES, 0xFFu);
    pmm_fill_bytes(pmm_reserved_bitmap, PMM_BITMAP_BYTES, 0xFFu);

    pmm_free_page_count_value = 0;
    pmm_available_page_count_value = 0;
    pmm_search_hint = (PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT);

    if (!pmm_validate_mbi(mbi_phys, &total_size))
        return 0;

    mbi = (uint8_t *)(uintptr_t)mbi_phys;
    if (!pmm_parse_mmap(mbi, total_size))
        return 0;

    /* Reserve low memory, all bootloader data and the complete linked image. */
    pmm_mark_reserved(0, PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT);
    pmm_reserve_range_internal((uint64_t)(uintptr_t)__kernel_start,
                               (uint64_t)(__kernel_end - __kernel_start));
    pmm_reserve_range_internal(mbi_phys, total_size);
    pmm_reserve_boot_tags(mbi, total_size);

    pmm_ready = 1;
    return 1;
}

int pmm_is_ready(void)
{
    return pmm_ready;
}

uint64_t pmm_total_bytes(void)
{
    return pmm_available_page_count_value * PMM_PAGE_SIZE;
}

uint64_t pmm_total_free_bytes(void)
{
    return pmm_free_page_count_value * PMM_PAGE_SIZE;
}

uint64_t pmm_used_bytes(void)
{
    uint64_t total = pmm_total_bytes();
    uint64_t free_bytes = pmm_total_free_bytes();

    return total > free_bytes ? total - free_bytes : 0;
}

uint64_t pmm_free_page_count(void)
{
    return pmm_free_page_count_value;
}

static uint64_t pmm_find_run(uint64_t begin, uint64_t limit,
                             uint64_t count, uint64_t alignment)
{
    uint64_t page;

    if (alignment == 0)
        alignment = 1;

    page = pmm_align_up(begin, alignment);
    while (page < limit && count <= limit - page) {
        uint64_t i;
        for (i = 0; i < count; ++i) {
            if (pmm_bit_is_set(page + i))
                break;
        }

        if (i == count)
            return page;

        page = pmm_align_up(page + i + 1u, alignment);
    }

    return PMM_PAGE_COUNT;
}

uintptr_t pmm_alloc_contiguous_pages(size_t page_count,
                                     size_t alignment_pages,
                                     uint64_t max_address)
{
    uint64_t limit_pages;
    uint64_t start;
    uint64_t found;

    if (!pmm_ready || page_count == 0)
        return 0;
    if (alignment_pages == 0)
        alignment_pages = 1;
    if ((alignment_pages & (alignment_pages - 1u)) != 0)
        return 0;

    if (max_address == 0 || max_address > PMM_PHYS_LIMIT)
        max_address = PMM_PHYS_LIMIT;
    limit_pages = max_address >> PMM_PAGE_SHIFT;
    if (limit_pages > PMM_PAGE_COUNT)
        limit_pages = PMM_PAGE_COUNT;
    if (page_count > limit_pages)
        return 0;

    start = pmm_search_hint;
    if (start < (PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT))
        start = PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT;
    if (start >= limit_pages)
        start = PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT;

    found = pmm_find_run(start, limit_pages, page_count, alignment_pages);
    if (found == PMM_PAGE_COUNT && start > (PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT))
        found = pmm_find_run(PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT,
                             start, page_count, alignment_pages);

    if (found == PMM_PAGE_COUNT)
        return 0;

    for (uint64_t i = 0; i < page_count; ++i)
        pmm_bit_set(found + i);

    if (pmm_free_page_count_value >= page_count)
        pmm_free_page_count_value -= page_count;
    else
        pmm_free_page_count_value = 0;

    pmm_search_hint = found + page_count;
    if (pmm_search_hint >= limit_pages)
        pmm_search_hint = PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT;

    return (uintptr_t)(found << PMM_PAGE_SHIFT);
}

static void *pmm_alloc_dma_below(size_t size_bytes, size_t align_bytes,
                                 uint64_t *phys_out, uint64_t limit)
{
    uint64_t pages;
    uint64_t align_pages;
    uintptr_t address;

    if (size_bytes == 0)
        return 0;
    if (align_bytes == 0)
        align_bytes = (size_t)PMM_PAGE_SIZE;
    if ((align_bytes & (align_bytes - 1u)) != 0)
        return 0;

    pages = ((uint64_t)size_bytes + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE;
    align_pages = align_bytes <= PMM_PAGE_SIZE ? 1u :
                  ((uint64_t)align_bytes + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE;

    /* pmm_alloc_contiguous_pages requires a power-of-two page alignment. */
    if ((align_pages & (align_pages - 1u)) != 0)
        return 0;

    address = pmm_alloc_contiguous_pages((size_t)pages,
                                         (size_t)align_pages,
                                         limit);
    if (address == 0)
        return 0;

    /* The bootstrap identity-maps all addresses below PMM_PHYS_LIMIT. */
    for (uint64_t i = 0; i < pages * PMM_PAGE_SIZE; ++i)
        ((volatile uint8_t *)address)[i] = 0;

    if (phys_out)
        *phys_out = (uint64_t)address;
    return (void *)address;
}

void *pmm_alloc_dma32(size_t size_bytes, size_t align_bytes,
                      uint64_t *phys_out)
{
    return pmm_alloc_dma_below(size_bytes, align_bytes, phys_out,
                               PMM_DMA32_LIMIT);
}

void *pmm_alloc_dma16(size_t size_bytes, size_t align_bytes,
                      uint64_t *phys_out)
{
    return pmm_alloc_dma_below(size_bytes, align_bytes, phys_out,
                               PMM_DMA16_LIMIT);
}

void pmm_free_pages(uintptr_t address, size_t page_count)
{
    uint64_t first_page;

    if (!pmm_ready || address == 0 || page_count == 0)
        return;
    if (((uint64_t)address & (PMM_PAGE_SIZE - 1u)) != 0)
        return;
    if ((uint64_t)address >= PMM_PHYS_LIMIT)
        return;

    first_page = (uint64_t)address >> PMM_PAGE_SHIFT;
    if (page_count > PMM_PAGE_COUNT - first_page)
        return;

    for (uint64_t i = 0; i < page_count; ++i) {
        uint64_t page = first_page + i;
        if (page < (PMM_MIN_ALLOC_ADDR >> PMM_PAGE_SHIFT))
            continue;
        if (pmm_reserved_bit_is_set(page))
            continue;
        if (pmm_bit_is_set(page)) {
            pmm_bit_clear(page);
            ++pmm_free_page_count_value;
        }
    }

    if (first_page < pmm_search_hint)
        pmm_search_hint = first_page;
}