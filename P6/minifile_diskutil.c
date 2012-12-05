#include "defs.h"
#include "disk.h"
#include "minifile_cache.h"
#include "minifile_diskutil.h"
#include "minifile_fs.h"

/* Make a file system with parameters specified in disk.h */
int
minifile_remkfs(blocknum_t total_blocks)
{
    buf_block_t buf;
    mem_inode_t inode;
    freespace_t freespace;
    blocknum_t i;

    /* Initialize superblock */
    sblock_get(maindisk, mainsb);
    sblock_init(mainsb, disk_size);
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
