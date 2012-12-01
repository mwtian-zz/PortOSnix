#include <stdio.h>
#include "minifile_fs.h"

int main() {
	printf("Size of inode is %lu\n", sizeof(struct inode));
	printf("Inodes per block is %d\n", INODE_PER_BLOCK);
	printf("Inode 32 is in block %d\n", INODE_TO_BLOCK(32));
	printf("Inode 32 offset is %d\n", INODE_OFFSET(32));
	printf("Inode 33 offset is %d\n", INODE_OFFSET(33));
	printf("Inode 100 offset is %d\n", INODE_OFFSET(100));
	return 0;
}