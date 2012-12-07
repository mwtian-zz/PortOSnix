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

int inode_test(int *arg)
{
    blocknum_t block[disk_num_blocks];
	mem_inode_t inode[disk_num_blocks];
    inodenum_t inode_num[disk_num_blocks];
    blocknum_t i, j, inode_count;
    char text[DISK_BLOCK_SIZE];

    printf("In inode system test.\n");
    /* Format super block */
    fs_format(mainsb);
    sblock_print(mainsb);

    memset(text, 'z', DISK_BLOCK_SIZE);

    i = 0;
    while ((inode_num[i] = ialloc(maindisk)) != -1) {
        iget(maindisk, inode_num[i], &inode[i]);
        ilock(inode[i]);
        for (j = 0; j < 11; ++j) {
            inode[i]->direct[j] = inode[i]->num + j;
        }
		printf("Written in inode %ld\n", inode_num[i]);
		iunlock(inode[i]);
		i++;
    }
    inode_count = i;
	
    for (i = 0; i < inode_count; ++i) {
        ilock(inode[i]);
        for (j = 0; j < 11; ++j) {
            if (inode[i]->direct[j] != (inode[i]->num + j))
                printf("Error at inode %ld direct %ld\n", inode_num[i], j);
        }
		printf("Checked inode %ld\n", inode_num[i]);
        iunlock(inode[i]);
        iput(inode[i]);
    }
	
	for (i = 0; i < inode_count; i++) {
		iget(maindisk, inode_num[i], &inode[i]);
		ilock(inode[i]);
		printf("Inode number is %d\n", inode[i]->num);
		printf("Direct block number is :\n");
		for (j = 0; j < 11; j++) {
			printf("%d ", inode[i]->direct[j]);
		}
		printf("\n");
		iunlock(inode[i]);
		iput(inode[i]);
	}

//    /* Test block alloca0tion/free */
//    i = 0;
//    while (mainsb->free_blocks > 0) {
//        block[i] = balloc(maindisk);
//        bpush(block[i], text);
//        //printf("Free blocks left: %ld.\n", mainsb->free_blocks);
//        i++;
//    }
//    printf("Allocated all blocks. Free blocks left: %ld.\n", mainsb->free_blocks);
//    printf("Block bit map: ");
//    for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
//        printf("%d ", mainsb->block_bitmap[i]);
//    }
//    printf("\n");
//
//    i = 0;
//    while (mainsb->free_blocks < mainsb->total_data_blocks) {
//        bfree(block[i]);
//        //printf("Free blocks left: %ld.\n", mainsb->free_blocks);
//        i++;
//    }
//    printf("Freed all blocks. Free block left: %ld.\n", mainsb->free_blocks);
//    printf("Block bit map: ");
//    for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
//        printf("%d ", mainsb->block_bitmap[i]);
//    }
//    printf("\n");
//
//    /* Test inode allocation/free */
//    i = 0;
//    while (mainsb->free_inodes > 0) {
//        inode[i] = ialloc(maindisk);
//        //printf("Got inode %ld\n", inode[i]);
//        i++;
//    }
//    printf("Allocated all inodes. Free inodes left: %ld.\n", mainsb->free_inodes);
//    printf("Inode bit map: ");
//    for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
//        printf("%d ", mainsb->inode_bitmap[i]);
//    }
//    printf("\n");
//
//    i = 0;
//    while (mainsb->free_inodes < mainsb->total_inodes) {
//        //printf("Freeing %ld\n", inode[i]);
//        ifree(inode[i]);
//        i++;
//    }
//    printf("Freed all inodes. Free inode left: %ld.\n", mainsb->free_inodes);
//    printf("Inode bit map: ");
//    for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
//        printf("%d ", mainsb->inode_bitmap[i]);
//    }
//    printf("\n");
//
//    printf("Multithreaded allocation starts.\n");
//    sig = semaphore_new(0);
//    for (i = 0; i < 10; ++i)
//        minithread_fork(fs_multithread_alloc, NULL);
//    for (i = 0; i < 10; ++i)
//        semaphore_P(sig);
//    fs_lock(mainsb);
//    printf("Allocated all inodes. Free inodes left: %ld.\n", mainsb->free_inodes);
//    printf("Inode bit map: ");
//    for (i = 0; i < mainsb->disk_num_blocks / 8; ++i) {
//        printf("%d ", mainsb->inode_bitmap[i]);
//    }
//    printf("\n");
//    fs_unlock(mainsb);

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

