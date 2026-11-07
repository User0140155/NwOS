#ifndef NW_PASSAGE_MODULE_H
#define NW_PASSAGE_MODULE_H

#include <stdint.h>

int nw_passage_module_init_from_multiboot2(uint32_t mbi_phys);
int nw_passage_module_is_ready(void);
uint32_t nw_passage_module_count(void);
int nw_passage_module_get(uint32_t verse_index,
                          const char **line_out,
                          uint32_t *length_out);
const char *nw_passage_module_error(void);

#endif
