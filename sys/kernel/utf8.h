#ifndef NWOS_UTF8_H
#define NWOS_UTF8_H

#include <stdint.h>

int utf8_next(const char **cursor, uint32_t *codepoint);

#endif
