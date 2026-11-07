#ifndef NW_WORD_MODULE_H
#define NW_WORD_MODULE_H

#include <stdint.h>

int nw_word_module_init_from_multiboot2(uint32_t mbi_phys);
int nw_word_module_is_ready(void);
uint32_t nw_word_module_count(void);
uint32_t nw_word_module_size(void);
const char *nw_word_module_get(uint32_t word_index);
const char *nw_word_module_error(void);

#endif
