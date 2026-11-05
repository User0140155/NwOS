#ifndef NWOS_MMU_H
#define NWOS_MMU_H

#include <stdint.h>
#include <stddef.h>

/* Mark the identity-mapped physical range [phys, phys+size) as uncached.
 *
 * The bootstrap page tables map the low 4 GiB identity-wise.  RAM is mapped
 * write-back and only the top 1 GiB (3-4 GiB) is uncached, which is not where
 * every firmware places PCI MMIO BARs.  Device registers read through a
 * write-back mapping return stale values, so drivers must explicitly make
 * their BAR uncached before touching it.
 *
 * This function walks the current page tables (via CR3), sets PCD+PWT on the
 * PTE/PDE covering the range, and flushes the TLB.  It is safe to call for a
 * range that is already uncached.
 */
void mmu_mark_mmio_uncached(uint64_t phys, uint64_t size);

#endif
