#ifndef NWOS_MMU_H
#define NWOS_MMU_H

#include <stdint.h>
#include <stddef.h>

/* Mark the identity-mapped physical range [phys, phys+size) as uncached.
 *
 * The bootstrap page tables identity-map the first 4 TiB.  RAM is mapped
 * write-back; only the legacy 3-4 GiB window is left uncached.  Device
 * registers read through a write-back mapping return stale values, so drivers
 * must explicitly make their BAR uncached before touching it.
 *
 * This function walks the current page tables (via CR3), sets PCD+PWT on the
 * PTE/PDE covering the range, and flushes the TLB.  It is safe to call for a
 * range that is already uncached.
 */
void mmu_mark_mmio_uncached(uint64_t phys, uint64_t size);
void mmu_mark_user(uint64_t phys, uint64_t size);

/* User address-space helpers for ring 3 isolation. */
uint64_t mmu_read_cr3(void);
void mmu_write_cr3(uint64_t cr3);
/* Create an isolated user address space that keeps kernel mappings supervisor-only
   and maps [phys, phys+size) as user-accessible in the user CR3. Returns user CR3
   physical address, or 0 on failure. */
uint64_t mmu_create_user_space(uint64_t phys, uint64_t size);

#endif
