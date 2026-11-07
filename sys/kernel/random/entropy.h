#ifndef NW_ENTROPY_H
#define NW_ENTROPY_H

#include <stdint.h>

void nw_entropy_init(void);
void nw_entropy_add_keyboard_event(uint64_t tsc_timestamp, int key);
uint32_t nw_entropy_available_bits(void);
int nw_entropy_get_bits(uint32_t bit_count, uint32_t *value_out);
int nw_entropy_random_bounded(uint32_t upper_bound, uint32_t *value_out);

#endif
