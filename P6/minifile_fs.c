#include <string.h>

#include "minifile_fs.h"
#include "minifile_cache.h"
#include "minithread.h"
#include "minithread_private.h"
#include "minifile_inode.h"
#include "synch.h"

void
bitmap_zeroall(bitmap_t bitmap, size_t bit_size);
int
bitmap_count_zero(bitmap_t bitmap, size_t bit_size);
int
bitmap_next_zero(bitmap_t bitmap, size_t bit_size);
void
bitmap_clear(bitmap_t bitmap, size_t bit);
void
bitmap_set(bitmap_t bitmap, size_t bit);
void
fs_lock(mem_sblock_t sbp);
void
fs_unlock(mem_sblock_t sbp);

/* Get super block into a memory struct */
int
sblock_get(disk_t* disk, mem_sblock_t sbp)
{
    buf_block_t buf;

    if (bread(disk, 0, &buf) != 0)
        return -1;
    memcpy(sbp, buf->data, sizeof(struct sblock));
    sbp->disk = disk;
    sbp->pos = 0;
    sbp->sb_buf = buf;
    sbp->init = 1;
    sbp->filesys_lock = semaphore_new(1);

    return 0;
}

/* Return super block and no write back to disk */
void
sblock_put(mem_sblock_t sbp)
{
    brelse(sbp->sb_buf);
}

/* Return super block and immediately write back to disk */
int
sblock_update(mem_sblock_t sbp)
{
    memcpy(sbp->sb_buf->data, sbp, sizeof(struct sblock));
    bwrite(sbp->sb_buf);
	return 0;
}

/*
 * Fill in the initial data for a super block in memory.
 * Requires getting the super block before this and updating it after.
 */
int
sblock_init(mem_sblock_t sbp, blocknum_t disk_num_blocks)
{
    inodenum_t inode_blocks = disk_num_blocks / INODE_PER_BLOCK + 1;
    blocknum_t bitmap_blocks = disk_num_blocks / BITS_PER_BLOCK + 1;
    if (NULL == sbp)
        return -1;

    sbp->magic_number = MINIFS_MAGIC_NUMBER;
    sbp->disk_num_blocks = disk_num_blocks;

    sbp->total_inodes = disk_num_blocks;
    sbp->first_inode_block = 1;

    sbp->inode_bitmap_first = 1 + inode_blocks;
    sbp->inode_bitmap_last = sbp->inode_bitmap_first + bitmap_blocks - 1;

    sbp->block_bitmap_first = 1 + sbp->inode_bitmap_last;
    sbp->block_bitmap_last = sbp->block_bitmap_first + bitmap_blocks - 1;

    sbp->total_data_blocks = sbp->disk_num_blocks - 1 - sbp->block_bitmap_last;
    sbp->first_data_block = 1 + inode_blocks + 2 * bitmap_blocks;

    sbp->root_inum = 1;

    return 0;
}

/* Check if a file system is valid. Return 0 (false) if invalid. */
int
sblock_isvalid(mem_sblock_t sbp)
{
    return (MINIFS_MAGIC_NUMBER == sbp->magic_number);
}

/* Print the super block in memory in a readable way */
void
sblock_print(mem_sblock_t sbp)
{
    printf("%-40s %-16X\n", "Magic number (4-byte HEX) :", sbp->magic_number);
    printf("%-40s %-16ld\n", "Total disk blocks :", sbp->disk_num_blocks);

    printf("%-40s %-16ld\n", "Total disk inodes :", sbp->total_inodes);
    printf("%-40s %-16ld\n", "Block number of 1st inode :", sbp->first_inode_block);
    printf("%-40s %-16ld\n", "Inode number of root :", sbp->root_inum);

    printf("%-40s %-16ld\n", "First inode-bitmap :", sbp->inode_bitmap_first);
    printf("%-40s %-16ld\n", "Last inode-bitmap :", sbp->inode_bitmap_last);
    printf("%-40s %-16ld\n", "Number of free inodes :", sbp->free_inodes);

    printf("%-40s %-16ld\n", "First block-bitmap :", sbp->block_bitmap_first);
    printf("%-40s %-16ld\n", "Last block-bitmap :", sbp->block_bitmap_last);
    printf("%-40s %-16ld\n", "Number of free blocks :",  sbp->free_blocks);

    printf("%-40s %-16ld\n", "Block number of 1st data block :", sbp->first_data_block);

}

void
fs_lock(mem_sblock_t sbp)
{
    semaphore_P(sbp->filesys_lock);
}

void
fs_unlock(mem_sblock_t sbp)
{
    semaphore_V(sbp->filesys_lock);
}

/* Get a free block from the disk and lock (bread) the block */
buf_block_t
balloc(disk_t* disk)
{
    blocknum_t i;
    blocknum_t free_bit_in_buf = -1;
    blocknum_t free_bit_ind = -1;
    buf_block_t block;

    /* Get super block */
    fs_lock(mainsb);
    if (mainsb->free_blocks <= 0) {
        return NULL;
    }

    /* Get free block number */
    for (i = mainsb->block_bitmap_first; i <= mainsb->block_bitmap_last; ++i) {
        bread(maindisk, i, &block);
        free_bit_in_buf = bitmap_next_zero((bitmap_t) block->data, BITS_PER_BLOCK);
        if (free_bit_in_buf > -1) {
            break;
        }
        brelse(block);
    }
    if (free_bit_in_buf < 0) {
        return NULL;
    }
    free_bit_ind = free_bit_in_buf + i * BITS_PER_BLOCK;
    if (free_bit_ind >= mainsb->disk_num_blocks) {
        brelse(block);
        return NULL;
    }

    /* Indicate block is used */
    bitmap_set((bitmap_t)block->data, free_bit_in_buf);
    bwrite(block);
    mainsb->free_blocks--;
    fs_unlock(mainsb);

    /* Get the block and return */
    bread(disk, free_bit_ind, &block);
    return block;
}

/* Free the block on disk and release control (bwrite) of the block */
void
bfree(buf_block_t block)
{
    blocknum_t bit_in_buf = -1;

    blocknum_t bitmap_block_num;
    buf_block_t bitmap_block;

    /* Get super block */
    fs_lock(mainsb);

    /* Get bitmap block */
    bitmap_block_num = block->num / BITS_PER_BLOCK + mainsb->block_bitmap_first;
    bread(maindisk, bitmap_block_num, &bitmap_block);
    bit_in_buf = block->num % BITS_PER_BLOCK;

    /* Indicate block is used */
    bitmap_set((bitmap_t) block->data, bit_in_buf);
    bwrite(bitmap_block);

    /* Update super block and data block */
    mainsb->free_blocks++;
    fs_unlock(mainsb);
    bwrite(block);
}
//
//int
//blist_check(mem_sblock_t sbp)
//{
//    blocknum_t i, next;
//    buf_block_t block;
//    freenode_t freenode;
//
//    sblock_get(maindisk, mainsb);
//    next = mainsb->free_blist_head;
//    //printf("Next: %ld\n", next);
//    for (i = 0; i < mainsb->free_blocks; ++i) {
//        if (bread(sbp->disk, next, &block) != 0)
//            break;
//        freenode = (freenode_t) block->data;
//        next = freenode->next;
//        //printf("Next: %ld\n", next);
//        brelse(block);
//    }
//    sblock_put(mainsb);
//
//    if (0 == next)
//        return 0;
//    else
//        return -1;
//}

/*
 * Allocate a free inode and return a pointer to free inode
 * Return NULL if fail.
 */
mem_inode_t
ialloc(disk_t* disk)
{
//    inodenum_t freeinode_num;
//    freenode_t freeblk;
//	mem_inode_t new_inode;
//
//	semaphore_P(sb_lock);
//	sblock_get(disk, mainsb);
//	/* No more free blocks */
//    if (mainsb->free_blocks <= 0 || mainsb->free_inodes <= 0) {
//		goto err1;
//    }
//
//    /* Get free block number and next free block */
//    freeinode_num = mainsb->free_ilist_head;
//	if (iget(disk, freeinode_num, &new_inode) != 0) {
//		goto err1;
//	}
//	freeblk = (freenode_t) new_inode;
//    mainsb->free_ilist_head = freeblk->next;
//	mainsb->free_inodes--;
//
//    /* Update superbock and release the empty block */
//    sblock_update(mainsb);
//	semaphore_V(sb_lock);
//    return new_inode;
//
//err1:
//    sblock_put(mainsb);
//    semaphore_V(sb_lock);
    return NULL;
}

/* Add an inode back to free list */
void
ifree(mem_inode_t inode)
{
//    freenode_t freeblk;
//	inodenum_t freeinode_num;
//
//    if (inode->disk->layout.size <= inode->num) {
//        return;
//    }
//
//	semaphore_P(sb_lock);
//	sblock_get(inode->disk, mainsb);
//	iget(inode->disk, inode->num, &inode);
//
//	freeinode_num = inode->num;
//    freeblk = (freenode_t) inode;
//    freeblk->next = mainsb->free_ilist_head;
//    mainsb->free_ilist_head = freeinode_num;
//	mainsb->free_inodes++;
//
//	iupdate(inode);
//    sblock_update(mainsb);
//	semaphore_V(sb_lock);
}

//int
//ilist_check(mem_sblock_t sbp)
//{
//    blocknum_t i, next;
//    mem_inode_t inode;
//    freenode_t freenode;
//
//    sblock_get(maindisk, mainsb);
//    next = mainsb->free_ilist_head;
//    printf("Next: %ld\n", next);
//    for (i = 0; i < mainsb->free_inodes; ++i) {
//        if (iget(sbp->disk, next, &inode) != 0)
//            break;
//        freenode = (freenode_t) inode;
//        next = freenode->next;
//        printf("Next: %ld\n", next);
//        iput(inode);
//    }
//    sblock_put(mainsb);
//
//    if (0 == next)
//        return 0;
//    else
//        return -1;
//}

int
iread(disk_t* disk, inodenum_t n, mem_inode_t *inop)
{
    return 0;
}

int
iwrite(mem_inode_t ino)
{
    return 0;
}

void
irelse(mem_inode_t ino)
{

}

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
    return (bitmap[i] & (1 << bit));
}

/* Find next zero bit */
int
bitmap_next_zero(bitmap_t bitmap, size_t bit_size)
{
    size_t i = 0;
    size_t j = 0;

    while (bit_size > 0) {
        /* Check if the byte contains zero */
        if (((~bitmap[i]) | 0) != 0) {
            for (j = 0; j < 8; ++j) {
                /* Check for the bit that contains zero */
                if (((~bitmap[i]) & (1 << j)) != 0)
                    return (i * 8 + j);
            }
        } else {
            ++i;
        }
    }

    return -1;
}

/* Count number of zero bits */
int
bitmap_count_zero(bitmap_t bitmap, size_t bit_size)
{
    size_t i = 0;
    size_t j = 0;

    while (bit_size > 0) {
        /* Check if the byte contains zero */
        if (((~bitmap[i]) | 0) != 0) {
            for (j = 0; j < 8; ++j) {
                /* Check for the bit that contains zero */
                if (((~bitmap[i]) & (1 << j)) != 0)
                    return (i * 8 + j);
            }
        } else {
            ++i;
        }
    }

    return -1;
}
