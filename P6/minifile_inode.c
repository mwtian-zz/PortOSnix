#include <string.h>
#include "minifile_fs.h"
#include "minifile_inode.h"

#include "minifile_inodetable.h"
#include "minifile_path.h"

/* Translate inode number to block number. Inode starts from 1 which is root directory */
#define INODE_TO_BLOCK(num) (((num) / INODE_PER_BLOCK) + INODE_START_BLOCK)
/* Inode offset within a data block */
#define INODE_OFFSET(num) (((num) % INODE_PER_BLOCK) * INODE_SIZE)

#define POINTER_PER_BLOCK 512
#define INODE_INDIRECT_BLOCKS (512)
#define INODE_DOUBLE_BLOCKS (512 * 512)
#define INODE_TRIPLE_BLOCKS (512 * 512 * 512)
#define INODE_MAX_FILE_BLOCKS (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS + INODE_TRIPLE_BLOCKS)

static int free_single_indirect(blocknum_t blocknum); /* Free single indirect block */
static int free_double_indirect(blocknum_t blocknum, blocknum_t block_lim); /* Free double indirect */
static int free_triple_indirect(blocknum_t blocknum, blocknum_t block_lim); /* Free triple indirect */
static int free_all_indirect(mem_inode_t ino, blocknum_t blocknum); /* Free all indirect block */
static int add_single_indirect(mem_inode_t ino, blocknum_t blocknum_to_add, int cur_blocknum);
static int add_double_indirect(mem_inode_t ino, blocknum_t blocknum_to_add, int cur_blocknum);
static int add_triple_indirect(mem_inode_t ino, blocknum_t blocknum_to_add, int cur_blocknum);
static int rm_single_indirect(mem_inode_t ino, int blocksize);
static int rm_double_indirect(mem_inode_t ino, int blocksize);
static int rm_triple_indirect(mem_inode_t ino, int blocksize);
static int single_offset(blocknum_t blocknum);
static int double_offset(blocknum_t blocknum);
static int triple_offset(blocknum_t blocknum);
static blocknum_t indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);
static blocknum_t double_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);
static blocknum_t triple_indirect(disk_t* disk, blocknum_t blocknum, size_t block_offset);

/* Lock and unlock a single inode */
void
ilock(mem_inode_t ino)
{
    semaphore_P(ino->inode_lock);
}

void
iunlock(mem_inode_t ino)
{
    semaphore_V(ino->inode_lock);
}

/*
 * Clear the content of an inode, including indirect blocks
 * iclear is probably only called in iput which means inode lock is grabbed
 * and no other process is changing inode
 */
int
iclear(mem_inode_t ino)
{
	int datablock_num, i, blocknum;

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

    izero(ino);
    iupdate(ino);

	return 0;
}

void
izero(mem_inode_t ino)
{
    int i;
    ino->size = 0;
	for (i = 0; i < 11; i++) {
		ino->direct[i] = 0;
	}
    ino->indirect = 0;
    ino->double_indirect = 0;
    ino->triple_indirect = 0;
}

void
iinit(mem_inode_t ino)
{

}

/* Get the content of the inode with inode number n. Return 0 if success, -1 if not */
int
iget(disk_t* disk, inodenum_t n, mem_inode_t *inop)
{
	buf_block_t buf;
	blocknum_t block_to_read;
	int sig = 0;

	semaphore_P(itable_lock);

	/* First find inode from table */
	if (itable_get_from_table(n, inop) == 0) {
		(*inop)->ref_count++;

		goto ret;
	}

	/* No free inode */
	if (itable_get_free_inode(inop) != 0) {
	    sig = -1;
	    goto ret;
	}

	/* Read from disk */
	block_to_read = INODE_TO_BLOCK(n);
	if (bread(disk, block_to_read, &buf) != 0) {
		itable_put_list(*inop);
		sig = -1;
		goto ret;
	}

	memcpy((*inop), buf->data + INODE_OFFSET(n), sizeof(struct inode));

	(*inop)->inode_lock = semaphore_new(1);
	if (NULL == (*inop)->inode_lock) {
        brelse(buf);
        sig = -1;
        goto ret;
	}
	(*inop)->disk = disk;
	(*inop)->num = n;
	(*inop)->buf = buf;
	(*inop)->blocknum = block_to_read;
	(*inop)->status = UNCHANGED;

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

ret:
    semaphore_V(itable_lock);
    return sig;
}

/* Return the inode and no write to disk */
void
iput(mem_inode_t ino)
{
    if (NULL == ino)
        return;

	semaphore_P(itable_lock);
	ino->ref_count--;
	if (ino->ref_count == 0) {
		/* Delete this file */
		if (ino->status == TO_DELETE) {
			iclear(ino);
			ifree(ino->num);
		}
//      else {
//			semaphore_P(ino->inode_lock); /* Other process couldn't be using this inode, otherwise count won't be 0 */
//			iupdate(ino);
//			semaphore_V(ino->inode_lock);
//		}
		/* Put inode back to free list, delete from table */
		itable_delete_from_table(ino);
		itable_put_list(ino);
	}
	semaphore_V(itable_lock);
}

/* Return the inode and update it on the disk */
int
iupdate(mem_inode_t ino)
{
    bread(maindisk, ino->blocknum, &(ino->buf));
    memcpy(ino->buf->data + INODE_OFFSET(ino->num), ino, sizeof(struct inode));
    return bwrite(ino->buf);
}

/* Add a block to inode in memory */
int
iadd_block(mem_inode_t ino, blocknum_t blocknum_to_add) {
	int cur_blocknum;

	cur_blocknum = ino->size_blocks;
	/* Still have direct block */
	if (cur_blocknum < INODE_DIRECT_BLOCKS) {
		ino->direct[cur_blocknum] = blocknum_to_add;
		return 0;
	}
	/* Fit in indirect block */
	if (cur_blocknum < (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS)) {
		return add_single_indirect(ino, blocknum_to_add, cur_blocknum);
	}
	/* Fit in double indirect block */
	if (cur_blocknum < (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS)) {
		return add_double_indirect(ino, blocknum_to_add, cur_blocknum);
	}
	if (cur_blocknum < INODE_MAX_FILE_BLOCKS) {
		return add_triple_indirect(ino, blocknum_to_add, cur_blocknum);
	}
	return -1;
}

/* Remove the last block of an inode, if any */
int
irm_block(mem_inode_t ino) {
	blocknum_t blocksize;
	blocknum_t blocknum;

	if (ino->size <= 0 || ino->size_blocks <= 0) {
		return 0;
	}
	blocksize = ino->size_blocks;
	printf("blocksize: %ld\n", blocksize);
	/* In direct block */
	if (blocksize <= INODE_DIRECT_BLOCKS) {
		blocknum = ino->direct[blocksize - 1];
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
		    printf("target_entry->inode_num: %ld\n", target_entry->inode_num);
			if (target_entry->inode_num == inodenum) {
				is_found = 1;
				break;
			}
			target_entry++;
		}
		if (is_found == 1) {
			break;
		} else {
			brelse(buf);
		}
	}
	if (is_found == 0) {
	    printf("idelete - not found.\n");
		return -1;
	}
	printf("idelete - mid.\n");
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
		memcpy(target_entry, src_entry, DIR_ENTRY_SIZE);
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
		ino->size_blocks--;
	}
	return 0;
}

/* Add entry to directy, size is not incremented */
int
iadd_to_dir(mem_inode_t ino, char* filename, inodenum_t inodenum) {
	struct dir_entry entry;
	int entry_num;
	blocknum_t blocknum;
	buf_block_t buf;
	int offset;

	if (ino == NULL || ino->type != MINIDIRECTORY || ino->status == TO_DELETE) {
		return -1;
	}
	strcpy(entry.name, filename);
	entry.inode_num = inodenum;

	entry_num = ino->size;
	/* Need a new block */
	if (entry_num % ENTRY_NUM_PER_BLOCK == 0) {
		blocknum = balloc(maindisk);
		if (bread(maindisk, blocknum, &buf) != 0) {
			return -1;
		}
		memcpy(buf, (void*)&entry, DIR_ENTRY_SIZE);
		bwrite(buf);
		iadd_block(ino, blocknum);
		ino->size_blocks++;
		return 0;
	}
	blocknum = blockmap(maindisk, ino, ino->size_blocks - 1);
	if (bread(maindisk, blocknum, &buf) != 0) {
		return -1;
	}
	offset = entry_num % ENTRY_NUM_PER_BLOCK;
	memcpy(buf->data + offset * DIR_ENTRY_SIZE, (void*)&entry, DIR_ENTRY_SIZE);
	bwrite(buf);
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
	memcpy((void*)&blocknum, (s_buf->data + 8 * offset), sizeof(blocknum_t));
	printf("block number to remove is %ld\n", blocknum);

	if (offset == 0) {
		printf("Need to remove indirect block %ld\n", s_buf->num);
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
	buf_block_t d_buf, s_buf;

	doffset = double_offset(blocksize - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS) - 1);
	soffset = single_offset(blocksize - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS) - 1 - doffset * INODE_INDIRECT_BLOCKS);

	if (bread(ino->disk, ino->double_indirect, &d_buf) != 0) {
		return -1;
	}
	memcpy((void*)&blocknum, (d_buf->data + 8 * doffset), sizeof(blocknum_t));

	if (bread(ino->disk, blocknum, &s_buf) != 0) {
		brelse(d_buf);
		return -1;
	}
	memcpy((void*)&blocknum, (s_buf->data + 8 * soffset), sizeof(blocknum_t));

	if (soffset == 0) {
		bfree(s_buf->num);
		if (doffset == 0) {
			bfree(d_buf->num);
		}
	}
    brelse(d_buf);
    brelse(s_buf);

	bfree(blocknum);
	return 0;
}

static int
rm_triple_indirect(mem_inode_t ino, int blocksize) {
	int soffset, doffset, toffset;
	blocknum_t blocknum;
	buf_block_t s_buf, d_buf, t_buf;

	toffset = triple_offset(blocksize - 1 - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS));
	doffset = double_offset(blocksize - 1 - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) - INODE_DOUBLE_BLOCKS * toffset);
	soffset = single_offset(blocksize - 1 - (INODE_DIRECT_BLOCKS + INODE_INDIRECT_BLOCKS + INODE_DOUBLE_BLOCKS) - INODE_DOUBLE_BLOCKS * toffset - INODE_INDIRECT_BLOCKS * doffset);

	if (bread(ino->disk, ino->triple_indirect, &t_buf) != 0) {
		return -1;
	}
	memcpy((void*)&blocknum, (t_buf->data + 8 * toffset), sizeof(blocknum_t));

	if (bread(ino->disk, blocknum, &d_buf) != 0) {
		brelse(t_buf);
		return -1;
	}
	memcpy((void*)&blocknum, (d_buf->data + 8 * doffset), sizeof(blocknum_t));
	if (bread(ino->disk, blocknum, &s_buf) != 0) {
		brelse(t_buf);
		brelse(d_buf);
		return -1;
	}
	memcpy((void*)&blocknum, (s_buf->data + 8 * soffset), sizeof(blocknum_t));
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
	bfree(blocknum);

	return 0;
}

/* Free all indirect blocks */
static int
free_all_indirect(mem_inode_t ino, blocknum_t datablock_num)
{
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
		memcpy((void*)&bnum, (buf->data + 8 * i), sizeof(blocknum_t));
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
		memcpy((void*)&bnum, (buf->data + 8 * i), sizeof(blocknum_t));
        free_double_indirect(bnum, relse_blocknum);
		i++;
	}
	brelse(buf);

	free_single_indirect(blocknum);
	return 0;
}

static int
add_single_indirect(mem_inode_t ino, blocknum_t blocknum_to_add, int cur_blocknum) {
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
	soffset = single_offset(cur_blocknum - INODE_DIRECT_BLOCKS);
	memcpy((new_buf->data + 8 * soffset), (void*)&blocknum_to_add, sizeof(blocknum_t));
	ino->indirect = new_buf->num;
	bwrite(new_buf);
	return 0;
}


static int
add_double_indirect(mem_inode_t ino, blocknum_t blocknum_to_add, int cur_blocknum) {
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
		memcpy(new_buf->data, (void*)&blocknum_to_add, sizeof(blocknum_t));

		blocknum_2 = balloc(ino->disk);
		bread(maindisk, blocknum_2, &sec_buf);
		if (sec_buf == NULL) {
			return -1;
		}
		memcpy(sec_buf->data, (void*)&(new_buf->num), sizeof(blocknum_t));
		ino->double_indirect = sec_buf->num;

		bwrite(new_buf);
		bwrite(sec_buf);
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
			memcpy(sec_buf->data, (void*)&blocknum_to_add, sizeof(blocknum_t));
			memcpy((new_buf->data + 8 * doffset), (void*)&blocknum_1, sizeof(blocknum_t));
			bwrite(sec_buf);
			bwrite(new_buf);
			return 0;
		} else {
			memcpy((void*)&blocknum_1, (new_buf->data + 8 * doffset), sizeof(blocknum_t));
			if (bread(ino->disk, blocknum_1, &sec_buf) != 0) {
				brelse(new_buf);
				return -1;
			}
			memcpy((sec_buf->data + 8 * soffset), (void*)&blocknum_to_add, sizeof(blocknum_t));
			bwrite(sec_buf);
			brelse(new_buf);
		}
	}
	return 0;
}

static int
add_triple_indirect(mem_inode_t ino, blocknum_t blocknum_to_add, int cur_blocknum) {
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
		memcpy(new_buf->data, (void*)&blocknum_to_add, sizeof(blocknum_t));

		blocknum = balloc(ino->disk);
		bread(maindisk, blocknum, &sec_buf);
		if (sec_buf == NULL) {
			return -1;
		}
		memcpy(sec_buf->data, (void*)&(new_buf->num), sizeof(blocknum_t));

		blocknum = balloc(ino->disk);
		bread(maindisk, blocknum, &thr_buf);
		if (thr_buf == NULL) {
			return -1;
		}
		memcpy(thr_buf->data, (void*)&(sec_buf->num), sizeof(blocknum_t));

		ino->triple_indirect = blocknum;
		bwrite(new_buf);
		bwrite(sec_buf);
		bwrite(thr_buf);
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
			memcpy((void*)&blocknum, (thr_buf->data + 8 * toffset), sizeof(blocknum_t));
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
			memcpy((void*)&blocknum, (sec_buf->data + 8 * doffset), sizeof(blocknum_t));
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
		memcpy(new_buf->data + 8 * soffset, (void*)&blocknum_to_add, sizeof(blocknum_t));
		blocknum = new_buf->num;
		memcpy(sec_buf->data + 8 * doffset, (void*)&blocknum, sizeof(blocknum_t));
		blocknum = sec_buf->num;
		memcpy(thr_buf->data + 8 * toffset, (void*)&blocknum, sizeof(blocknum_t));
		bwrite(new_buf);
		bwrite(sec_buf);
		bwrite(thr_buf);
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
	memcpy((void*)&ret_blocknum, (buf->data + 8 * offset_to_read), sizeof(blocknum_t));
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
	memcpy((void*)&block_to_read, (buf->data + 8 * offset_to_read), sizeof(blocknum_t));
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
	memcpy((void*)&block_to_read, (buf->data + 8 * offset_to_read), sizeof(blocknum_t));
	brelse(buf);

	return double_indirect(disk, block_to_read, block_offset - offset_to_read * POINTER_PER_BLOCK * POINTER_PER_BLOCK);
}
