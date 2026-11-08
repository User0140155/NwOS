#define NWOS_INCLUDE_WORDS_DATA_H
#include "nwos_internal.h"

static void nw_god_pick(void);
static int nw_god_bits(uint32_t bit_count, uint32_t *value_out);
static uint32_t nw_god_bits_required(uint32_t value);

unsigned int rand_simple(void)
{
    random_seed =
        random_seed * 1103515245 + 12345;

    return random_seed;
}

static void nw_god_pick(void)
{
    print("\n[NwWord] Entropy FIFO is empty. Press a key to sample the timer... ");
    (void)keyboard_getkey();
    print("\n");
}

static int nw_god_bits(uint32_t bit_count, uint32_t *value_out)
{
    uint32_t value = 0;
    uint32_t i;

    if (value_out == 0 || bit_count == 0u || bit_count > 32u)
        return 0;

    for (i = 0; i < bit_count; ++i) {
        uint32_t bit = 0;

        while (!nw_entropy_get_bits(1u, &bit))
            nw_god_pick();

        value = (value << 1) | (bit & 1u);
    }

    *value_out = value;
    return 1;
}

static uint32_t nw_god_bits_required(uint32_t value)
{
    uint32_t bits = 0;

    while (value != 0u) {
        ++bits;
        value >>= 1;
    }

    return bits;
}

uint32_t nw_god_random_bounded(uint32_t upper_bound)
{
    uint32_t bit_count;
    uint32_t mask;

    if (upper_bound <= 1u)
        return 0u;

    bit_count = nw_god_bits_required(upper_bound - 1u);
    mask = (bit_count >= 32u) ? 0xFFFFFFFFu : ((1u << bit_count) - 1u);

    for (;;) {
        uint32_t candidate = 0;

        (void)nw_god_bits(bit_count, &candidate);
        candidate &= mask;
        if (candidate < upper_bound)
            return candidate;
    }
}

const char* nwf_random_word(void)
{
    uint32_t bits;
    uint32_t count;
    uint32_t index;

    if (!nw_god_bits(17u, &bits))
        return "";

    if (nw_word_module_is_ready()) {
        count = nw_word_module_count();
        if (count != 0u) {
            index = bits % count;
            {
                const char *word = nw_word_module_get(index);
                if (word != 0 && word[0] != '\0')
                    return word;
            }
        }
    }

    count = (NWOS_WORD_COUNT > 0) ? (uint32_t)NWOS_WORD_COUNT : 0u;
    if (count == 0u)
        return "";

    index = bits % count;
    return nw_words[index];
}

void random_init(void)
{
    unsigned int seed;

    __asm__ volatile (
        "rdtsc"
        : "=a"(seed)
        :
        : "edx"
    );

    random_seed ^= seed;
}
