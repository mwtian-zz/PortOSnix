#include "defs.h"
#include "disk.h"
#include "minifile_cache.h"
#include "minifile_diskutil.h"
#include "minifile_fs.h"

/* Make a file system with the specified number of blocks */
int
minifile_remkfs()
{
    buf_block_t buf;
    mem_inode_t inode;
    freespace_t freespace;
    blocknum_t i;

    /* Initialize superblock */
    sblock_get(maindisk, mainsb);
    mainsb = (mem_sblock_t) buf->data;
    mainsb->total_blocks = maindisk->layout.size;
    mainsb->total_inodes = mainsb->total_blocks / 10;
    mainsb->free_ilist_head = 2;
    mainsb->free_ilist_tail = mainsb->total_inodes;
    mainsb->free_inodes = mainsb->total_inodes - 1;
    mainsb->free_blist_head = mainsb->total_inodes + 1;
    mainsb->free_blist_tail = mainsb->total_blocks - 1;
    mainsb->free_blocks = mainsb->free_blist_tail - mainsb->free_blist_head + 1;
    sblock_update(mainsb);

    /* Initialize root inode */
    iget(maindisk, 1, &inode);
    inode->type = MINIDIRECTORY;
    inode->size = 0;
    iupdate(inode);

    /* Initialize free inode list */
    for (i = mainsb->free_ilist_head; i < mainsb->free_ilist_tail; ++i) {
        iget(maindisk, i, &inode);
        freespace = (freespace_t) inode;
        freespace->next = i + 1;
        iupdate(inode);
    }
    iget(maindisk, mainsb->free_ilist_tail, &inode);
    freespace = (freespace_t) inode;
    freespace->next = 0;
    iupdate(inode);

    /* Initialize free block list */
    for (i = mainsb->free_blist_head; i < mainsb->free_blist_tail; ++i) {
        bread(maindisk, i, &buf);
        freespace = (freespace_t) buf->data;
        freespace->next = i + 1;
        bwrite(buf);
    }
    bread(maindisk, mainsb->free_blist_tail, &buf);
    freespace = (freespace_t) buf->data;
    freespace->next = 0;
    bwrite(buf);

    printf("minifile system established.\n");

    return 0;
}

/* Check the consistency of the file system on disk */
int
minifile_fsck(disk_t* disk)
{

    return 0;
}
