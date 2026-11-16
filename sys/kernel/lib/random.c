#include "nwos_internal.h"

unsigned int rand_simple(void)
{
    random_seed = random_seed * 1103515245 + 12345;
    return random_seed;
}

static void nw_random_pick_entropy(void)
{
    print("\n[NwRandom] Entropy FIFO is empty. Press a key to sample the timer... ");
    (void)keyboard_getkey();
    print("\n");
}

static int nw_random_bits(uint32_t bit_count, uint32_t *value_out)
{
    uint32_t value = 0;

    if (value_out == 0 || bit_count == 0u || bit_count > 32u)
        return 0;

    for (uint32_t i = 0; i < bit_count; ++i)
    {
        uint32_t bit = 0;
        while (!nw_entropy_get_bits(1u, &bit))
            nw_random_pick_entropy();
        value = (value << 1) | (bit & 1u);
    }

    *value_out = value;
    return 1;
}

static uint32_t nw_random_bits_required(uint32_t value)
{
    uint32_t bits = 0;
    while (value != 0u)
    {
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

    bit_count = nw_random_bits_required(upper_bound - 1u);
    mask = (bit_count >= 32u) ? 0xFFFFFFFFu : ((1u << bit_count) - 1u);

    for (;;)
    {
        uint32_t candidate = 0;
        (void)nw_random_bits(bit_count, &candidate);
        candidate &= mask;
        if (candidate < upper_bound)
            return candidate;
    }
}

void random_init(void)
{
    unsigned int seed;
    __asm__ volatile ("rdtsc" : "=a"(seed) : : "edx");
    random_seed ^= seed;
}
