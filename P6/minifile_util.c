#include "minifile_util.h"
#include "minifile_inode.h"
#include <stdlib.h>
#include <string.h>

/* Indirect block management */
static blocknum_t indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);
static blocknum_t double_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);
static blocknum_t triple_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);


/*
 * Block offset to block number
 * Return -1 on error
 */
blocknum_t
blockmap(disk_t* disk, mem_inode_t ino, size_t block_offset) {
	size_t offset;

	/* Too small or too large */
	if (block_offset < 0 || block_offset >= INODE_MAX_FILE_BLOCKS) {
		return -1;
	}

	/* In direct block */
	if (block_offset < INODE_DIRECT_BLOCKS) {
		return ino->direct[block_offset];
	}

	/* In indirect block */
	if (block_offset < INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS) {
		offset = block_offset - INODE_DIRECT_BLOCKS;
		return indirect(disk, ino->indirect, offset);
	}

	/* In double indirect block */
	if (block_offset < INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) {
		offset = block_offset - INODE_DIRECT_BLOCKS - INODE_INDIRECT_BLOCKS;
		return double_indirect(disk, ino->double_indirect, offset);
	}

	/* In triple indirect block */
	offset = block_offset - INODE_DIRECT_BLOCKS - INODE_INDIRECT_BLOCKS - INODE_DOUBLE_BLOCKS;
	return triple_indirect(disk, ino->triple_indirect, offset);
}

/*
 * Byte offset to block number
 * Return -1 on error
 */
blocknum_t
bytemap(disk_t* disk, mem_inode_t ino, size_t byte_offset) {
	return blockmap(disk, ino, byte_offset / DISK_BLOCK_SIZE);
}

/* Find file name from path */
char* pathtofile(char* path) {
	char* pch;
	char* filename = NULL;
	
	pch = strtok(path, "/");
	while (pch != NULL) {
		filename = realloc(filename, strlen(pch) + 1);
		strcpy(filename, pch);
		pch = strtok(NULL, "/");
	}
	
	return filename;
}


static blocknum_t
indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset) {
	buf_block_t buf;
	size_t offset_to_read;
	blocknum_t ret_blocknum;

	if (bread(disk, blocknum, &buf) != 0) {
		return -1;
	}
	offset_to_read = block_offset % POINTER_PER_BLOCK;
	ret_blocknum = (blocknum_t)(buf->data + offset_to_read * 8);
	brelse(buf);

	return ret_blocknum;
}

static blocknum_t
double_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset) {
	buf_block_t buf;
	size_t offset_to_read;
	blocknum_t block_to_read;

	if (bread(disk, blocknum, &buf) != 0) {
		return -1;
	}
	offset_to_read = block_offset / POINTER_PER_BLOCK;
	block_to_read = (blocknum_t)(buf->data + offset_to_read * 8);
	brelse(buf);

	return indirect(disk, block_to_read, block_offset - offset_to_read * POINTER_PER_BLOCK);
}

static blocknum_t
triple_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset) {
	buf_block_t buf;
	size_t offset_to_read;
	blocknum_t block_to_read;

	if (bread(disk, blocknum, &buf) != 0) {
		return -1;
	}
	offset_to_read = block_offset / POINTER_PER_BLOCK / POINTER_PER_BLOCK;
	block_to_read = (blocknum_t)(buf->data + offset_to_read * 8);
	brelse(buf);

	return double_indirect(disk, block_to_read, block_offset - offset_to_read * POINTER_PER_BLOCK * POINTER_PER_BLOCK);
}