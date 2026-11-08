#include "mmu.h"

#define MMU_PAGE_PRESENT 0x001ull
#define MMU_PAGE_USER    0x004ull
#define MMU_PAGE_PWT     0x008ull
#define MMU_PAGE_PCD     0x010ull
#define MMU_PAGE_PS      0x080ull
#define MMU_PAGE_MASK    0x000FFFFFFFFFF000ull
#define MMU_2M_SIZE      0x200000ull
#define MMU_1G_SIZE      0x40000000ull

static inline uint64_t mmu_read_cr3(void)
{
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value) : : "memory");
    return value;
}

static inline void mmu_flush_tlb(void)
{
    uint64_t cr3 = mmu_read_cr3();
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static uint64_t mmu_align_down(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1ull);
}

static uint64_t mmu_align_up(uint64_t value, uint64_t alignment)
{
    if (value > UINT64_MAX - (alignment - 1ull))
        return UINT64_MAX;
    return (value + alignment - 1ull) & ~(alignment - 1ull);
}

static void mmu_set_uncached_4k_table(uint64_t *table)
{
    for (uint64_t i = 0; i < 512ull; ++i) {
        if (table[i] & MMU_PAGE_PRESENT)
            table[i] |= MMU_PAGE_PCD | MMU_PAGE_PWT;
    }
}

void mmu_mark_mmio_uncached(uint64_t phys, uint64_t size)
{
    uint64_t start, end, addr;
    uint64_t *pml4;

    if (size == 0)
        return;

    start = mmu_align_down(phys, MMU_2M_SIZE);
    end = mmu_align_up(phys, MMU_2M_SIZE);
    if (size > UINT64_MAX - end)
        end = UINT64_MAX;
    else
        end += size;
    end = mmu_align_up(end, MMU_2M_SIZE);

    pml4 = (uint64_t *)(uintptr_t)(mmu_read_cr3() & MMU_PAGE_MASK);

    for (addr = start; addr < end; addr += MMU_2M_SIZE) {
        uint64_t i4 = (addr >> 39) & 0x1FFull;
        uint64_t i3 = (addr >> 30) & 0x1FFull;
        uint64_t i2 = (addr >> 21) & 0x1FFull;
        uint64_t e4, e3, e2;
        uint64_t *pdpt, *pd;

        e4 = pml4[i4];
        if (!(e4 & MMU_PAGE_PRESENT))
            continue;

        pdpt = (uint64_t *)(uintptr_t)(e4 & MMU_PAGE_MASK);
        e3 = pdpt[i3];
        if (!(e3 & MMU_PAGE_PRESENT))
            continue;

        if (e3 & MMU_PAGE_PS) {
            pdpt[i3] = e3 | MMU_PAGE_PCD | MMU_PAGE_PWT;
            continue;
        }

        pd = (uint64_t *)(uintptr_t)(e3 & MMU_PAGE_MASK);
        e2 = pd[i2];
        if (!(e2 & MMU_PAGE_PRESENT))
            continue;

        if (e2 & MMU_PAGE_PS) {
            pd[i2] = e2 | MMU_PAGE_PCD | MMU_PAGE_PWT;
        } else {
            uint64_t *pt = (uint64_t *)(uintptr_t)(e2 & MMU_PAGE_MASK);
            mmu_set_uncached_4k_table(pt);
        }
    }

    mmu_flush_tlb();
}

void mmu_mark_user(uint64_t phys, uint64_t size)
{
    uint64_t start, end, addr;
    uint64_t *pml4;

    if (size == 0)
        return;

    start = mmu_align_down(phys, 4096ull);
    end = mmu_align_up(phys, 4096ull);
    if (size > UINT64_MAX - end)
        end = UINT64_MAX;
    else
        end += size;
    end = mmu_align_up(end, 4096ull);

    pml4 = (uint64_t *)(uintptr_t)(mmu_read_cr3() & MMU_PAGE_MASK);

    for (addr = start; addr < end; addr += 4096ull) {
        uint64_t i4 = (addr >> 39) & 0x1FFull;
        uint64_t i3 = (addr >> 30) & 0x1FFull;
        uint64_t i2 = (addr >> 21) & 0x1FFull;
        uint64_t i1 = (addr >> 12) & 0x1FFull;
        uint64_t e4, e3, e2;
        uint64_t *pdpt, *pd;

        e4 = pml4[i4];
        if (!(e4 & MMU_PAGE_PRESENT))
            continue;
        pml4[i4] = e4 | MMU_PAGE_USER;

        pdpt = (uint64_t *)(uintptr_t)(e4 & MMU_PAGE_MASK);
        e3 = pdpt[i3];
        if (!(e3 & MMU_PAGE_PRESENT))
            continue;
        pdpt[i3] = e3 | MMU_PAGE_USER;

        if (e3 & MMU_PAGE_PS)
            continue;

        pd = (uint64_t *)(uintptr_t)(e3 & MMU_PAGE_MASK);
        e2 = pd[i2];
        if (!(e2 & MMU_PAGE_PRESENT))
            continue;

        if (e2 & MMU_PAGE_PS) {
            pd[i2] = e2 | MMU_PAGE_USER;
            continue;
        }

        pd[i2] = e2 | MMU_PAGE_USER;
        uint64_t *pt = (uint64_t *)(uintptr_t)(e2 & MMU_PAGE_MASK);
        if (pt[i1] & MMU_PAGE_PRESENT)
            pt[i1] |= MMU_PAGE_USER;
    }

    mmu_flush_tlb();
}
