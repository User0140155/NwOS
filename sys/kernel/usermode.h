#ifndef NWOS_USERMODE_H
#define NWOS_USERMODE_H

#include <stdint.h>

int usermode_enter_shell(void);
int usermode_run_native(const uint8_t *image, uint32_t image_size,
                        uint32_t bss_size, uint32_t entry,
                        const uint32_t *relocs, uint32_t reloc_count,
                        uint64_t *exit_code);

#endif
