#ifndef NWOS_MEM_H
#define NWOS_MEM_H

unsigned int  nwf_ram_mb(void);

unsigned long nwf_ram_total_kb(void);
unsigned long nwf_ram_used_kb(void);
unsigned long nwf_ram_free_kb(void);
int           nwf_ram_used_percent(void);

unsigned long kernel_static_memory_kb(void);

#endif /* NWOS_MEM_H */