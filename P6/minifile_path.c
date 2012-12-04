#include "minifile_path.h"
#include "minithread.h"
#include <string.h>

/*
 * Translate path to inode number
 * Return 0 on failure
 */
inodenum_t
namei(char* path) {
	inodenum_t working_inodenum;  /* Current working inode number */
	inodenum_t ret_inodenum;      /* Inode number to return */
	mem_inode_t working_inode;    /* Current working inode */
	char* pch;                    /* Parsed path */
	size_t block_num;             /* Number of data blocks in an inode */ 
	size_t entry_num;             /* Number of entries in a directory inode */
	size_t existing_entry;        /* Number of existing entries in a data block */
	size_t left_entry;            /* Number of entries left to exam */
	blocknum_t blocknum;          /* Data block number to read */
	buf_block_t buf;              /* Block buffer */
	dir_entry_t entry;            /* Directory entry */
	char isFound = 0;             /* If a directory is found */
	int i, j;
	
	if (strlen(path) <= 0) {
		return 0;
	}
	
	/* Start with root directory */
	if (path[0] == "/") {
		working_inode = root_inode;
	} else {
		working_inodenum = minithread_self()->current_dir;
		if (iget(maindisk, working_inodenum, &working_inode) != 0) {
			return 0;
		}
	}
	
	pch = strtok(path, "/");
	while (pch != NULL) {
		semaphore_P(working_inode->inode_lock);
		if (working_inode->type != MINIDIRECTORY) {
			return 0;
		}
		entry_num = working_inode->size;
		block_num = (entry_num - 1) / ENTRY_NUM_PER_BLOCK + 1;
		isFound = 0;
		for (i = 0; i < block_num; i++) {
			left_entry = entry_num - i * ENTRY_NUM_PER_BLOCK;
			existing_entry = (left_entry > ENTRY_NUM_PER_BLOCK ? ENTRY_NUM_PER_BLOCK : left_entry);
			blocknum = blockmap(maindisk, working_inode, i);
			if (bread(maindisk, blocknum, &buf) != 0) {
				return 0;
			}
			entry = (dir_entry_t)buf->data;
			for (j = 0; j < existing_entry; j++) {
				if (strcmp(pch, entry->name) == 0) {
					working_inodenum = entry->inode_num;
					ret_inodenum = working_inodenum;
					isFound = 1;
					break;
				} else {
					entry++;
				}
			}
			brelse(buf);
			if (isFound == 1) {
				break;
			}
		}
		semaphore_V(working_inode);
		if (working_inode != root_inode) {
			iput(maindisk, working_inode);
		}
		if (isFound == 0) {
			return 0;
		} 
		if (iget(maindisk, working_inodenum, &working_inode) != 0) {
			return 0;
		}
		pch = strtok(NULL, "/");
	}
	if (working_inode != root_inode) {
		iput(maindisk, working_inode);
	}
	return ret_inodenum;
}