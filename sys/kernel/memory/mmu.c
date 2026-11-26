#include "mmu.h"
#include "pmm.h"

#define MMU_PAGE_PRESENT 0x001ull
#define MMU_PAGE_RW      0x002ull
#define MMU_PAGE_USER    0x004ull
#define MMU_PAGE_PWT     0x008ull
#define MMU_PAGE_PCD     0x010ull
#define MMU_PAGE_PS      0x080ull
#define MMU_PAGE_MASK    0x000FFFFFFFFFF000ull
#define MMU_4K_SIZE      0x1000ull
#define MMU_2M_SIZE      0x200000ull
#define MMU_1G_SIZE      0x40000000ull

uint64_t mmu_read_cr3(void)
{
    uint64_t value;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(value) : : "memory");
    return value;
}

void mmu_write_cr3(uint64_t cr3)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static inline void mmu_flush_tlb(void)
{
    mmu_write_cr3(mmu_read_cr3());
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

static uint64_t *mmu_alloc_table(void)
{
    uint64_t phys = 0;
    void *p = pmm_alloc_dma32(4096, 4096, &phys);
    if (!p || phys == 0)
        return 0;
    /* pmm_alloc_dma32 zeroes memory */
    return (uint64_t *)(uintptr_t)phys;
}

uint64_t mmu_create_user_space(uint64_t phys, uint64_t size)
{
    if (size == 0)
        return 0;

    uint64_t kcr3 = mmu_read_cr3() & MMU_PAGE_MASK;
    uint64_t *kpml4 = (uint64_t *)(uintptr_t)kcr3;
    uint64_t *upml4 = mmu_alloc_table();
    if (!upml4)
        return 0;

    /* Copy kernel top-level mappings (identity map). All remain supervisor-only
       unless we explicitly mark a user range below. */
    for (int i = 0; i < 512; ++i)
        upml4[i] = kpml4[i];

    uint64_t start = mmu_align_down(phys, MMU_4K_SIZE);
    uint64_t end = phys + size;
    if (end < phys) end = UINT64_MAX;
    end = mmu_align_up(end, MMU_4K_SIZE);

    for (uint64_t addr = start; addr < end; addr += MMU_4K_SIZE) {
        uint64_t i4 = (addr >> 39) & 0x1FFull;
        uint64_t i3 = (addr >> 30) & 0x1FFull;
        uint64_t i2 = (addr >> 21) & 0x1FFull;
        uint64_t i1 = (addr >> 12) & 0x1FFull;

        uint64_t ke4 = kpml4[i4];
        if (!(ke4 & MMU_PAGE_PRESENT))
            return 0;
        uint64_t *kpdpt = (uint64_t *)(uintptr_t)(ke4 & MMU_PAGE_MASK);
        uint64_t ke3 = kpdpt[i3];
        if (!(ke3 & MMU_PAGE_PRESENT))
            return 0;

        /* Duplicate PDPT for this PML4 entry if still shared */
        uint64_t ue4 = upml4[i4];
        uint64_t *updpt;
        if ((ue4 & MMU_PAGE_MASK) == (ke4 & MMU_PAGE_MASK)) {
            updpt = mmu_alloc_table();
            if (!updpt) return 0;
            for (int j = 0; j < 512; ++j) updpt[j] = kpdpt[j];
            upml4[i4] = ((uint64_t)(uintptr_t)updpt & MMU_PAGE_MASK) |
                        (ke4 & 0xFFFull) | MMU_PAGE_USER;
            ue4 = upml4[i4];
        } else {
            updpt = (uint64_t *)(uintptr_t)(ue4 & MMU_PAGE_MASK);
            upml4[i4] = ue4 | MMU_PAGE_USER;
        }

        if (ke3 & MMU_PAGE_PS) {
            /* 1GiB page: cannot safely isolate without splitting; mark user for this range */
            updpt[i3] = ke3 | MMU_PAGE_USER;
            continue;
        }

        uint64_t *kpd = (uint64_t *)(uintptr_t)(ke3 & MMU_PAGE_MASK);
        uint64_t ke2 = kpd[i2];
        if (!(ke2 & MMU_PAGE_PRESENT))
            return 0;

        /* Duplicate PD for this PDPT entry if still shared */
        uint64_t ue3 = updpt[i3];
        uint64_t *upd;
        if ((ue3 & MMU_PAGE_MASK) == (ke3 & MMU_PAGE_MASK)) {
            upd = mmu_alloc_table();
            if (!upd) return 0;
            for (int j = 0; j < 512; ++j) upd[j] = kpd[j];
            updpt[i3] = ((uint64_t)(uintptr_t)upd & MMU_PAGE_MASK) |
                        (ke3 & 0xFFFull) | MMU_PAGE_USER;
            ue3 = updpt[i3];
        } else {
            upd = (uint64_t *)(uintptr_t)(ue3 & MMU_PAGE_MASK);
            updpt[i3] = ue3 | MMU_PAGE_USER;
        }

        uint64_t ue2 = upd[i2];
        if (ke2 & MMU_PAGE_PS) {
            /* If already split in our user space, reuse the PT. */
            if (!(ue2 & MMU_PAGE_PS) &&
                (ue2 & MMU_PAGE_MASK) != (ke2 & MMU_PAGE_MASK) &&
                (ue2 & MMU_PAGE_PRESENT)) {
                uint64_t *upt = (uint64_t *)(uintptr_t)(ue2 & MMU_PAGE_MASK);
                upd[i2] = ue2 | MMU_PAGE_USER;
                upt[i1] |= MMU_PAGE_USER;
                continue;
            }

            /* Split 2MiB page into 4KiB pages to avoid granting user access to
               unrelated memory in the same 2MiB region. */
            uint64_t base2m = ke2 & MMU_PAGE_MASK & ~0x1FFFFFull; /* 2M-aligned phys base */
            uint64_t flags_keep = (ke2 & ~MMU_PAGE_MASK) & ~MMU_PAGE_PS; /* preserve NX/cache/low flags */
            uint64_t *upt = mmu_alloc_table();
            if (!upt) return 0;
            for (uint64_t j = 0; j < 512ull; ++j) {
                uint64_t paddr = base2m + j * MMU_4K_SIZE;
                uint64_t pte = (paddr & MMU_PAGE_MASK) | flags_keep;
                pte |= MMU_PAGE_PRESENT | MMU_PAGE_RW;
                upt[j] = pte;
            }
            /* replace PD entry with pointer to PT (no PS) */
            upd[i2] = ((uint64_t)(uintptr_t)upt & MMU_PAGE_MASK) |
                      (flags_keep & 0xFFFULL) | MMU_PAGE_PRESENT | MMU_PAGE_RW | MMU_PAGE_USER;
            /* mark the specific 4k page user-accessible */
            upt[i1] |= MMU_PAGE_USER;
            continue;
        }

        uint64_t *kpt = (uint64_t *)(uintptr_t)(ke2 & MMU_PAGE_MASK);
        uint64_t ue2_cur = upd[i2];
        uint64_t *upt;
        if ((ue2_cur & MMU_PAGE_MASK) == (ke2 & MMU_PAGE_MASK)) {
            upt = mmu_alloc_table();
            if (!upt) return 0;
            for (int j = 0; j < 512; ++j) upt[j] = kpt[j];
            upd[i2] = ((uint64_t)(uintptr_t)upt & MMU_PAGE_MASK) |
                      (ke2 & 0xFFFull) | MMU_PAGE_USER;
        } else {
            upt = (uint64_t *)(uintptr_t)(ue2_cur & MMU_PAGE_MASK);
            upd[i2] = ue2_cur | MMU_PAGE_USER;
        }

        upt[i1] |= MMU_PAGE_USER;
    }

    return (uint64_t)(uintptr_t)upml4;
}
