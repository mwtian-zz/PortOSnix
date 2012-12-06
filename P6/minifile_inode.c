#include <string.h>
#include "minifile_fs.h"
#include "minifile_inode.h"

#include "minifile_inodetable.h"
#include "minifile_path.h"
#include "minifile_util.h"


static int free_single_indirect(buf_block_t buf); /* Free single indirect block */
static int free_double_indirect(buf_block_t buf, int blocknum); /* Free double indirect */
static int free_triple_indirect(buf_block_t buf, int blocknum); /* Free triple indirect */
static int free_all_indirect(mem_inode_t ino, int blocknum); /* Free all indirect block */
/*
 * Clear the content of an inode, including indirect blocks
 * iclear is probably only called in iput which means inode lock is grabbed
 * and no other process is changing inode
 */
int
iclear(mem_inode_t ino)
{
	int datablock_num, i, blocknum;
	buf_block_t buf;

	if (ino->type == MINIDIRECTORY) {
		if (ino->size <= 0) {
			return 0;
		} else {
			datablock_num = (ino->size - 1) / ENTRY_NUM_PER_BLOCK + 1;
		}
	} else if (ino->type == MINIFILE) {
		if (ino->size <= 0) {
			return 0;
		} else {
			datablock_num = (ino->size - 1) / DISK_BLOCK_SIZE + 1;
		}
	} else {
		return 0;
	}

	semaphore_P(sb_lock);
	for (i = 0; i < datablock_num; i++) {
		blocknum = blockmap(ino->disk, ino, i);
		if (blocknum == -1) {
			printf("Error on mapping block number!\n");
			/* What to do here? */
		}
		if (bread(ino->disk, blocknum, &buf) == 0) {
			bfree(buf);
		}
	}
	semaphore_V(sb_lock);
	free_all_indirect(ino, datablock_num);
	
	return 0;
}

/* Free all indirect blocks */
static int 
free_all_indirect(mem_inode_t ino, int datablock_num) {
	buf_block_t buf;
	int relse_blocknum;
	blocknum_t blocknum;
	
	if (datablock_num > INODE_DIRECT_BLOCKS) {
		blocknum = ino->indirect;
		if (bread(ino->disk, blocknum, &buf) == 0) {
			free_single_indirect(buf);
		}
	}
	if (datablock_num > (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		relse_blocknum = datablock_num - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS);
		relse_blocknum = (relse_blocknum > (INODE_DOUBLE_BLOCKS)) ? INODE_DOUBLE_BLOCKS : relse_blocknum;
		blocknum = ino->double_indirect;
		if (bread(ino->disk, blocknum, &buf) == 0) {
			free_double_indirect(buf, relse_blocknum);
		}
	}
	if (datablock_num > (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS)) {
		relse_blocknum = datablock_num - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS);
		blocknum = ino->triple_indirect;
		if (bread(ino->disk, blocknum, &buf) == 0) {
			free_triple_indirect(buf, relse_blocknum);
		}
	}
	return 0;
}

/* Free single indirect block */
static int 
free_single_indirect(buf_block_t buf) {
	bfree(buf);
	return 0;
}

/* Free double indirect */
static int 
free_double_indirect(buf_block_t buf, int blocknum) {
	blocknum_t bnum;
	buf_block_t buf_to_relse;
	int left_blocknum, relse_blocknum, i;
	
	left_blocknum = blocknum;
	i = 0;
	while (left_blocknum > 0) {
		relse_blocknum = ((left_blocknum > INODE_INDIRECT_BLOCKS) ? INODE_INDIRECT_BLOCKS : left_blocknum);
		left_blocknum -= relse_blocknum;
		bnum = (blocknum_t)(buf->data + 8 * i);
		if (bread(maindisk, bnum, &buf_to_relse) == 0) {
			free_single_indirect(buf_to_relse);
		}
		i++;
	}
	free_single_indirect(buf);
	return 0;
}

/* Free triple indirect */
static int 
free_triple_indirect(buf_block_t buf, int blocknum) {
	blocknum_t bnum;
	buf_block_t buf_to_relse;
	int left_blocknum, relse_blocknum, i;
	
	left_blocknum = blocknum;
	i = 0;
	while (left_blocknum > 0) {
		relse_blocknum = ((left_blocknum > INODE_DOUBLE_BLOCKS) ? INODE_DOUBLE_BLOCKS : left_blocknum);
		left_blocknum -= relse_blocknum;
		bnum = (blocknum_t)(buf->data + 8 * i);
		if (bread(maindisk, bnum, & buf_to_relse) == 0) {
			free_double_indirect(buf_to_relse, relse_blocknum);
		}
		i++;
	}
	free_single_indirect(buf);
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
	if ((*inop)->size <= 0) {
		(*inop)->size_blocks = 0;
	} else {
		if ((*inop)->type == MINIDIRECTORY) {
			(*inop)->size_blocks = ((*inop)->size - 1) / ENTRY_NUM_PER_BLOCK + 1;
		} else {
			(*inop)->size_blocks = ((*inop)->size - 1) / DISK_BLOCK_SIZE + 1;
		}
	}
	(*inop)->ref_count = 1;

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
			iclear(ino);
			ifree(ino);
		}
		semaphore_P(ino->inode_lock);
		iupdate(ino);
		semaphore_V(ino->inode_lock);
		/* Put inode back to free list, delete from table */
		itable_delete_from_table(ino);
		itable_put_list(ino);
		/* Relase buffer */
		brelse(ino->buf);
	}
	semaphore_V(inode_lock);
}

/* Return the inode and update it on the disk */
int iupdate(mem_inode_t ino)
{
    memcpy(ino->buf->data + INODE_OFFSET(ino->num), ino, sizeof(struct inode));
    return blocking_write(ino->buf);
}
