#ifndef CHAR_SYMBOL_H
#define CHAR_SYMBOL_H

#include <stdint.h>

#define CHAR_COPYRIGHT   ((char)0x80)
#define STR_COPYRIGHT    "\x80"

#define CHAR_COPYRIGHT_BYTE 0x80u
//#define CHAR_COPYRIGHT      ((char)CHAR_COPYRIGHT_BYTE)
//#define STR_COPYRIGHT       "\x80"

static const uint8_t char_symbol_copyright[8] = {
    0x3C,
    0x42,
    0x9D,
    0x85,
    0x85,
    0x9D,
    0x42,
    0x3C
};

static inline const uint8_t *char_symbol_glyph(uint8_t c)
{
    if (c == CHAR_COPYRIGHT_BYTE)
        return char_symbol_copyright;

    return 0;
}

#endif
