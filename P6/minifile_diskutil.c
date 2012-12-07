#include "defs.h"
#include "disk.h"
#include "minifile_cache.h"
#include "minifile_diskutil.h"
#include "minifile_fs.h"

/* Make a file system with parameters specified in disk.h */
int
minifile_remkfs()
{
    buf_block_t buf;
    mem_inode_t inode;
    freenode_t freenode;
    blocknum_t i;

    /* Initialize superblock */
    sblock_get(maindisk, mainsb);
    sblock_format(mainsb, disk_size);
    sblock_update(mainsb);

    /* Initialize root inode */
    iget(maindisk, 1, &inode);
    inode->type = MINIDIRECTORY;
    inode->size = 0;
    iupdate(inode);

//    /* Initialize free inode list */
//    for (i = mainsb->free_ilist_head; i < mainsb->free_ilist_tail; ++i) {
//        iget(maindisk, i, &inode);
//        freenode = (freenode_t) inode;
//        freenode->next = i + 1;
//        iupdate(inode);
//    }
//    iget(maindisk, mainsb->free_ilist_tail, &inode);
//    freenode = (freenode_t) inode;
//    freenode->next = 0;
//    iupdate(inode);
//
//    /* Initialize free block list */
//    for (i = mainsb->free_blist_head; i < mainsb->free_blist_tail; ++i) {
//        bread(maindisk, i, &buf);
//        freenode = (freenode_t) buf->data;
//        freenode->next = i + 1;
//        bwrite(buf);
//    }
//    bread(maindisk, mainsb->free_blist_tail, &buf);
//    freenode = (freenode_t) buf->data;
//    freenode->next = 0;
//    bwrite(buf);

    printf("minifile system established.\n");

    return 0;
}

/* Check the consistency of the file system on disk */
int
minifile_fsck(disk_t* disk)
{

    return 0;
}
