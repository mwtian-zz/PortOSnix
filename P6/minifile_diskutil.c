#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_diskutil.h"
#include "minifile_fs.h"
#include "minifile_path.h"

/* Make a file system with parameters specified in disk.c */
int
minifile_remkfs(int *arg)
{
    mem_inode_t inode;
    buf_block_t buf;
    blocknum_t blocknum;
    dir_entry_t dir;

    /* Initialize superblock and bit map*/
    fs_format(mainsb);

    /* Create root inode */
    mainsb->root_inum = ialloc(maindisk);
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

    sblock_print(mainsb);
    printf("minifile system established on %s. ", disk_name);
    printf("Hit ^C to quit minithread.\n");

    return 0;
}

/* Check the consistency of the file system on disk */
int
minifile_fsck(int *arg)
{
    char *disk_block_map;
    char *used_block_map;
    int error_count = 0;
    mem_inode_t inode;
    blocknum_t i, j;
    blocknum_t block_free_count = 0;
    blocknum_t blocknum;
    printf("File system checking starts...\n");

    disk_block_map = malloc(mainsb->disk_num_blocks * sizeof(*disk_block_map));
    used_block_map = malloc(mainsb->disk_num_blocks * sizeof(*used_block_map));

    printf("Loading free block map from disk...\n");
    /* Copy over disk bitmap */
    for (i = 0; i < mainsb->disk_num_blocks; ++i) {
        disk_block_map[i] = bitmap_get(mainsb->block_bitmap, i);
        //printf("block %ld: %d\n", i , disk_block_map[i]);
        if (0 == disk_block_map[i])
            block_free_count++;
    }
    if (block_free_count != mainsb->free_blocks) {
        printf("Inconsistent numer of free blocks: system - %ld, bitmap - %ld.",
               mainsb->free_blocks, block_free_count);
        mainsb->free_blocks = block_free_count;
        printf("    Fixed.\n");
    }

    printf("Marking used blocks...\n");
    /* Set file system used blocks */
    for (i = 0; i <= mainsb->block_bitmap_last; ++i) {
        used_block_map[i]++;
    }
    /* Set file used blocks */
    for (i = 1; i < mainsb->total_inodes; ++i) {
        if (bitmap_get(mainsb->inode_bitmap, i) == 1) {
            //printf("checking inode %ld\n", i);
            iget(maindisk, i, &inode);
            for (j = 0; j < inode->size_blocks; ++j) {
                //printf("    logical block %ld ", j);
                blocknum = blockmap(maindisk, inode, j);
                //printf("with block num %ld\n", blocknum);
                used_block_map[blocknum]++;
            }
            iput(inode);
        }
    }

    printf("Comparing disk block map and actual used blocks...\n");
    /* Compare disk bitmap with counted block usage */
    for (i = 0; i < mainsb->disk_num_blocks; ++i) {
        if (disk_block_map[i] != used_block_map[i]) {
            error_count++;
            printf("Inconsistency at block %ld.", i);
            if (disk_block_map[i] == 1 || used_block_map[i] == 0) {
                printf("    Free block marked as used on disk \
                       - fixed by freeing the block in bitmap.");
                bfree(i);
            } else if (disk_block_map[i] == 0 || used_block_map[i] == 1) {
                printf("    Used block marked as free on disk \
                       - fixed by setting the block to used in bitmap.");
                bset(i);
            } else {
                printf("    Unable to fix inconsistency: for block %ld - \
                       on disk count %d, actual used count %d",
                       i, disk_block_map[i], used_block_map[i]);
            }
        }
    }
    printf("Number of inconsistent blocks found: %d\n", error_count);
    printf("File system check finishes. Hit ^C to quit minithread.\n");

    return 0;
}
