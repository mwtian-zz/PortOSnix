#include "bitmap.h"

/* Clear all bits in bitmap with 'num_bits' number of bits */
void
bitmap_zeroall(bitmap_t bitmap, size_t bit_size)
{
    size_t i = 0;
    size_t byte_size = ((bit_size - 1) >> 3) + 1;
    for (i = 0; i < byte_size; ++i) {
        bitmap[i] = 0;
    }
}

/* Set 'bit' in bitmap to 1. Index starts at 0. */
void
bitmap_set(bitmap_t bitmap, size_t bit)
{
    size_t i = bit >> 3;
    bit &= 7;
    bitmap[i] |= (1 << bit);
}

/* Set 'bit' in bitmap to 0. Index starts at 0. */
void
bitmap_clear(bitmap_t bitmap, size_t bit)
{
    size_t i = bit >> 3;
    bit &= 7;
    bitmap[i] &= ~(1 << bit);
}

/* Get 'bit' in bitmap. Index starts at 0. */
char
bitmap_get(bitmap_t bitmap, size_t bit)
{
    size_t i = bit >> 3;
    bit &= 7;
    return ((bitmap[i] >> bit) & 1);
}

/* Find next zero bit */
int
bitmap_next_zero(bitmap_t bitmap, size_t bit_size)
{
    int i = 0;
    int j = 0;
    int check_bit = 0;

    while (bit_size > 0) {
        /* Check if the byte contains zero */
        if (bit_size > 8) {
            check_bit = 8;
        } else {
            check_bit = bit_size;
        }
        bit_size -= check_bit;
        /* Count bits in char that are zero */
        for (j = 0; j < check_bit; ++j) {
            if (((~bitmap[i]) & (1 << j)) != 0)
                return (i * 8 + j);
        }
        ++i;
    }

    return -1;
}

/* Count number of zero bits */
int
bitmap_count_zero(bitmap_t bitmap, size_t bit_size)
{
    int i = 0;
    int j = 0;
    int check_bit = 0;
    int count = 0;

    while (bit_size > 0) {
        /* Number of bits in char to check */
        if (bit_size > 8) {
            check_bit = 8;
        } else {
            check_bit = bit_size;
        }
        bit_size -= check_bit;
        /* Count bits in char that are zero */
        for (j = 0; j < check_bit; ++j) {
            if (((~bitmap[i]) & (1 << j)) != 0)
                count++;
        }
        ++i;
    }
    return count;
}

