#include "minifile_fs.h"
#include "minifile_cache.h"

/* Super block management */
static int sblock_get(disk_t* disk, mem_sblock_t *sbp);
static void sblock_put(mem_sblock_t sb);
static int sblock_update(mem_sblock_t sb);

/* Get super block into a memory struct */
int sblock_get(disk_t* disk, mem_sblock_t *sbp)
{
    mem_sblock_t sb = malloc(sizeof(struct mem_sblock));
    buf_block_t buf;
    if (bread(disk, 0, &buf) != 0)
        return -1;
    memcpy(sb, buf->data, sizeof(struct sblock));
    sb->disk = disk;
    sb->pos = 0;
    sb->buf = buf;
    *sbp = sb;
    return 0;
}

/* Return super block and no write back to disk */
void sblock_put(mem_sblock_t sb)
{
    brelse(sb->buf);
    free(sb);
}

/* Return super block and immediately write back to disk */
int sblock_update(mem_sblock_t sb)
{
    memcpy(buf->data, sb, sizeof(struct sblock));
    bwrite(sb->buf);
    free(sb);
}

/* Get a free block from the disk */
blocknum_t
balloc(disk_t* disk)
{
    mem_sblock_t sb;
    blocknum_t freeblk_num;
    freeblock_t freeblk;
    buf_block_t buf;

    /* Get super block */
    sblock_get(disk, &sb);
    if (sb->free_blocks <= 0) {
        return NULL;
    }

    /* Get free block number and next free block */
    freeblk_num = sb->free_blist_head;
    bread(disk, freeblk_num, &buf);
    freeblk = (freeblock_t) buf->data;
    sb->free_blist_head = freeblk->next;

    /* Update superbock and release the empty block */
    sblock_update(sb);
    brelse(buf);

    return freeblk_num;
}

void
bfree(disk_t* disk, blocknum_t freeblk_num)
{
    mem_sblock_t sb;
    freeblock_t freeblk;
    buf_block_t buf;

    if (disk->layout.size <= freeblk_num) {
        return;
    }

    /* Get super block */
    sblock_get(disk, &sb);

    /* Add to the free block list  */
    bread(disk, freeblk_num, &buf);
    freeblk = (freeblock_t) buf->data;
    freeblk->next = sb->free_blist_head;
    sb->free_blist_head = freeblk_num;

    /* Update superbock and the new free block */
    sblock_update(sb);
    bwrite(buf);
}

/* Allocate a free inode */
blocknum_t
ialloc(disk_t* disk)
{

}

/* Add an inode back to free list */
void
ifree(disk_t* disk, blocknum_t n)
{

}

/* Clear the content of an inode, including indirect blocks */
int
iclear(disk_t* disk, blocknum_t n)
{

}

/* Get the content of the inode */
int iget(disk_t* disk, blocknum_t n, mem_inode_t *inodep)
{

}

/* Return the inode and no write to disk */
void iput(mem_inode_t inode)
{

}

/* Return the inode and update it on the disk */
int iupdate(mem_inode_t inode)
{

}
