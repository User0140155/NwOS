#include "mem.h"

extern unsigned long pmm_total_pages(void) __attribute__((weak));
extern unsigned long pmm_used_pages(void)  __attribute__((weak));

#define NWOS_PAGE_KB 4UL

unsigned long nwf_ram_total_kb(void)
{
    return (unsigned long)nwf_ram_mb() * 1024UL;
}

unsigned long nwf_ram_used_kb(void)
{
    if (pmm_used_pages && pmm_total_pages)
        return (unsigned long)pmm_used_pages() * NWOS_PAGE_KB;

    return kernel_static_memory_kb();
}

unsigned long nwf_ram_free_kb(void)
{
    unsigned long total = nwf_ram_total_kb();
    unsigned long used  = nwf_ram_used_kb();
    return total > used ? total - used : 0;
}

int nwf_ram_used_percent(void)
{
    unsigned long total = nwf_ram_total_kb();
    if (total == 0) return 0;
    return (int)((nwf_ram_used_kb() * 100ULL) / total);
}