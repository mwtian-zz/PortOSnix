#include <stdio.h>
#include "minifile_fs.h"

int main() {
	printf("Size of inode is %lu\n", sizeof(struct inode));
	printf("Inodes per block is %d\n", INODE_PER_BLOCK);
	printf("Inode 32 is in block %d\n", INODE_TO_BLOCK(32));
	return 0;
}