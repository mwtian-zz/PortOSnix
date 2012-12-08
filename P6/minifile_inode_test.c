#include "defs.h"

#include "disk.h"
#include "minifile_cache.h"
#include "minifile_fs.h"
#include "minifile_inode.h"
#include "minifile_inodetable.h"

#include "minithread.h"
#include "synch.h"

/* This value should be limited to not overflow inode_test stack */
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

