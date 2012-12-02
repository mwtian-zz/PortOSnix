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
    memcpy(sb->buf->data, sb, sizeof(struct sblock));
    bwrite(sb->buf);
    free(sb);
}

/* Get a free block from the disk */
blocknum_t
balloc(disk_t* disk)
{
    mem_sblock_t sb;
    blocknum_t freeblk_num;
    freespace_t freeblk;
    buf_block_t buf;

    /* Get super block */
    sblock_get(disk, &sb);
    if (sb->free_blocks <= 0) {
        return NULL;
    }

    /* Get free block number and next free block */
    freeblk_num = sb->free_blist_head;
    bread(disk, freeblk_num, &buf);
    freeblk = (freespace_t) buf->data;
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
    freespace_t freeblk;
    buf_block_t buf;

    if (disk->layout.size <= freeblk_num) {
        return;
    }

    /* Get super block */
    sblock_get(disk, &sb);

    /* Add to the free block list  */
    bread(disk, freeblk_num, &buf);
    freeblk = (freespace_t) buf->data;
    freeblk->next = sb->free_blist_head;
    sb->free_blist_head = freeblk_num;

    /* Update superbock and the new free block */
    sblock_update(sb);
    bwrite(buf);
}

/* Allocate a free inode */
inodenum_t
ialloc(disk_t* disk)
{
    mem_sblock_t sb;
    inodenum_t freeino_num;
    freespace_t freeino;
    buf_block_t buf;

    /* Get super block */
    sblock_get(disk, &sb);
    if (sb->free_blocks <= 0) {
        return NULL;
    }

    /* Get free block number and next free block */
    freeino_num = sb->free_ilist_head;
    bread(disk, freeino_num, &buf);
    freeino = (freespace_t) buf->data;
    sb->free_ilist_head = freeino->next;

    /* Update superbock and release the empty block */
    sblock_update(sb);
    brelse(buf);

    return freeino_num;
}

/* Add an inode back to free list */
void
ifree(disk_t* disk, inodenum_t n)
{
    mem_sblock_t sb;
    freespace_t freeblk;
    buf_block_t buf;

    if (disk->layout.size <= freeblk_num) {
        return;
    }

    /* Get super block */
    sblock_get(disk, &sb);

    /* Add to the free block list  */
    bread(disk, freeblk_num, &buf);
    freeblk = (freespace_t) buf->data;
    freeblk->next = sb->free_ilist_head;
    sb->free_ilist_head = freeblk_num;

    /* Update superbock and the new free block */
    sblock_update(sb);
    bwrite(buf);
}

/* Clear the content of an inode, including indirect blocks */
int
iclear(disk_t* disk, inodenum_t n)
{

}

/* Get the content of the inode with inode number n*/
int iget(disk_t* disk, inodenum_t n, mem_inode_t *inop)
{
	blocknum_t block_to_read = INODE_TO_BLOCK(n);
	mem_inode_t in = malloc(sizeof(struct mem_inode));
	if (in == NULL) {
		return -1;
	}
    buf_block_t buf;
    if (bread(disk, block_to_read, &buf) != 0) {
		free(in);
		return -1;
	}
    memcpy(in, buf->data + INODE_OFFSET(n), sizeof(struct inode));
    in->disk = disk;
    in->num = n;
    in->buf = buf;
    in->size_blocks = in->size / DISK_BLOCK_SIZE + 1;
    *inop = in;
    return 0;
}

/* Return the inode and no write to disk */
void iput(mem_inode_t ino)
{
    brelse(ino->buf);
    free(ino);
}

/* Return the inode and update it on the disk */
int iupdate(mem_inode_t ino)
{
    memcpy(ino->buf->data + INODE_OFFSET(ino->num), ino, sizeof(struct inode));
    bwrite(ino->buf);
    free(ino);
}
