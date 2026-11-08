#include "mem.h"
#include "pmm.h"

unsigned long nwf_ram_total_kb(void)
{
    if (pmm_is_ready())
        return (unsigned long)(pmm_total_bytes() / 1024ULL);

    return (unsigned long)nwf_ram_mb() * 1024UL;
}

unsigned long nwf_ram_used_kb(void)
{
    if (pmm_is_ready())
        return (unsigned long)(pmm_used_bytes() / 1024ULL);

    return kernel_static_memory_kb();
}

unsigned long nwf_ram_free_kb(void)
{
    if (pmm_is_ready())
        return (unsigned long)(pmm_total_free_bytes() / 1024ULL);

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