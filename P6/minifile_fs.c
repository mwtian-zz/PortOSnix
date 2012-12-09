#include <string.h>
#include "defs.h"

#include "minifile_fs.h"
#include "minifile_cache.h"
#include "minithread.h"
#include "minithread_private.h"
#include "minifile_inode.h"
#include "synch.h"

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
sblock_format(mem_sblock_t sbp, blocknum_t disk_num_blocks)
{
    inodenum_t inode_blocks = (disk_num_blocks - 1) / INODE_PER_BLOCK + 1;
    blocknum_t bitmap_blocks = (disk_num_blocks - 1) / BITS_PER_BLOCK + 1;
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

/* Establish the file system and write it to disk */
int
fs_format(mem_sblock_t sbp)
{
    blocknum_t inode_bitmap_size;
    blocknum_t block_bitmap_size;
    blocknum_t i;
    sbp->filesys_lock = semaphore_new(1);
    if (NULL == sbp->filesys_lock) {
        return -1;
    }

    fs_lock(sbp);

    /* Format super block */
    sblock_get(maindisk, sbp);
    sblock_format(sbp, disk_size);
    sblock_update(sbp);

    /* Format inode and block bitmaps */
    inode_bitmap_size = sbp->inode_bitmap_last - sbp->inode_bitmap_first + 1;
    sbp->inode_bitmap = malloc(inode_bitmap_size * DISK_BLOCK_SIZE);
    block_bitmap_size = sbp->block_bitmap_last - sbp->block_bitmap_first + 1;
    sbp->block_bitmap = malloc(block_bitmap_size * DISK_BLOCK_SIZE);
    if (NULL == sbp->block_bitmap || NULL == sbp->inode_bitmap) {
        semaphore_V(sbp->filesys_lock);
        return -1;
    }
    /* Clear bitmaps */
    bitmap_zeroall(sbp->inode_bitmap, inode_bitmap_size * BITS_PER_BLOCK);
    bitmap_zeroall(sbp->block_bitmap, block_bitmap_size * BITS_PER_BLOCK);
    /* Set file system blocks to be occupied */
    for (i = 0; i <= sbp->block_bitmap_last; ++i) {
        bitmap_set(sbp->block_bitmap, i);
    }
    /* Set inode 0 to be occupied */
    bitmap_set(sbp->inode_bitmap, 0);
    /* Push updates to disk */
    for (i = sbp->inode_bitmap_first; i <= sbp->inode_bitmap_last; ++i) {
        bpush(i, (char*) sbp->inode_bitmap + (i - sbp->inode_bitmap_first)
              * DISK_BLOCK_SIZE);
    }
    for (i = sbp->block_bitmap_first; i <= sbp->block_bitmap_last; ++i) {
        bpush(i, (char*) sbp->block_bitmap + (i - sbp->block_bitmap_first)
              * DISK_BLOCK_SIZE);
    }

    /* Count free inodes and free blocks */
    mainsb->free_inodes = bitmap_count_zero(mainsb->inode_bitmap,
                                            mainsb->total_inodes);
    mainsb->free_blocks = bitmap_count_zero(mainsb->block_bitmap,
                                            mainsb->disk_num_blocks);

    fs_unlock(sbp);
    return 0;
}

/* Initialize the file system structure from disk */
int
fs_init(mem_sblock_t sbp)
{
    blocknum_t inode_bitmap_size;
    blocknum_t block_bitmap_size;
    blocknum_t i;
    sbp->filesys_lock = semaphore_new(1);
    if (NULL == sbp->filesys_lock) {
        return -1;
    }

    fs_lock(sbp);
    sblock_get(maindisk, sbp);
    sblock_put(sbp);
    if (sblock_isvalid(sbp) != 1) {
        sblock_print(sbp);
        kprintf("File system is not recognized. ");
        kprintf("Recommend running './mkfs <blocks>'.\n");
        return -2;
    }

    inode_bitmap_size = sbp->inode_bitmap_last - sbp->inode_bitmap_first + 1;
    sbp->inode_bitmap = malloc(inode_bitmap_size * DISK_BLOCK_SIZE);
    block_bitmap_size = sbp->block_bitmap_last - sbp->block_bitmap_first + 1;
    sbp->block_bitmap = malloc(block_bitmap_size * DISK_BLOCK_SIZE);
    if (NULL == sbp->block_bitmap || NULL == sbp->inode_bitmap) {
        semaphore_V(sbp->filesys_lock);
        return -1;
    }

    /* Get disk bitmap */
    for (i = sbp->inode_bitmap_first; i <= sbp->inode_bitmap_last; ++i) {
        bpull(i, (char*) sbp->inode_bitmap + (i - sbp->inode_bitmap_first)
              * DISK_BLOCK_SIZE);
    }
    for (i = sbp->block_bitmap_first; i <= sbp->block_bitmap_last; ++i) {
        bpull(i, (char*) sbp->block_bitmap + (i - sbp->block_bitmap_first)
              * DISK_BLOCK_SIZE);
    }

    /* Count free inodes and free blocks */
    mainsb->free_inodes = bitmap_count_zero(mainsb->inode_bitmap,
                                            mainsb->total_inodes);
    mainsb->free_blocks = bitmap_count_zero(mainsb->block_bitmap,
                                            mainsb->disk_num_blocks);

    fs_unlock(sbp);

    return 0;
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

/* Get a free block number from the disk and mark the block used */
blocknum_t
balloc(disk_t* disk)
{
    blocknum_t free_bit = -1;
    blocknum_t block_offset = -1;

    /* Get super block */
    fs_lock(mainsb);
    if (mainsb->free_blocks <= 0) {
        fs_unlock(mainsb);
        return -1;
    }

    /* Get free block number */
    free_bit = bitmap_next_zero(mainsb->block_bitmap, mainsb->disk_num_blocks);
    block_offset = free_bit / BITS_PER_BLOCK;
    if (block_offset < 0 || block_offset >= mainsb->disk_num_blocks) {
        fs_unlock(mainsb);
        return -1;
    }

    /* Set the free block to used */
    bitmap_set(mainsb->block_bitmap, free_bit);
    bpush(block_offset + mainsb->block_bitmap_first,
          (char*) mainsb->block_bitmap + block_offset * DISK_BLOCK_SIZE);

    mainsb->free_blocks--;
    fs_unlock(mainsb);

    return free_bit;
}

/* Free the block on disk and release control (bwrite) of the block */
void
bfree(blocknum_t blocknum)
{
    blocknum_t bit = -1;
    blocknum_t block_offset;

    fs_lock(mainsb);
    if (blocknum <= mainsb->block_bitmap_last || blocknum >=mainsb->disk_num_blocks) {
        fs_unlock(mainsb);
        return;
    }

    /* Set the block to free */
    bit = blocknum;
    block_offset = bit / BITS_PER_BLOCK;

    if (bitmap_get(mainsb->block_bitmap, bit) == 1) {
        bitmap_clear(mainsb->block_bitmap, bit);
        bpush(block_offset + mainsb->block_bitmap_first,
            (char*) mainsb->block_bitmap + block_offset * DISK_BLOCK_SIZE);
        mainsb->free_blocks++;
    }

    fs_unlock(mainsb);
}

/* Set a block to used */
void
bset(blocknum_t blocknum)
{
    blocknum_t bit = -1;
    blocknum_t block_offset;

    fs_lock(mainsb);
    if (blocknum <= mainsb->block_bitmap_last || blocknum >=mainsb->disk_num_blocks) {
        fs_unlock(mainsb);
        return;
    }

    /* Set the block to free */
    bit = blocknum;
    block_offset = bit / BITS_PER_BLOCK;

    if (bitmap_get(mainsb->block_bitmap, bit) == 0) {
        bitmap_set(mainsb->block_bitmap, bit);
        bpush(block_offset + mainsb->block_bitmap_first,
            (char*) mainsb->block_bitmap + block_offset * DISK_BLOCK_SIZE);
        mainsb->free_blocks--;
    }

    fs_unlock(mainsb);
}

/*
 * Allocate a free inode and return a pointer to free inode
 * Return NULL if fail.
 */
inodenum_t
ialloc(disk_t* disk)
{
    inodenum_t free_bit = -1;
    inodenum_t block_offset = -1;

    fs_lock(mainsb);
    if (mainsb->free_inodes <= 0) {
        fs_unlock(mainsb);
        return -1;
    }

    /* Get free inode number */
    free_bit = bitmap_next_zero(mainsb->inode_bitmap, mainsb->disk_num_blocks);
    block_offset = free_bit / BITS_PER_BLOCK;
    if (free_bit < 0 || block_offset >= mainsb->disk_num_blocks) {
        fs_unlock(mainsb);
        return -1;
    }

    /* Set the free inode to used */
    bitmap_set(mainsb->inode_bitmap, free_bit);
    bpush(block_offset + mainsb->inode_bitmap_first,
          (char*) mainsb->inode_bitmap + block_offset * DISK_BLOCK_SIZE);

    mainsb->free_inodes--;
    fs_unlock(mainsb);

    return free_bit;
}

/* Add an inode back to free list */
void
ifree(inodenum_t inum)
{
    blocknum_t bit = -1;
    blocknum_t block_offset;

    fs_lock(mainsb);

    /* Set the inode to free */
    bit = inum;
    block_offset = bit / BITS_PER_BLOCK;
    if (bitmap_get(mainsb->inode_bitmap, bit) == 1) {
        mainsb->free_inodes++;
    }
    bitmap_clear(mainsb->inode_bitmap, bit);
    bpush(block_offset + mainsb->inode_bitmap_first,
          (char*) mainsb->inode_bitmap + block_offset * DISK_BLOCK_SIZE);

    fs_unlock(mainsb);
}
