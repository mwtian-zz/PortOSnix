#include "minifile_inode.h"
#include <string.h>
#include "minifile_fs.h"
#include "minifile_inodetable.h"
#include "minifile_path.h"

/* Translate inode number to block number. Inode starts from 1 which is root directory */
#define INODE_TO_BLOCK(num) ((((num) - 1) / INODE_PER_BLOCK) + INODE_START_BLOCK)
/* Inode offset within a data block */
#define INODE_OFFSET(num) ((((num) - 1) % INODE_PER_BLOCK) * INODE_SIZE)

#define POINTER_PER_BLOCK 512
#define INODE_INDIRECT_BLOCKS (512)
#define INODE_DOUBLE_BLOCKS (512 * 512)
#define INODE_TRIPLE_BLOCKS (512 * 512 * 512)
#define INODE_MAX_FILE_BLOCKS (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS + INODE_TRIPLE_BLOCKS)


/* Indirect block management */
static blocknum_t indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);
static blocknum_t double_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);
static blocknum_t triple_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);

/* Clear the content of an inode, including indirect blocks */
int
iclear(disk_t* disk, inodenum_t n)
{
	return 0;
}

/* Get the content of the inode with inode number n. Return 0 if success, -1 if not */
int iget(disk_t* disk, inodenum_t n, mem_inode_t *inop)
{
	buf_block_t buf;
	blocknum_t block_to_read;

	semaphore_P(inode_lock);
	/* First find inode from table */
	if (itable_get_from_table(n, inop) == 0) {
		(*inop)->ref_count++;
		semaphore_V(inode_lock);
		return 0;
	}

	/* No free inode */
	if (itable_get_free_inode(inop) != 0) {
		return -1;
	}

	/* Read from disk */
	block_to_read = INODE_TO_BLOCK(n);
	if (bread(disk, block_to_read, &buf) != 0) {
		itable_put_list(*inop);
		return -1;
	}

	memcpy((*inop), buf->data + INODE_OFFSET(n), sizeof(struct inode));
	(*inop)->disk = disk;
	(*inop)->num = n;
	(*inop)->buf = buf;
	if ((*inop)->size == 0) {
		(*inop)->size_blocks = 0;
	} else {
		if ((*inop)->type == MINIDIRECTORY) {
			(*inop)->size_blocks = ((*inop)->size - 1) / ENTRY_NUM_PER_BLOCK + 1;
		} else {
			(*inop)->size_blocks = ((*inop)->size - 1) / DISK_BLOCK_SIZE + 1;
		}
	}
	(*inop)->ref_count = 1;
	(*inop)->inode_lock = semaphore_create();
	/* Fail to create lock */
	if ((*inop)->inode_lock == NULL) {
		brelse((*inop)->buf);     /* Release buffer */
		itable_put_list((*inop)); /* Put inode back to free list */
		semaphore_V(inode_lock);  /* Release lock */
		return -1;
	}
	semaphore_initialize((*inop)->inode_lock, 1);

	/* Put to new queue */
	itable_put_table(*inop);
	semaphore_V(inode_lock);

    return 0;
}

/* Return the inode and no write to disk */
void iput(mem_inode_t ino)
{
	semaphore_P(inode_lock);
	ino->ref_count--;
	if (ino->ref_count == 0) {
		/* Delete this file */
		if (ino->status == TO_DELETE) {
			/* Should free disk blocks as well */
			ifree(ino);
			/* Put inode back to free list, delete from table */
			itable_delete_from_table(ino);
			itable_put_list(ino);
		} else {
			iupdate(ino);
			/* Put inode back to free list */
			itable_put_list(ino);
		}
		/* Relase buffer */
		brelse(ino->buf);
	}
	semaphore_V(inode_lock);
}

/* Return the inode and update it on the disk */
int iupdate(mem_inode_t ino)
{
    memcpy(ino->buf->data + INODE_OFFSET(ino->num), ino, sizeof(struct inode));
    bwrite(ino->buf);
	return 0;
}


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
