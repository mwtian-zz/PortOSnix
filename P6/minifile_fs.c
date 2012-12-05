#include "minifile_fs.h"
#include "minifile_cache.h"
#include "minithread.h"
#include <string.h>
#include "minithread_private.h"
#include "minifile_inode.h"


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
    sbp->buf = buf;
    sbp->init = 1;

    return 0;
}

/* Return super block and no write back to disk */
void
sblock_put(mem_sblock_t sbp)
{
    brelse(sbp->buf);
}

/* Return super block and immediately write back to disk */
int
sblock_update(mem_sblock_t sbp)
{
    memcpy(sbp->buf->data, sbp, sizeof(struct sblock));
    bwrite(sbp->buf);
	return 0;
}

/*
 * Fill in the initial data for a super block in memory.
 * Requires getting the super block before this and updating it after.
 */
int
sblock_init(mem_sblock_t sbp, blocknum_t total_blocks)
{
    inodenum_t inode_blocks = total_blocks / INODE_PER_BLOCK;
    if (NULL == sbp)
        return -1;

    sbp->magic_number = MINIFS_MAGIC_NUMBER;
    sbp->total_blocks = total_blocks;
    sbp->free_blist_head = 1 + inode_blocks;
    sbp->free_blist_tail = total_blocks - 1;
    sbp->free_blocks = total_blocks - inode_blocks - 1;
    sbp->total_inodes = total_blocks;
    sbp->free_ilist_head = 1;
    sbp->free_ilist_tail = total_blocks;
    sbp->free_inodes = total_blocks;
    sbp->first_inode_block = 1;
    sbp->first_data_block = 1 + inode_blocks;
    sbp->root_inum = 1;

    return 0;
}

/* Check if a file system is valid. Return 0 (false) if invalid. */
int
sblock_isvalid(mem_sblock_t sbp)
{
    if (MINIFS_MAGIC_NUMBER == sbp->magic_number)
        return 1;
    else
        return 0;
}

/* Print the super block in memory in a readable way */
void
sblock_print(mem_sblock_t sbp)
{
    printf("%-40s %-16X\n", "Magic number (4-byte HEX) :", sbp->magic_number);
    printf("%-40s %-16ld\n", "Total disk blocks :", sbp->total_blocks);
    printf("%-40s %-16ld\n", "Free block head :", sbp->free_blist_head);
    printf("%-40s %-16ld\n", "Free block tail :", sbp->free_blist_tail);
    printf("%-40s %-16ld\n", "Number of free blocks :",  sbp->free_blocks);
    printf("%-40s %-16ld\n", "Total disk inodes :", sbp->total_inodes);
    printf("%-40s %-16ld\n", "Free inode head :", sbp->free_ilist_head);
    printf("%-40s %-16ld\n", "Free inode tail :", sbp->free_ilist_tail);
    printf("%-40s %-16ld\n", "Number of free inodes :", sbp->free_inodes);

    printf("%-40s %-16ld\n", "Block number of 1st inode :", sbp->first_inode_block);
    printf("%-40s %-16ld\n", "Block number of 1st data block :", sbp->first_data_block);
    printf("%-40s %-16ld\n", "Block number of root :", sbp->root_inum);
}

/* Get a free block from the disk and lock (bread) the block */
buf_block_t
balloc(disk_t* disk)
{
    blocknum_t freeblk_num;
    freespace_t freeblk;
    buf_block_t buf;

    /* Get super block */
    sblock_get(disk, mainsb);
    if (mainsb->free_blocks <= 0) {
        return NULL;
    }

    /* Get free block number and next free block */
    freeblk_num = mainsb->free_blist_head;
    bread(disk, freeblk_num, &buf);
    freeblk = (freespace_t) buf->data;
    mainsb->free_blist_head = freeblk->next;
    mainsb->free_blocks--;
    if (0 == mainsb->free_blocks) {
        mainsb->free_blist_head = 0;
        mainsb->free_blist_tail = 0;
    }

    /* Update superbock and release the empty block */
    sblock_update(mainsb);

    return buf;
}

/* Free the block on disk and release control (bwrite) of the block */
void
bfree(buf_block_t block)
{
    freespace_t freeblk;

    /* Get super block */
    sblock_get(block->disk, mainsb);

    /* Add to the free block list  */
    freeblk = (freespace_t) block->data;
    freeblk->next = mainsb->free_blist_head;
    mainsb->free_blist_head = block->num;
    mainsb->free_blocks++;
    if (1 == mainsb->free_blocks) {
        mainsb->free_blist_head = block->num;
        mainsb->free_blist_tail = block->num;
    }

    /* Update superbock and the new free block */
    sblock_update(mainsb);
    bwrite(buf);
}

int
blist_check(mem_sblock_t sbp)
{

}

/*
 * Allocate a free inode and return a pointer to free inode
 * Return NULL if fail.
 */
mem_inode_t
ialloc(disk_t* disk)
{
    inodenum_t freeinode_num;
    freespace_t freeblk;
	mem_inode_t new_inode;

	semaphore_P(sb_lock);
	sblock_get(disk, mainsb);
	/* No more free blocks */
    if (mainsb->free_blocks <= 0 || mainsb->free_inodes <= 0) {
		goto err1;
    }

    /* Get free block number and next free block */
    freeinode_num = mainsb->free_ilist_head;
	if (iget(disk, freeinode_num, &new_inode) != 0) {
		goto err1;
	}
	freeblk = (freespace_t) new_inode;
    mainsb->free_ilist_head = freeblk->next;
	mainsb->free_inodes--;

    /* Update superbock and release the empty block */
    sblock_update(mainsb);
	semaphore_V(sb_lock);
    return new_inode;

err1:
    sblock_put(mainsb);
    semaphore_V(sb_lock);
    return NULL;
}

/* Add an inode back to free list */
void
ifree(mem_inode_t inode)
{
    freespace_t freeblk;
	inodenum_t freeinode_num;

    if (inode->disk->layout.size <= inode->num) {
        return;
    }

	semaphore_P(sb_lock);
	sblock_get(inode->disk, mainsb);
	iget(inode->disk, inode->num, &inode);

	freeinode_num = inode->num;
    freeblk = (freespace_t) inode;
    freeblk->next = mainsb->free_ilist_head;
    mainsb->free_ilist_head = freeinode_num;
	mainsb->free_inodes++;

	iupdate(inode);
    sblock_update(mainsb);
	semaphore_V(sb_lock);
}

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

void irelse(mem_inode_t ino)
{

}

