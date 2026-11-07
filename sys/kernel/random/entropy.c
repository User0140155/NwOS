#include "random/entropy.h"

#include <stddef.h>
#include <stdint.h>

#define NW_GOD_BAD_BITS       4u
#define NW_GOD_GOOD_BITS      24u
#define NW_ENTROPY_FIFO_BITS  16384u
#define NW_ENTROPY_FIFO_BYTES (NW_ENTROPY_FIFO_BITS / 8u)
#define NW_ENTROPY_FIFO_MASK  (NW_ENTROPY_FIFO_BITS - 1u)

#if (NW_ENTROPY_FIFO_BITS & (NW_ENTROPY_FIFO_BITS - 1u)) != 0
#error NW_ENTROPY_FIFO_BITS_must_be_a_power_of_two
#endif

static uint8_t entropy_fifo[NW_ENTROPY_FIFO_BYTES];
static uint32_t entropy_head;
static uint32_t entropy_tail;
static uint32_t entropy_count;

static uint64_t entropy_irq_save(void)
{
#ifdef NW_ENTROPY_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void entropy_irq_restore(uint64_t flags)
{
#ifndef NW_ENTROPY_TEST
    if (flags & (1ull << 9))
        __asm__ volatile ("sti" ::: "memory");
#else
    (void)flags;
#endif
}

static void entropy_push_bit_unlocked(uint32_t bit)
{
    uint32_t byte_index;
    uint8_t bit_mask;

    if (entropy_count == NW_ENTROPY_FIFO_BITS) {
        entropy_tail = (entropy_tail + 1u) & NW_ENTROPY_FIFO_MASK;
        --entropy_count;
    }

    byte_index = entropy_head >> 3;
    bit_mask = (uint8_t)(1u << (entropy_head & 7u));
    if (bit)
        entropy_fifo[byte_index] |= bit_mask;
    else
        entropy_fifo[byte_index] &= (uint8_t)~bit_mask;

    entropy_head = (entropy_head + 1u) & NW_ENTROPY_FIFO_MASK;
    ++entropy_count;
}

void nw_entropy_init(void)
{
    uint64_t flags = entropy_irq_save();
    uint32_t i;

    for (i = 0; i < NW_ENTROPY_FIFO_BYTES; ++i)
        entropy_fifo[i] = 0;

    entropy_head = 0;
    entropy_tail = 0;
    entropy_count = 0;

    entropy_irq_restore(flags);
}

void nw_entropy_add_keyboard_event(uint64_t tsc_timestamp, int key)
{
    uint64_t flags;
    uint32_t sample;
    uint32_t i;

    if (tsc_timestamp == 0u || key == 0)
        return;

    sample = (uint32_t)(tsc_timestamp >> NW_GOD_BAD_BITS);

    flags = entropy_irq_save();
    for (i = 0; i < NW_GOD_GOOD_BITS; ++i)
        entropy_push_bit_unlocked((sample >> i) & 1u);
    entropy_irq_restore(flags);
}

uint32_t nw_entropy_available_bits(void)
{
    uint64_t flags = entropy_irq_save();
    uint32_t result = entropy_count;
    entropy_irq_restore(flags);
    return result;
}

int nw_entropy_get_bits(uint32_t bit_count, uint32_t *value_out)
{
    uint64_t flags;
    uint32_t value = 0;
    uint32_t i;

    if (value_out == 0 || bit_count == 0u || bit_count > 32u)
        return 0;

    flags = entropy_irq_save();
    if (entropy_count < bit_count) {
        entropy_irq_restore(flags);
        return 0;
    }

    for (i = 0; i < bit_count; ++i) {
        uint32_t byte_index = entropy_tail >> 3;
        uint32_t bit_index = entropy_tail & 7u;
        uint32_t bit = (entropy_fifo[byte_index] >> bit_index) & 1u;

        value = (value << 1) | bit;
        entropy_tail = (entropy_tail + 1u) & NW_ENTROPY_FIFO_MASK;
        --entropy_count;
    }

    entropy_irq_restore(flags);
    *value_out = value;
    return 1;
}

static uint32_t bits_required(uint32_t maximum_value)
{
    uint32_t bits = 0;
    while (maximum_value != 0u) {
        ++bits;
        maximum_value >>= 1;
    }
    return bits;
}

int nw_entropy_random_bounded(uint32_t upper_bound, uint32_t *value_out)
{
    uint32_t bits;
    uint32_t mask;
    uint32_t attempt;

    if (upper_bound == 0u || value_out == 0)
        return 0;
    if (upper_bound == 1u) {
        *value_out = 0;
        return 1;
    }

    bits = bits_required(upper_bound - 1u);
    mask = (bits == 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);

    for (attempt = 0; attempt < 32u; ++attempt) {
        uint32_t candidate;
        if (!nw_entropy_get_bits(bits, &candidate))
            return 0;
        candidate &= mask;
        if (candidate < upper_bound) {
            *value_out = candidate;
            return 1;
        }
    }

    return 0;
}
