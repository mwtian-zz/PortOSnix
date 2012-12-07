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
static int add_single_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum);
static int add_double_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum);
static int add_triple_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum);
static int rm_single_indirect(mem_inode_t ino, int blocksize);
static int rm_double_indirect(mem_inode_t ino, int blocksize);
static int rm_triple_indirect(mem_inode_t ino, int blocksize);
static int single_offset(blocknum_t blocknum);
static int double_offset(blocknum_t blocknum);
static int triple_offset(blocknum_t blocknum);

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

	for (i = 0; i < datablock_num; i++) {
		blocknum = blockmap(ino->disk, ino, i);    /* A little inefficient to do this... */
		if (blocknum == -1) {
			printf("Error on mapping block number!\n");
			/* What to do here? */
		}
		if (bread(ino->disk, blocknum, &buf) == 0) {
			bfree(buf);
		}
	}
	free_all_indirect(ino, datablock_num);

	return 0;
}

/* Get the content of the inode with inode number n. Return 0 if success, -1 if not */
int
iget(disk_t* disk, inodenum_t n, mem_inode_t *inop)
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
	    semaphore_V(inode_lock);
		return -1;
	}

	/* Read from disk */
	block_to_read = INODE_TO_BLOCK(n);
	if (bread(disk, block_to_read, &buf) != 0) {
		itable_put_list(*inop);
		semaphore_V(inode_lock);
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
void
iput(mem_inode_t ino)
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
int
iupdate(mem_inode_t ino)
{
    printf("iupdate starts\n");

    memcpy(ino->buf->data + INODE_OFFSET(ino->num), ino, sizeof(struct inode));
    return bwrite(ino->buf);
}

/* Add a block to inode in memory */
int
iadd_block(mem_inode_t ino, buf_block_t buf) {
	int cur_blocknum;

	cur_blocknum = ino->size_blocks;
	/* Still have direct block */
	if (cur_blocknum < INODE_DIRECT_BLOCKS) {
		ino->direct[cur_blocknum] = buf->num;
		ino->size_blocks++;
		bwrite(buf); /* Could be brelse */
		return 0;
	}
	/* Fit in indirect block */
	if (cur_blocknum < (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		return add_single_indirect(ino, buf, cur_blocknum);
	}
	/* Fit in double indirect block */
	if (cur_blocknum < (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS)) {
		return add_double_indirect(ino, buf, cur_blocknum);
	}
	if (cur_blocknum < INODE_MAX_FILE_BLOCKS) {
		return add_triple_indirect(ino, buf, cur_blocknum);
	}
	return -1;
}

/* Remove the last block of an inode, if any */
int 
irm_block(mem_inode_t ino) {
	int blocksize;
	blocknum_t blocknum;
	buf_block_t buf;
	
	if (ino->size <= 0 || ino->size_blocks <= 0) {
		return 0;
	}
	blocksize = ino->size_blocks;
	/* In direct block */
	if (blocksize <= INODE_DIRECT_BLOCKS) {
		blocknum = ino->direct[blocksize - 1];
		if (bread(ino->disk, blocknum, &buf) != 0) {
			return -1;
		}
		bfree(buf);
		return 0;
	}
	/* In indirect blocks */
	if (blocksize <= (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		return rm_single_indirect(ino, blocksize);
	}
	if (blocksize <= (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS)) {
		return rm_double_indirect(ino, blocksize);
	}
	if (blocksize <= (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS + INODE_TRIPLE_BLOCKS)) {
		return rm_triple_indirect(ino, blocksize);
	}
	return -1;
}

/* Remove single indirect */ 
static int
rm_single_indirect(mem_inode_t ino, int blocksize) {
	int offset;
	blocknum_t blocknum;
	buf_block_t s_buf, buf;
	
	offset = single_offset(blocksize - INODE_DIRECT_BLOCKS - 1);
	if (bread(ino->disk, ino->indirect, &s_buf) != 0) {
		return -1;
	}
	blocknum = (blocknum_t)(s_buf->data + 8 * offset);
	if (bread(ino->disk, blocknum, &buf) != 0) {
		brelse(s_buf);
		return -1;
	}
	if (offset == 0) {
		bfree(s_buf);
	} else {
		brelse(s_buf);
	}
	bfree(buf);
	return 0;
}

static int
rm_double_indirect(mem_inode_t ino, int blocksize) {
	int soffset, doffset;
	blocknum_t blocknum;
	buf_block_t d_buf, s_buf, buf;
	
	doffset = double_offset(blocksize - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS) - 1);
	soffset = single_offset(blocksize - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS) - 1 - doffset * INODE_INDIRECT_BLOCKS);
	
	if (bread(ino->disk, ino->double_indirect, &d_buf) != 0) {
		return -1;
	}
	blocknum = (blocknum_t)(d_buf->data + 8 * doffset);
	if (bread(ino->disk, blocknum, &s_buf) != 0) {
		brelse(d_buf);
		return -1;
	}
	blocknum = (blocknum_t)(s_buf->data + 8 * soffset);
	if (bread(ino->disk, blocknum, &buf) != 0) {
		brelse(d_buf);
		brelse(s_buf);
		return -1;
	}
	if (soffset == 0) {
		bfree(s_buf);
		if (doffset == 0) {
			bfree(d_buf);
		} else {
			brelse(d_buf);
		}
	} else {
		brelse(d_buf);
		brelse(s_buf);
	}
	bfree(buf);
	return 0;
}

static int
rm_triple_indirect(mem_inode_t ino, int blocksize) {
	int soffset, doffset, toffset;
	blocknum_t blocknum;
	buf_block_t s_buf, d_buf, t_buf, buf;

	toffset = triple_offset(blocksize - 1 - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS));
	doffset = double_offset(blocksize - 1 - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) - INODE_DOUBLE_BLOCKS * toffset);
	soffset = single_offset(blocksize - 1 - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) - INODE_DOUBLE_BLOCKS * toffset - INODE_INDIRECT_BLOCKS * doffset);
	
	if (bread(ino->disk, ino->triple_indirect, &t_buf) != 0) {
		return -1;
	}
	blocknum = (blocknum_t)(t_buf->data + 8 * toffset);
	if (bread(ino->disk, blocknum, &d_buf) != 0) {
		brelse(t_buf);
		return -1;
	}
	blocknum = (blocknum_t)(d_buf->data + 8 * doffset);
	if (bread(ino->disk, blocknum, &s_buf) != 0) {
		brelse(t_buf);
		brelse(d_buf);
		return -1;
	}
	blocknum = (blocknum_t)(s_buf->data + 8 * soffset);
	if (bread(ino->disk, blocknum, &buf) != 0) {
		brelse(t_buf);
		brelse(d_buf);
		brelse(s_buf);
		return -1;
	}
	if (soffset == 0) {
		bfree(s_buf);
		if (doffset == 0) {
			bfree(d_buf);
			if (toffset == 0) {
				bfree(t_buf);
			} else {
				brelse(t_buf);
			}
		} else {
			brelse(d_buf);
			brelse(t_buf);
		}
	} else {
		brelse(s_buf);
		brelse(d_buf);
		brelse(t_buf);
	}
	bfree(buf);
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

static int
add_single_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum) {
	buf_block_t new_buf;
	int soffset;
	blocknum_t blocknum;

	/* Need a indirect block */
	if (cur_blocknum == INODE_DIRECT_BLOCKS) {
		new_buf = balloc(ino->disk);
		if (new_buf == NULL) {
			return -1;
		}
	} else {
		if (bread(ino->disk, ino->indirect, &new_buf) != 0) {
			return -1;
		}
	}
	blocknum = buf->num;
	soffset = single_offset(cur_blocknum - INODE_DIRECT_BLOCKS);
	memcpy((new_buf->data + 8 * soffset), (void*)&blocknum, sizeof(blocknum_t));
	ino->indirect = new_buf->num;
	bwrite(new_buf);
	bwrite(buf);
	return 0;
}


static int
add_double_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum) {
	buf_block_t new_buf, sec_buf;
	blocknum_t blocknum;
	int soffset, doffset;

	/* Need double indirect block */
	if (cur_blocknum == (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		new_buf = balloc(ino->disk);
		if (new_buf == NULL) {
			return -1;
		}
		blocknum = buf->num;
		memcpy(new_buf->data, (void*)&blocknum, sizeof(blocknum_t));
		blocknum = new_buf->num;
		sec_buf = balloc(ino->disk);
		if (sec_buf == NULL) {
			bfree(new_buf);
			return -1;
		}
		memcpy(sec_buf->data, (void*)&blocknum, sizeof(blocknum_t));
		ino->double_indirect = sec_buf->num;
		bwrite(new_buf);
		bwrite(sec_buf);
		bwrite(buf);
		return 0;
	} else {
		if (bread(ino->disk, ino->double_indirect, &new_buf) != 0) {
			return -1;
		}
		doffset = double_offset(cur_blocknum - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS));
		soffset = single_offset(cur_blocknum - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS) - doffset * INODE_INDIRECT_BLOCKS);
		/* Need a new second level indirect block */
		if (soffset == 0) {
			sec_buf = balloc(ino->disk);
			if (sec_buf == NULL) {
				brelse(new_buf);
				return -1;
			}
			blocknum = buf->num;
			memcpy(sec_buf->data, (void*)&blocknum, sizeof(blocknum_t));
			blocknum = sec_buf->num;
			memcpy((new_buf->data + 8 * doffset), (void*)&blocknum, sizeof(blocknum_t));
			bwrite(sec_buf);
			bwrite(new_buf);
			bwrite(buf);
			return 0;
		} else {
			blocknum = (blocknum_t)(new_buf->data + 8 * doffset);
			if (bread(ino->disk, blocknum, &sec_buf) != 0) {
				brelse(new_buf);
				return -1;
			}
			blocknum = buf->num;
			memcpy((sec_buf->data + 8 * soffset), (void*)&blocknum, sizeof(blocknum_t));
			bwrite(sec_buf);
			bwrite(buf);
			brelse(new_buf);
		}
	}
	return 0;
}
static int
add_triple_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum) {
	buf_block_t new_buf, sec_buf, thr_buf;
	blocknum_t blocknum;
	int soffset, doffset, toffset;

	/* Need to new triple indirect block */
	if (cur_blocknum == (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS)) {
		new_buf = balloc(ino->disk);
		if (new_buf == NULL) {
			return -1;
		}
		blocknum = buf->num;
		memcpy(new_buf->data, (void*)&blocknum, sizeof(blocknum_t));
		sec_buf = balloc(ino->disk);
		if (new_buf == NULL) {
			bfree(new_buf);
			return -1;
		}
		blocknum = new_buf->num;
		memcpy(sec_buf->data, (void*)&blocknum, sizeof(blocknum_t));
		thr_buf = balloc(ino->disk);
		if (thr_buf == NULL) {
			bfree(sec_buf);
			bfree(new_buf);
			return -1;
		}
		blocknum = sec_buf->num;
		memcpy(thr_buf->data, (void*)&blocknum, sizeof(blocknum_t));
		ino->triple_indirect = thr_buf->num;
		bwrite(new_buf);
		bwrite(sec_buf);
		bwrite(thr_buf);
		bwrite(buf);
		return 0;
	} else {
		toffset = triple_offset(cur_blocknum - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS));
		doffset = double_offset(cur_blocknum - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) - INODE_DOUBLE_BLOCKS * toffset);
		soffset = single_offset(cur_blocknum - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) - INODE_DOUBLE_BLOCKS * toffset - INODE_INDIRECT_BLOCKS * doffset);
		if (bread(ino->disk, ino->triple_indirect, &thr_buf) != 0) {
			return -1;
		}
		if (doffset == 0) {
			sec_buf = balloc(ino->disk);
			if (sec_buf == NULL) {
				brelse(thr_buf);
				return -1;
			}
		} else {
			blocknum = (blocknum_t)(thr_buf->data + 8 * toffset);
			if (bread(ino->disk, blocknum, &sec_buf) != 0) {
				brelse(thr_buf);
				return -1;
			}
		}
		if (soffset == 0) {
			new_buf = balloc(ino->disk);
			if (new_buf == NULL) {
				if (doffset == 0) {
					bfree(sec_buf);
				} else {
					brelse(sec_buf);
				}
				brelse(thr_buf);
				return -1;
			}
		} else {
			blocknum = (blocknum_t)(sec_buf->data + 8 * doffset);
			if (bread(ino->disk, blocknum, &new_buf) != 0) {
				if (doffset == 0) {
					bfree(sec_buf);
				} else {
					brelse(sec_buf);
				}
				brelse(thr_buf);
				return -1;
			}
		}
		blocknum = buf->num;
		memcpy(new_buf->data + 8 * soffset, (void*)&blocknum, sizeof(blocknum_t));
		blocknum = new_buf->num;
		memcpy(sec_buf->data + 8 * doffset, (void*)&blocknum, sizeof(blocknum_t));
		blocknum = sec_buf->num;
		memcpy(thr_buf->data + 8 * toffset, (void*)&blocknum, sizeof(blocknum_t));
		bwrite(new_buf);
		bwrite(sec_buf);
		bwrite(thr_buf);
		bwrite(buf);
		return 0;
	}
}

static int
single_offset(blocknum_t blocknum) {
	return blocknum % INODE_INDIRECT_BLOCKS;
}

static int
double_offset(blocknum_t blocknum) {
	return blocknum / INODE_INDIRECT_BLOCKS;
}

static int triple_offset(blocknum_t blocknum) {
	return blocknum / INODE_DOUBLE_BLOCKS;
}
