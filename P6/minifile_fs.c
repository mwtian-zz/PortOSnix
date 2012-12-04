#include "minifile_fs.h"
#include "minifile_cache.h"
#include "minithread.h"
#include <string.h>
#include "minithread_private.h"
#include "minifile_inode.h"


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



