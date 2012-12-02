#include "minifile_fs.h"
#include "minifile_cache.h"


/* Get super block into a memory struct */
int sblock_get(disk_t* disk, mem_sblock_t sbp)
{
    buf_block_t buf;
    if (bread(disk, 0, &buf) != 0)
        return -1;
    memcpy(sbp, buf->data, sizeof(struct sblock));
    sbp->disk = disk;
    sbp->pos = 0;
    sbp->buf = buf;

    return 0;
}

/* Return super block and no write back to disk */
void sblock_put(mem_sblock_t sbp)
{
    brelse(sbp->buf);
}

/* Return super block and immediately write back to disk */
int sblock_update(mem_sblock_t sbp)
{
    memcpy(sbp->buf->data, sbp, sizeof(struct sblock));
    bwrite(sbp->buf);
	return 0;
}

/* Get a free block from the disk */
blocknum_t
balloc(disk_t* disk)
{
    blocknum_t freeblk_num;
    freespace_t freeblk;
    buf_block_t buf;

    /* Get super block */
    sblock_get(disk, sb);
    if (sb->free_blocks <= 0) {
        return -1;
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
    freespace_t freeblk;
    buf_block_t buf;

    if (disk->layout.size <= freeblk_num) {
        return;
    }

    /* Get super block */
    sblock_get(disk, sb);

    /* Add to the free block list  */
    bread(disk, freeblk_num, &buf);
    freeblk = (freespace_t) buf->data;
    freeblk->next = sb->free_blist_head;
    sb->free_blist_head = freeblk_num;

    /* Update superbock and the new free block */
    sblock_update(sb);
    bwrite(buf);
}

/*
 * Allocate a free inode and return a pointer to free inode
 * Return NULL if fail. May need locks.
 */
mem_inode_t
ialloc(disk_t* disk)
{
    inodenum_t freeinode_num;
    freespace_t freeblk;
    buf_block_t buf;
	blocknum_t block_to_read;
	mem_inode_t new_inode;

    /* Get super block */
    sblock_get(disk, sb);
    if (sb->free_blocks <= 0) {
        return NULL;
    }

    /* Get free block number and next free block */
    freeinode_num = sb->free_ilist_head;
	block_to_read = INODE_TO_BLOCK(freeinode_num);
    bread(disk, block_to_read, &buf);
    freeblk = (freespace_t) (buf->data + INODE_OFFSET(freeinode_num));
	new_inode = malloc(sizeof(struct mem_inode));
	/* If fails allocating new inode memory, don't update super block */
	if (new_inode == NULL) {
		return NULL;
	}
	memcpy(new_inode, freeblk, sizeof(struct inode));
	new_inode->num = freeinode_num;
	new_inode->disk = disk;
	new_inode->buf = buf;
	new_inode->ref_count = 0;   /* ref count may need to be 1 */
	new_inode->size = 0;

    sb->free_ilist_head = freeblk->next;

    /* Update superbock and release the empty block */
    sblock_update(sb);
    brelse(buf);

    return new_inode;
}

/* Add an inode back to free list */
void
ifree(disk_t* disk, inodenum_t n)
{
    freespace_t freeblk;
    buf_block_t buf;
	blocknum_t freeblk_num;

    if (disk->layout.size <= n) {
        return;
    }

    /* Get super block */
    sblock_get(disk, sb);
	freeblk_num = INODE_TO_BLOCK(n);
    /* Add to the free block list  */
    bread(disk, freeblk_num, &buf);
    freeblk = (freespace_t) (buf->data + INODE_OFFSET(n));
    freeblk->next = sb->free_ilist_head;
    sb->free_ilist_head = freeblk_num;
	sb->free_blocks++;
    /* Update superbock and the new free block */
    sblock_update(sb);
    bwrite(buf);
}

/* Clear the content of an inode, including indirect blocks */
int
iclear(disk_t* disk, inodenum_t n)
{
	return 0;
}

/* Get the content of the inode with inode number n*/
int iget(disk_t* disk, inodenum_t n, mem_inode_t *inop)
{
	blocknum_t block_to_read = INODE_TO_BLOCK(n);
	mem_inode_t in;

    buf_block_t buf;
    if (bread(disk, block_to_read, &buf) != 0) {
		return -1;
	}

	in = malloc(sizeof(struct mem_inode));
	if (in == NULL) {
		return -1;
	}
    memcpy(in, buf->data + INODE_OFFSET(n), sizeof(struct inode));
    in->disk = disk;
    in->num = n;
    in->buf = buf;
	/* May need to increment ref count */
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
	return 0;
}
