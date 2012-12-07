#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_fs.h"
#include "minifile_inode.h"
#include "minifile_inodetable.h"
#include "minifile_util.h"

#include "minithread.h"
#include "synch.h"

static blocknum_t disk_num_blocks = 128;
static semaphore_t sig;

int inode_multithread(int *arg)
{
//    inodenum_t inode;
//    while ((inode = ialloc(maindisk)) != -1) {
//        int i;
//        printf("Alocated: %ld\n", inode);
//        printf("Inode bit map: ");
//        for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
//            printf("%d ", mainsb->inode_bitmap[i]);
//        }
//        printf("\n");
//    }
//    semaphore_V(sig);
//    printf("Multithreaded allocation returns.\n");

    return 0;
}

int inode_test(int *arg)
{
    blocknum_t block[disk_num_blocks];
	mem_inode_t inode[disk_num_blocks];
    inodenum_t inode_num[disk_num_blocks];
    blocknum_t i, j, inode_count;
    char text[DISK_BLOCK_SIZE];
	buf_block_t buf;
	blocknum_t k;
	
    printf("In inode system test.\n");
    /* Format super block */
    fs_format(mainsb);
    sblock_print(mainsb);

    memset(text, 'z', DISK_BLOCK_SIZE);

	
    i = 0;
	inode_num[i] = ialloc(maindisk);
	iget(maindisk, inode_num[i], &inode[i]);
	inode[i]->type = MINIFILE;
	while (mainsb->free_blocks > 0) {
		ilock(inode[i]);
		k = balloc(maindisk);
		iadd_block(inode[i], k);
		inode[i]->size_blocks++;
		printf("Adding block %ld\n", k);
		inode[i]->size += DISK_BLOCK_SIZE;
		iupdate(inode[i]);
		iunlock(inode[i]);
	}
	iput(inode[i]);
	
	iget(maindisk, inode_num[i], &inode[i]);
	printf("Data block is:\n");
	for (j = 0; j < inode[i]->size_blocks; j++) {
		printf("%ld ", blockmap(maindisk, inode[i], j));
	}
	printf("\n");
	iput(inode[i]);
	
    return 0;
}

int main(int argc, char** argv)
{
    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = disk_num_blocks;

    minithread_system_initialize(inode_test, NULL);

    return 0;
}

