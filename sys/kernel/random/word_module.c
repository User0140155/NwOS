#include "word_module.h"

#include <stddef.h>
#include <stdint.h>

#define MB2_INFO_MAX_SIZE       (16u * 1024u * 1024u)
#define MB2_TAG_END             0u
#define MB2_TAG_MODULE          3u
#define MB2_TAG_HEADER_SIZE     8u
#define MB2_MODULE_MIN_SIZE    16u
#define NWWD_HEADER_SIZE       28u
#define NWWD_VERSION            1u
#define NWWD_MAX_WORD_BYTES   255u
#define NWWD_MAX_MODULE_BYTES (64u * 1024u * 1024u)
#define NWWD_MODULE_COMMAND    "nwos-word-dictionary"

static const uint8_t *g_index;
static const uint8_t *g_data;
static uint32_t g_blob_size;
static uint32_t g_word_count;
static uint32_t g_index_stride;
static uint32_t g_index_count;
static uint32_t g_data_size;
static int g_ready;
static const char *g_error = "not initialized";

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t crc32_bytes(const uint8_t *data, uint32_t size)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;

    for (i = 0; i < size; ++i) {
        unsigned int bit;
        crc ^= data[i];
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xFFFFFFFFu;
}

static int valid_utf8_word(const uint8_t *s, uint32_t length)
{
    uint32_t i = 0;

    if (length == 0u || length > NWWD_MAX_WORD_BYTES)
        return 0;

    while (i < length) {
        uint8_t a = s[i++];
        if (a <= 0x7Fu)
            continue;

        if (a >= 0xC2u && a <= 0xDFu) {
            if (i >= length || (s[i] & 0xC0u) != 0x80u)
                return 0;
            ++i;
            continue;
        }

        if (a >= 0xE0u && a <= 0xEFu) {
            uint8_t b, c;
            if (i + 1u >= length)
                return 0;
            b = s[i];
            c = s[i + 1u];
            if ((b & 0xC0u) != 0x80u || (c & 0xC0u) != 0x80u)
                return 0;
            if ((a == 0xE0u && b < 0xA0u) ||
                (a == 0xEDu && b >= 0xA0u))
                return 0;
            i += 2u;
            continue;
        }

        if (a >= 0xF0u && a <= 0xF4u) {
            uint8_t b, c, d;
            if (i + 2u >= length)
                return 0;
            b = s[i];
            c = s[i + 1u];
            d = s[i + 2u];
            if ((b & 0xC0u) != 0x80u ||
                (c & 0xC0u) != 0x80u ||
                (d & 0xC0u) != 0x80u)
                return 0;
            if ((a == 0xF0u && b < 0x90u) ||
                (a == 0xF4u && b > 0x8Fu))
                return 0;
            i += 3u;
            continue;
        }

        return 0;
    }

    return 1;
}

static int validate_nwwd(const uint8_t *blob, uint32_t blob_size)
{
    uint32_t word_count;
    uint32_t stride;
    uint32_t index_count;
    uint32_t data_size;
    uint32_t expected_crc;
    uint64_t index_bytes;
    uint64_t data_offset;
    uint64_t expected_size;
    uint32_t data_pos = 0;
    uint32_t index_pos = 0;
    uint32_t word_index;

    if (blob == 0 || blob_size < NWWD_HEADER_SIZE) {
        g_error = "dictionary module is smaller than its header";
        return 0;
    }
    if (blob[0] != 'N' || blob[1] != 'W' ||
        blob[2] != 'W' || blob[3] != 'D') {
        g_error = "dictionary magic is not NWWD";
        return 0;
    }
    if (read_u16_le(blob + 4u) != NWWD_VERSION ||
        read_u16_le(blob + 6u) != NWWD_HEADER_SIZE) {
        g_error = "unsupported NWWD version or header size";
        return 0;
    }

    word_count = read_u32_le(blob + 8u);
    stride = read_u32_le(blob + 12u);
    index_count = read_u32_le(blob + 16u);
    data_size = read_u32_le(blob + 20u);
    expected_crc = read_u32_le(blob + 24u);

    if (word_count == 0u || stride == 0u || data_size == 0u ||
        word_count > data_size / 2u) {
        g_error = "invalid NWWD counts or empty dictionary";
        return 0;
    }

    if (((uint64_t)word_count + stride - 1u) / stride != index_count) {
        g_error = "NWWD sparse-index count is inconsistent";
        return 0;
    }

    index_bytes = (uint64_t)index_count * 4u;
    data_offset = (uint64_t)NWWD_HEADER_SIZE + index_bytes;
    expected_size = data_offset + data_size;
    if (expected_size != blob_size || data_offset > blob_size) {
        g_error = "NWWD section sizes do not match module size";
        return 0;
    }

    g_index = blob + NWWD_HEADER_SIZE;
    g_data = blob + (uint32_t)data_offset;
    if (g_data[data_size - 1u] != 0u) {
        g_error = "NWWD data section is not NUL terminated";
        return 0;
    }
    if (crc32_bytes(g_data, data_size) != expected_crc) {
        g_error = "NWWD data CRC32 mismatch";
        return 0;
    }

    for (word_index = 0; word_index < word_count; ++word_index) {
        uint32_t start = data_pos;
        uint32_t length;

        if (word_index % stride == 0u) {
            uint32_t indexed_offset = read_u32_le(g_index + index_pos * 4u);
            if (indexed_offset != data_pos) {
                g_error = "NWWD sparse index points to the wrong word";
                return 0;
            }
            ++index_pos;
        }

        while (data_pos < data_size && g_data[data_pos] != 0u &&
               data_pos - start <= NWWD_MAX_WORD_BYTES)
            ++data_pos;

        if (data_pos >= data_size || data_pos - start == 0u ||
            data_pos - start > NWWD_MAX_WORD_BYTES) {
            g_error = "NWWD contains an empty, unterminated or oversized word";
            return 0;
        }

        length = data_pos - start;
        if (!valid_utf8_word(g_data + start, length)) {
            g_error = "NWWD contains an invalid UTF-8 word";
            return 0;
        }
        ++data_pos; /* skip NUL */
    }

    if (data_pos != data_size || index_pos != index_count) {
        g_error = "NWWD word count does not match its data section";
        return 0;
    }

    g_blob_size = blob_size;
    g_word_count = word_count;
    g_index_stride = stride;
    g_index_count = index_count;
    g_data_size = data_size;
    g_ready = 1;
    g_error = "ok";
    return 1;
}

static int module_string_matches(const uint8_t *text, uint32_t capacity)
{
    static const char expected[] = NWWD_MODULE_COMMAND;
    uint32_t i;

    for (i = 0; i < (uint32_t)(sizeof(expected) - 1u); ++i) {
        if (i >= capacity || text[i] != (uint8_t)expected[i])
            return 0;
    }
    if (i >= capacity)
        return 0;
    return text[i] == 0u;
}

int nw_word_module_init_from_multiboot2(uint32_t mbi_phys)
{
    const uint8_t *mbi;
    uint32_t total_size;
    uint32_t offset;
    int found_end = 0;

    g_index = 0;
    g_data = 0;
    g_blob_size = 0;
    g_word_count = 0;
    g_index_stride = 0;
    g_index_count = 0;
    g_data_size = 0;
    g_ready = 0;
    g_error = "Multiboot2 dictionary module not found";

    if (mbi_phys == 0u || (mbi_phys & 7u) != 0u ||
        (uint64_t)mbi_phys + 8u > 0x100000000ull) {
        g_error = "invalid Multiboot2 info address";
        return 0;
    }

    mbi = (const uint8_t *)(uintptr_t)mbi_phys;
    total_size = read_u32_le(mbi);
    if (total_size < 16u || total_size > MB2_INFO_MAX_SIZE ||
        (uint64_t)mbi_phys + total_size > 0x100000000ull) {
        g_error = "invalid Multiboot2 info size";
        return 0;
    }

    offset = 8u;
    while (offset <= total_size - MB2_TAG_HEADER_SIZE) {
        const uint8_t *tag = mbi + offset;
        uint32_t type = read_u32_le(tag);
        uint32_t size = read_u32_le(tag + 4u);
        uint32_t aligned_size;

        if (size < MB2_TAG_HEADER_SIZE || size > total_size - offset) {
            g_error = "malformed Multiboot2 tag";
            return 0;
        }
        if (type == MB2_TAG_END) {
            if (size != MB2_TAG_HEADER_SIZE) {
                g_error = "malformed Multiboot2 end tag";
                return 0;
            }
            found_end = 1;
            break;
        }

        if (type == MB2_TAG_MODULE && size >= MB2_MODULE_MIN_SIZE + 1u) {
            uint32_t module_start = read_u32_le(tag + 8u);
            uint32_t module_end = read_u32_le(tag + 12u);
            const uint8_t *cmdline = tag + MB2_MODULE_MIN_SIZE;
            uint32_t cmdline_capacity = size - MB2_MODULE_MIN_SIZE;

            if (module_string_matches(cmdline, cmdline_capacity)) {
                uint32_t module_size;
                if (module_start == 0u || module_end <= module_start) {
                    g_error = "NwWord module has invalid physical bounds";
                    return 0;
                }
                module_size = module_end - module_start;
                if (module_size > NWWD_MAX_MODULE_BYTES) {
                    g_error = "NwWord module exceeds the 64 MiB safety limit";
                    return 0;
                }
                return validate_nwwd(
                    (const uint8_t *)(uintptr_t)module_start,
                    module_size
                );
            }
        }

        aligned_size = (size + 7u) & ~7u;
        if (aligned_size < size || aligned_size > total_size - offset) {
            g_error = "Multiboot2 tag alignment exceeds info structure";
            return 0;
        }
        offset += aligned_size;
    }

    if (!found_end)
        g_error = "Multiboot2 end tag not found";
    return 0;
}

int nw_word_module_is_ready(void)
{
    return g_ready;
}

uint32_t nw_word_module_count(void)
{
    return g_ready ? g_word_count : 0u;
}

uint32_t nw_word_module_size(void)
{
    return g_ready ? g_blob_size : 0u;
}

const char *nw_word_module_get(uint32_t word_index)
{
    uint32_t checkpoint;
    uint32_t offset;
    const uint8_t *word;

    if (!g_ready || word_index >= g_word_count)
        return 0;

    checkpoint = word_index / g_index_stride;
    if (checkpoint >= g_index_count)
        return 0;
    offset = read_u32_le(g_index + checkpoint * 4u);
    word = g_data + offset;

    while (word_index % g_index_stride != 0u) {
        while (*word != 0u)
            ++word;
        ++word;
        --word_index;
    }
    return (const char *)word;
}

const char *nw_word_module_error(void)
{
    return g_error;
}
