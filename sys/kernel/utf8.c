#include "utf8.h"

static int is_continuation(uint8_t b)
{
    return (b & 0xC0u) == 0x80u;
}

int utf8_next(const char **cursor, uint32_t *codepoint)
{
    if (cursor == 0 || *cursor == 0 || codepoint == 0)
        return 0;

    const uint8_t *s = (const uint8_t *)*cursor;
    uint8_t b0 = s[0];

    if (b0 == 0)
        return 0;

    if (b0 < 0x80u) {
        *codepoint = b0;
        *cursor = (const char *)(s + 1);
        return 1;
    }

    uint32_t cp;
    unsigned int len;

    if (b0 >= 0xC2u && b0 <= 0xDFu) {
        cp = b0 & 0x1Fu;
        len = 2;
    } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
        cp = b0 & 0x0Fu;
        len = 3;
    } else if (b0 >= 0xF0u && b0 <= 0xF4u) {
        cp = b0 & 0x07u;
        len = 4;
    } else {
        goto invalid;
    }

    /* Every following byte must be a continuation byte. */
    for (unsigned int i = 1; i < len; i++) {
        if (s[i] == 0 || !is_continuation(s[i]))
            goto invalid;
    }

    /* Reject overlong encodings and invalid Unicode ranges. */
    if (len == 3) {
        if (b0 == 0xE0u && s[1] < 0xA0u)
            goto invalid;
        if (b0 == 0xEDu && s[1] >= 0xA0u)
            goto invalid;
    }

    if (len == 4) {
        if (b0 == 0xF0u && s[1] < 0x90u)
            goto invalid;
        if (b0 == 0xF4u && s[1] > 0x8Fu)
            goto invalid;
    }

    for (unsigned int i = 1; i < len; i++)
        cp = (cp << 6) | (s[i] & 0x3Fu);

    *codepoint = cp;
    *cursor = (const char *)(s + len);
    return 1;

invalid:
    *codepoint = 0xFFFDu; /* Unicode replacement character */
    *cursor = (const char *)(s + 1);
    return 1;
}
