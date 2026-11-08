#ifndef NWOS_PMM_H
#define NWOS_PMM_H

#include <stdint.h>
#include <stddef.h>

void *pmm_alloc_dma32(size_t size, size_t alignment, uint64_t *phys_out);
void pmm_free_pages(uintptr_t virt, size_t count);
int pmm_is_ready(void);
int pmm_init(void);
uint64_t pmm_total_bytes(void);
uint64_t pmm_total_free_bytes(void);
uint64_t pmm_used_bytes(void);
uint64_t pmm_free_page_count(void);

#endif

