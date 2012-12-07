#include <string.h>
#include "minifile_fs.h"
#include "minifile_inode.h"

#include "minifile_inodetable.h"
#include "minifile_path.h"
#include "minifile_util.h"


static int free_single_indirect(blocknum_t blocknum); /* Free single indirect block */
static int free_double_indirect(blocknum_t blocknum, blocknum_t block_lim); /* Free double indirect */
static int free_triple_indirect(blocknum_t blocknum, blocknum_t block_lim); /* Free triple indirect */
static int free_all_indirect(mem_inode_t ino, blocknum_t blocknum); /* Free all indirect block */
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
        bfree(blocknum);
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
	brelse(buf);

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
			ifree(ino->num);
		} else {
			semaphore_P(ino->inode_lock); /* Other process couldn't be using this inode, otherwise count won't be 0 */
			iupdate(ino);
			semaphore_V(ino->inode_lock);
		}
		/* Put inode back to free list, delete from table */
		itable_delete_from_table(ino);
		itable_put_list(ino);
	}
	semaphore_V(inode_lock);
}

/* Return the inode and update it on the disk */
int
iupdate(mem_inode_t ino)
{
    printf("iupdate starts\n");
    bread(maindisk, ino->buf->num, &(ino->buf));
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
		bfree(blocknum);
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

int
idelete_from_dir(mem_inode_t ino, inodenum_t inodenum) {
	size_t block_num;             /* Number of data blocks in an inode */
	size_t entry_num;             /* Number of entries in a directory inode */
	size_t existing_entry;        /* Number of existing entries in a data block */
	size_t left_entry;            /* Number of entries left to exam */
	blocknum_t blocknum;          /* Data block number to read */
	buf_block_t buf, last_buf;    /* Block buffer */
	dir_entry_t src_entry, target_entry;
	int i, j, offset;
	char is_found = 0;

	entry_num = ino->size;
	block_num = ino->size_blocks;
	if (entry_num <= 0) {
		return -1;
	}

	for (i = 0; i < block_num; i++) {
		left_entry = entry_num - i * ENTRY_NUM_PER_BLOCK;
		existing_entry = (left_entry > ENTRY_NUM_PER_BLOCK ? ENTRY_NUM_PER_BLOCK : left_entry);
		blocknum = blockmap(maindisk, ino, i);
		if (blocknum == -1) {
			continue;
		}
		if (bread(ino->disk, blocknum, &buf) != 0) {
			continue;
		}
		target_entry = (dir_entry_t)buf->data;
		for (j = 0; j < existing_entry; j++) {
			if (target_entry->inode_num == inodenum) {
				is_found = 1;
				break;
			}
		}
		if (is_found == 1) {
			break;
		} else {
			brelse(buf);
		}
	}
	if (is_found == 0) {
		return -1;
	}
	/* Only one entry in this directory, remove last data block */
	if (ino->size == 1) {
		brelse(buf);
		irm_block(ino);
		ino->size_blocks = 0;
		return 0;
	}
	/* Copy last entry to this one */
	/* Get last block */
	offset = (ino->size - 1) % ENTRY_NUM_PER_BLOCK;
	blocknum = blockmap(ino->disk, ino, block_num - 1);
	if (blocknum == -1) {
		brelse(buf);
		return -1;
	}
	/* Is buf */
	if (blocknum == buf->num) {
		src_entry = (dir_entry_t)buf->data;
		src_entry += offset;
		memcpy(target_entry, src_entry, sizeof(struct dir_entry));
		bwrite(buf);
		return 0;
	}
	if (bread(ino->disk, blocknum, &last_buf) != 0) {
		brelse(buf);
		return -1;
	}
	src_entry = (dir_entry_t)last_buf->data;
	src_entry += offset;
	memcpy(target_entry, src_entry, sizeof(struct dir_entry));
	bwrite(buf);
	brelse(last_buf);
	/* Only 1 entry in last block, so remove the block */
	if (offset == 0) {
		irm_block(ino);
	}
	return 0;
}

/* Remove single indirect */
static int
rm_single_indirect(mem_inode_t ino, int blocksize) {
	int offset;
	blocknum_t blocknum;
	buf_block_t s_buf;

	offset = single_offset(blocksize - INODE_DIRECT_BLOCKS - 1);
	if (bread(ino->disk, ino->indirect, &s_buf) != 0) {
		return -1;
	}
	blocknum = (blocknum_t) (s_buf->data + 8 * offset);

	if (offset == 0) {
		bfree(s_buf->num);
	}
	brelse(s_buf);
	bfree(blocknum);
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
		bfree(s_buf->num);
		if (doffset == 0) {
			bfree(d_buf->num);
		}
	}
    brelse(d_buf);
    brelse(s_buf);

	bfree(buf->num);
	brelse(buf);
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
		bfree(s_buf->num);
		if (doffset == 0) {
			bfree(d_buf->num);
			if (toffset == 0) {
				bfree(t_buf->num);
			}
		}
	}
    brelse(s_buf);
    brelse(d_buf);
    brelse(t_buf);

	bfree(buf->num);
	brelse(buf);

	return 0;
}

/* Free all indirect blocks */
static int
free_all_indirect(mem_inode_t ino, blocknum_t datablock_num)
{
	buf_block_t buf;
	blocknum_t relse_blocknum;
	blocknum_t blocknum;

	if (datablock_num > INODE_DIRECT_BLOCKS) {
		blocknum = ino->indirect;
        free_single_indirect(blocknum);
	}
	if (datablock_num > (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		relse_blocknum = datablock_num - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS);
		relse_blocknum = (relse_blocknum > (INODE_DOUBLE_BLOCKS)) ? INODE_DOUBLE_BLOCKS : relse_blocknum;
		blocknum = ino->double_indirect;
        //free_double_indirect(blocknum, relse_blocknum);
	}
	if (datablock_num > (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS)) {
		relse_blocknum = datablock_num - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS);
		blocknum = ino->triple_indirect;
        free_triple_indirect(blocknum, relse_blocknum);
	}
	return 0;
}

/* Free single indirect block */
static int
free_single_indirect(blocknum_t blocknum)
{
	bfree(blocknum);
	return 0;
}

/* Free double indirect */
static int
free_double_indirect(blocknum_t blocknum, blocknum_t blockleft)
{
	blocknum_t bnum;
	buf_block_t buf;
	int left_blocknum, relse_blocknum, i;

    bread(maindisk, blocknum, &buf);
	left_blocknum = blockleft;
	i = 0;
	while (left_blocknum > 0) {
		relse_blocknum = ((left_blocknum > INODE_INDIRECT_BLOCKS) ? INODE_INDIRECT_BLOCKS : left_blocknum);
		left_blocknum -= relse_blocknum;
		bnum = (blocknum_t)(buf->data + 8 * i);
        free_single_indirect(bnum);
		i++;
	}
	brelse(buf);
	free_single_indirect(blocknum);
	return 0;
}

/* Free triple indirect */
static int
free_triple_indirect(blocknum_t blocknum, blocknum_t blockleft) {
	blocknum_t bnum;
	buf_block_t buf;
	blocknum_t left_blocknum, relse_blocknum, i;

    bread(maindisk, blocknum, &buf);
	left_blocknum = blockleft;
	i = 0;
	while (left_blocknum > 0) {
		relse_blocknum = ((left_blocknum > INODE_DOUBLE_BLOCKS) ? INODE_DOUBLE_BLOCKS : left_blocknum);
		left_blocknum -= relse_blocknum;
		bnum = (blocknum_t)(buf->data + 8 * i);
        free_double_indirect(bnum, relse_blocknum);
		i++;
	}
	brelse(buf);

	free_single_indirect(blocknum);
	return 0;
}

static int
add_single_indirect(mem_inode_t ino, buf_block_t buf, int cur_blocknum) {
	buf_block_t new_buf;
	int soffset;
	blocknum_t blocknum;

	/* Need a indirect block */
	if (cur_blocknum == INODE_DIRECT_BLOCKS) {
		blocknum = balloc(ino->disk);
		bread(ino->disk, blocknum, &new_buf);
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
	blocknum_t blocknum_1, blocknum_2;
	int soffset, doffset;

	/* Need double indirect block */
	if (cur_blocknum == (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		blocknum_1 = balloc(ino->disk);
		bread(maindisk, blocknum_1, &new_buf);
		if (new_buf == NULL) {
			return -1;
		}
		memcpy(new_buf->data, (void*)&blocknum_1, sizeof(blocknum_t));

		blocknum_2 = balloc(ino->disk);
		bread(maindisk, blocknum_2, &sec_buf);
		if (sec_buf == NULL) {
			return -1;
		}
		memcpy(sec_buf->data, (void*)&blocknum_2, sizeof(blocknum_t));
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
			blocknum_1 = balloc(ino->disk);
            bread(maindisk, blocknum_1, &sec_buf);
			if (sec_buf == NULL) {
				return -1;
			}
			memcpy(sec_buf->data, (void*)&blocknum_1, sizeof(blocknum_t));
			memcpy((new_buf->data + 8 * doffset), (void*)&blocknum_1, sizeof(blocknum_t));
			bwrite(sec_buf);
			bwrite(new_buf);
			bwrite(buf);
			return 0;
		} else {
			blocknum_1 = (blocknum_t)(new_buf->data + 8 * doffset);
			if (bread(ino->disk, blocknum_1, &sec_buf) != 0) {
				brelse(new_buf);
				return -1;
			}
			blocknum_1 = buf->num;
			memcpy((sec_buf->data + 8 * soffset), (void*)&blocknum_1, sizeof(blocknum_t));
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
		blocknum = balloc(ino->disk);
		bread(maindisk, blocknum, &new_buf);
		if (new_buf == NULL) {
			return -1;
		}
		memcpy(new_buf->data, (void*)&blocknum, sizeof(blocknum_t));

		blocknum = balloc(ino->disk);
		bread(maindisk, blocknum, &sec_buf);
		if (sec_buf == NULL) {
			return -1;
		}
		memcpy(sec_buf->data, (void*)&blocknum, sizeof(blocknum_t));

		blocknum = balloc(ino->disk);
		bread(maindisk, blocknum, &thr_buf);
		if (thr_buf == NULL) {
			return -1;
		}
		memcpy(thr_buf->data, (void*)&blocknum, sizeof(blocknum_t));

		ino->triple_indirect = blocknum;
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
		    blocknum = balloc(ino->disk);
		    bread(maindisk, blocknum, &sec_buf);
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
		    blocknum = balloc(ino->disk);
		    bread(maindisk,  blocknum, &new_buf);
			if (new_buf == NULL) {
				if (doffset == 0) {
					bfree(blocknum);
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
					bfree(blocknum);
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
