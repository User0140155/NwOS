#include "random/passage_module.h"

#include <stddef.h>
#include <stdint.h>

#define MB2_INFO_MAX_SIZE          (16u * 1024u * 1024u)
#define MB2_TAG_END                0u
#define MB2_TAG_MODULE             3u
#define MB2_TAG_HEADER_SIZE        8u
#define MB2_MODULE_HEADER_SIZE     16u
#define NW_PASSAGE_MAX_BYTES       (8u * 1024u * 1024u)
#define NW_PASSAGE_INDEX_STRIDE    128u
#define NW_PASSAGE_INDEX_CAPACITY  512u
#define NW_PASSAGE_MODULE_COMMAND  "nwos-bible-passages"

static const uint8_t *g_data;
static uint32_t g_size;
static uint32_t g_verse_count;
static uint32_t g_index_count;
static uint32_t g_index[NW_PASSAGE_INDEX_CAPACITY];
static int g_ready;
static const char *g_error = "not initialized";

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int valid_utf8(const uint8_t *text, uint32_t length)
{
    uint32_t i = 0;

    while (i < length) {
        uint8_t a = text[i++];

        if (a <= 0x7Fu)
            continue;

        if (a >= 0xC2u && a <= 0xDFu) {
            if (i >= length || (text[i] & 0xC0u) != 0x80u)
                return 0;
            ++i;
            continue;
        }

        if (a >= 0xE0u && a <= 0xEFu) {
            uint8_t b, c;
            if (i + 1u >= length)
                return 0;
            b = text[i];
            c = text[i + 1u];
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
            b = text[i];
            c = text[i + 1u];
            d = text[i + 2u];
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

static int is_ascii_letter_or_digit(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
}
static int is_verse_line(const uint8_t *line, uint32_t length)
{
    uint32_t end = length;
    uint32_t colon = 0xFFFFFFFFu;
    uint32_t book_sep = 0xFFFFFFFFu;
    uint32_t text_start;
    uint32_t i;
    int has_book_letter = 0;

    if (line == 0)
        return 0;

    if (end >= 3u && line[0] == 0xEFu &&
        line[1] == 0xBBu && line[2] == 0xBFu) {
        line += 3u;
        end -= 3u;
    }

    while (end > 0u && (line[end - 1u] == '\r' ||
                         line[end - 1u] == ' ' ||
                         line[end - 1u] == '\t'))
        --end;

    if (end < 8u)
        return 0;

    for (i = 0; i < end; ++i) {
        uint8_t c = line[i];
        if (c == ':') {
            colon = i;
            break;
        }
        if (c == ' ')
            book_sep = i;
        else if (is_ascii_letter_or_digit(c) || c == '-' || c == '\'') {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                has_book_letter = 1;
        } else {
            return 0;
        }
    }

    if (colon == 0xFFFFFFFFu || book_sep == 0xFFFFFFFFu ||
        !has_book_letter || book_sep == 0u || book_sep + 1u >= colon)
        return 0;

    for (i = book_sep + 1u; i < colon; ++i)
        if (line[i] < '0' || line[i] > '9')
            return 0;

    text_start = colon + 1u;
    if (text_start >= end || line[text_start] < '0' || line[text_start] > '9')
        return 0;

    while (text_start < end && line[text_start] >= '0' && line[text_start] <= '9')
        ++text_start;

    if (text_start >= end || line[text_start] != ' ')
        return 0;

    while (text_start < end && line[text_start] == ' ')
        ++text_start;

    if (text_start >= end)
        return 0;

    return valid_utf8(line, end);
}

static uint32_t line_end_offset(const uint8_t *data, uint32_t size,
                                uint32_t start)
{
    uint32_t end = start;
    while (end < size && data[end] != '\n')
        ++end;
    return end;
}

static int module_string_matches(const uint8_t *text, uint32_t capacity)
{
    static const char expected[] = NW_PASSAGE_MODULE_COMMAND;
    uint32_t i;

    for (i = 0; i < (uint32_t)(sizeof(expected) - 1u); ++i) {
        if (i >= capacity || text[i] != (uint8_t)expected[i])
            return 0;
    }

    return i < capacity && text[i] == 0u;
}

static int build_verse_index(const uint8_t *data, uint32_t size)
{
    uint32_t offset = 0;
    uint32_t count = 0;
    uint32_t index_count = 0;

    while (offset < size) {
        uint32_t end = line_end_offset(data, size, offset);
        uint32_t length = end - offset;

        if (length > 2048u) {
            g_error = "Bible text contains a line longer than 2048 bytes";
            return 0;
        }

        if (is_verse_line(data + offset, length)) {
            if ((count % NW_PASSAGE_INDEX_STRIDE) == 0u) {
                if (index_count >= NW_PASSAGE_INDEX_CAPACITY) {
                    g_error = "Bible text has too many verses for the sparse index";
                    return 0;
                }
                g_index[index_count++] = offset;
            }
            ++count;
        }

        offset = (end < size) ? end + 1u : size;
    }

    if (count < 8u) {
        g_error = "Bible text has fewer than eight usable verse lines";
        return 0;
    }

    g_data = data;
    g_size = size;
    g_verse_count = count;
    g_index_count = index_count;
    g_ready = 1;
    g_error = "ok";
    return 1;
}

int nw_passage_module_init_from_multiboot2(uint32_t mbi_phys)
{
    const uint8_t *mbi;
    uint32_t total_size;
    uint32_t offset;
    int found_end = 0;

    g_data = 0;
    g_size = 0;
    g_verse_count = 0;
    g_index_count = 0;
    g_ready = 0;
    g_error = "NwOS Bible passage module not found";

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
        uint32_t tag_size = read_u32_le(tag + 4u);
        uint32_t aligned_size;

        if (tag_size < MB2_TAG_HEADER_SIZE || tag_size > total_size - offset) {
            g_error = "malformed Multiboot2 tag";
            return 0;
        }

        if (type == MB2_TAG_END) {
            if (tag_size != MB2_TAG_HEADER_SIZE) {
                g_error = "malformed Multiboot2 end tag";
                return 0;
            }
            found_end = 1;
            break;
        }

        if (type == MB2_TAG_MODULE && tag_size >= MB2_MODULE_HEADER_SIZE + 1u) {
            uint32_t module_start = read_u32_le(tag + 8u);
            uint32_t module_end = read_u32_le(tag + 12u);
            const uint8_t *cmdline = tag + MB2_MODULE_HEADER_SIZE;
            uint32_t cmdline_capacity = tag_size - MB2_MODULE_HEADER_SIZE;

            if (module_string_matches(cmdline, cmdline_capacity)) {
                uint32_t module_size;

                if (module_start == 0u || module_end <= module_start) {
                    g_error = "Bible module has invalid physical bounds";
                    return 0;
                }

                module_size = module_end - module_start;
                if (module_size > NW_PASSAGE_MAX_BYTES) {
                    g_error = "Bible module exceeds the 8 MiB safety limit";
                    return 0;
                }

                return build_verse_index(
                    (const uint8_t *)(uintptr_t)module_start,
                    module_size
                );
            }
        }

        aligned_size = (tag_size + 7u) & ~7u;
        if (aligned_size < tag_size || aligned_size > total_size - offset) {
            g_error = "Multiboot2 tag alignment exceeds info structure";
            return 0;
        }
        offset += aligned_size;
    }

    if (!found_end)
        g_error = "Multiboot2 end tag not found";
    return 0;
}

int nw_passage_module_is_ready(void)
{
    return g_ready;
}

uint32_t nw_passage_module_count(void)
{
    return g_ready ? g_verse_count : 0u;
}

int nw_passage_module_get(uint32_t verse_index,
                          const char **line_out,
                          uint32_t *length_out)
{
    uint32_t checkpoint;
    uint32_t current_index;
    uint32_t offset;

    if (!g_ready || line_out == 0 || length_out == 0 ||
        verse_index >= g_verse_count)
        return 0;

    checkpoint = verse_index / NW_PASSAGE_INDEX_STRIDE;
    if (checkpoint >= g_index_count)
        return 0;

    offset = g_index[checkpoint];
    current_index = checkpoint * NW_PASSAGE_INDEX_STRIDE;

    while (offset < g_size) {
        uint32_t end = line_end_offset(g_data, g_size, offset);
        uint32_t length = end - offset;

        if (is_verse_line(g_data + offset, length)) {
            if (current_index == verse_index) {
                while (length > 0u && g_data[offset + length - 1u] == '\r')
                    --length;
                *line_out = (const char *)(g_data + offset);
                *length_out = length;
                return 1;
            }
            ++current_index;
        }

        offset = (end < g_size) ? end + 1u : g_size;
    }

    return 0;
}

const char *nw_passage_module_error(void)
{
    return g_error;
}
