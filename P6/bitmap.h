#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "stdlib.h"

/* Bitmap for free inodes and blocks */
typedef unsigned char* bitmap_t;

extern void bitmap_zeroall(bitmap_t bitmap, size_t bit_size);

extern int bitmap_count_zero(bitmap_t bitmap, size_t bit_size);

extern int bitmap_next_zero(bitmap_t bitmap, size_t bit_size);

extern void bitmap_clear(bitmap_t bitmap, size_t bit);

extern void bitmap_set(bitmap_t bitmap, size_t bit);

extern char bitmap_get(bitmap_t bitmap, size_t bit);

#endif /* __BITMAP_H__ */
