#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_diskutil.h"
#include "minifile_fs.h"
#include "minifile_path.h"

/* Make a file system with parameters specified in disk.c */
int
minifile_remkfs()
{
    mem_inode_t inode;
    buf_block_t buf;
    blocknum_t blocknum;
    dir_entry_t dir;

    /* Initialize superblock and bit map*/
    fs_format(mainsb);

    /* Create root inode */
    iget(maindisk, mainsb->root_inum, &inode);
    ilock(inode);
    inode->type = MINIDIRECTORY;
    inode->size = 2;
    blocknum = balloc(maindisk);
    iadd_block(inode, blocknum);
    iupdate(inode);
    iunlock(inode);

    /* Create root inode entries */
    bread(maindisk, blocknum, &buf);
    dir = (dir_entry_t) buf->data;
    strcpy(dir[0].name, ".");
    dir[0].inode_num = mainsb->root_inum;
    strcpy(dir[1].name, "..");
    dir[1].inode_num = mainsb->root_inum;
    bwrite(buf);

    iput(inode);

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
