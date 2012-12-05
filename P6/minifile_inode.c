#include "minifile_inode.h"
#include <string.h>
#include "minifile_fs.h"
#include "minifile_inodetable.h"
#include "minifile_path.h"
#include "minifile_util.h"

/*
 * Allocate a free inode and return a pointer to free inode
 * Return NULL if fail.
 */
mem_inode_t
ialloc(disk_t* disk)
{
    inodenum_t freeinode_num;
    freespace_t freeblk;
	mem_inode_t new_inode;

	semaphore_P(sb_lock);
	/* No more free blocks */
    if (sb->free_blocks <= 0 || sb->free_inodes <= 0) {
		semaphore_V(sb_lock);
        return NULL;
    }

    /* Get free block number and next free block */
    freeinode_num = sb->free_ilist_head;
	if (iget(disk, freeinode_num, &new_inode) != 0) {
		semaphore_V(sb_lock);
		return NULL;
	}
	freeblk = (freespace_t)new_inode;
    sb->free_ilist_head = freeblk->next;
	sb->free_inodes--;
    /* Update superbock and release the empty block */
    sblock_update(sb);
	semaphore_V(sb_lock);
	
    return new_inode;
}

/* Add an inode back to free list */
void
ifree(disk_t* disk, mem_inode_t ino)
{
    freespace_t freeblk;
	inodenum_t freeinode_num;

    if (disk->layout.size <= ino->num) {
        return;
    }
	
	semaphore_P(sb_lock);
	freeinode_num = ino->num;
    freeblk = (freespace_t) (ino);
    freeblk->next = sb->free_ilist_head;
	iupdate(ino);
    sb->free_ilist_head = freeinode_num;
	sb->free_inodes++;
    sblock_update(sb);
	semaphore_V(sb_lock);
}

/* 
 * Clear the content of an inode, including indirect blocks
 * iclear is probably only called in iput which means inode lock is grabbed 
 * and no other process is changing inode
 */
int
iclear(disk_t* disk, mem_inode_t ino)
{
	int datablock_num, i, blocknum;
	buf_block_t buf;
	freespace_t freeblock;
	
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
		blocknum = blockmap(disk, ino, i);
		if (blocknum == -1) {
			printf("Error on mapping block number!\n");
			/* What to do here? */
		}
		if (bread(disk, blocknum, &buf) == 0) {
			freeblock = (freespace_t)buf->data;
			freeblock->next = sb->free_blist_head;
			sb->free_blist_head = buf->num;
			sb->free_blocks++;
			brelse(buf);
		}
	}
	sblock_update(sb);
	
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
	
	/* Put to new queue */
	itable_put_table(*inop);
	semaphore_V(inode_lock);
	
    return 0;
}

/* Return the inode and no write to disk */
void iput(disk_t* disk, mem_inode_t ino)
{
	semaphore_P(inode_lock);
	ino->ref_count--;
	if (ino->ref_count == 0) {
		/* Delete this file */
		if (ino->status == TO_DELETE) {
			iclear(disk, ino);
			ifree(disk, ino);
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
	semaphore_P(ino->inode_lock);
    memcpy(ino->buf->data + INODE_OFFSET(ino->num), ino, sizeof(struct inode));
    bwrite(ino->buf);
	semaphore_V(ino->inode_lock);
	return 0;
}