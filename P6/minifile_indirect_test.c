#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_fs.h"
#include "minifile_inode.h"
#include "minifile_inodetable.h"

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

int
inode_test(int *arg)
{
    blocknum_t block[disk_num_blocks];
	mem_inode_t inode[disk_num_blocks];
    inodenum_t inode_num[disk_num_blocks];
    blocknum_t i, j;
    char text[DISK_BLOCK_SIZE];
	blocknum_t k;

    printf("In inode system test.\n");
    /* Format super block */
    fs_format(mainsb);
    sblock_print(mainsb);

    memset(text, 'z', DISK_BLOCK_SIZE);

    i = 0;
	inode_num[i] = ialloc(maindisk);
	/*
	iget(maindisk, inode_num[i], &inode[i]);
	inode[i]->type = MINIFILE;
	printf("Allocating all free blocks.\n");
	while (mainsb->free_blocks > 0) {
		ilock(inode[i]);
		k = balloc(maindisk);
		//printf("Adding block %ld\n", k);
		iadd_block(inode[i], k);
		inode[i]->size_blocks++;
		inode[i]->size += DISK_BLOCK_SIZE;
		iupdate(inode[i]);
		iunlock(inode[i]);
	}
	iput(inode[i]);

	iget(maindisk, inode_num[i], &inode[i]);
	printf("Data blocks are:\n");
	for (j = 0; j < inode[i]->size_blocks; j++) {
		printf("%ld ", blockmap(maindisk, inode[i], j));
	}
	printf("\n");
	iput(inode[i]);
    printf("Free block left: %ld.\n", mainsb->free_blocks);

	iget(maindisk, inode_num[i], &inode[i]);
	printf("Clearing data blocks.\n");
	iclear(inode[i]);
	printf("Inode direct pointers: ");
	for (j = 0; j < 11; j++) {
		printf("%ld ", inode[i]->direct[i]);
	}
    printf("\n");
    printf("Inode indirect pointers: ");
    printf("%ld ", inode[i]->indirect);
    printf("%ld ", inode[i]->double_indirect);
    printf("%ld ", inode[i]->triple_indirect);
    printf("\n");
    printf("Inode indirect pointers: ");
    printf("Free block left: %ld.\n", mainsb->free_blocks);
    iput(inode[i]);
    printf("Block bit map: ");
    for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
        printf("%d ", mainsb->block_bitmap[i]);
    }
    printf("\n");
    */
    printf("Allocating all free blocks.\n");
    inode[i]->type = MINIFILE;
    iget(maindisk, inode_num[i], &inode[i]);
    while (mainsb->free_blocks > 0) {
		ilock(inode[i]);
		k = balloc(maindisk);
		//printf("Adding block %ld\n", k);
		iadd_block(inode[i], k);
		inode[i]->size_blocks++;
		inode[i]->size += DISK_BLOCK_SIZE;
		iupdate(inode[i]);
		iunlock(inode[i]);
	}
	iput(inode[i]);

	iget(maindisk, inode_num[i], &inode[i]);
	printf("Data blocks are:\n");
	for (j = 0; j < inode[i]->size_blocks; j++) {
		printf("%ld ", blockmap(maindisk, inode[i], j));
	}
	printf("\n");
	iput(inode[i]);
    printf("Free block left: %ld.\n", mainsb->free_blocks);

	iget(maindisk, inode_num[i], &inode[i]);
	printf("Removing data blocks one by one.\n");
	while (irm_block(inode[i]) == 0)
        ;
	printf("Inode direct pointers: ");
	for (j = 0; j < 11; j++) {
		printf("%ld ", inode[i]->direct[i]);
	}
    printf("\n");
    printf("Inode indirect pointers: ");
    printf("%ld ", inode[i]->indirect);
    printf("%ld ", inode[i]->double_indirect);
    printf("%ld ", inode[i]->triple_indirect);
    printf("\n");
    printf("Inode indirect pointers: ");
    printf("Free block left: %ld.\n", mainsb->free_blocks);
    iput(inode[i]);

    return 0;
}

int
main(int argc, char** argv)
{
    use_existing_disk = 0;
    disk_name = "minidisk";
    disk_flags = DISK_READWRITE;
    disk_size = disk_num_blocks;

    minithread_system_initialize(inode_test, NULL);

    return 0;
}

