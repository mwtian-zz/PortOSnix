#include <string.h>

#include "minifile_inode.h"
#include "minifile_path.h"
#include "minithread.h"

/*
 * Translate path to inode number
 * Return 0 on failure
 */
inodenum_t
namei(char* path) {
	inodenum_t working_inodenum;  /* Current working inode number */
	inodenum_t ret_inodenum = 1;      /* Inode number to return */
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
	char* path_buf;

	if (strlen(path) <= 0) {
		return 0;
	}
	path_buf = malloc(strlen(path) + 1);
	strcpy(path_buf, path);

	/* Start with root directory */
	if (path_buf[0] == '/') {
		working_inode = root_inode;
	} else {
		working_inodenum = minithread_wd();
		if (iget(maindisk, working_inodenum, &working_inode) != 0) {
			return 0;
		}
	}

	pch = strtok(path_buf, "/");
	while (pch != NULL) {
		semaphore_P(working_inode->inode_lock);
		if (working_inode->type != MINIDIRECTORY || working_inode->status == TO_DELETE) {
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
		semaphore_V(working_inode->inode_lock);
		if (working_inode != root_inode) {
			iput(working_inode);
		}
		if (isFound == 0) {
			free(path_buf);
			return 0;
		}
		if (iget(maindisk, working_inodenum, &working_inode) != 0) {
			free(path_buf);
			return 0;
		}
		pch = strtok(NULL, "/");
	}
	if (working_inode != root_inode) {
		iput(working_inode);
	}
	free(path_buf);

	return ret_inodenum;
}

/* Translate path to inode number base on given inode, return 0 on failure*/
inodenum_t
nameinode(char* path, mem_inode_t ino) {
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
	char* path_buf;

	if (strlen(path) <= 0) {
		return 0;
	}
	path_buf = malloc(strlen(path) + 1);
	strcpy(path_buf, path);

	working_inode = ino;
	working_inodenum = ino->num;

	pch = strtok(path_buf, "/");
	while (pch != NULL) {
		semaphore_P(working_inode->inode_lock);
		if (working_inode->type != MINIDIRECTORY) {
			free(path_buf);
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
				free(path_buf);
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
		semaphore_V(working_inode->inode_lock);
		if (working_inode != root_inode) {
			iput(working_inode);
		}
		if (isFound == 0) {
			free(path_buf);
			return 0;
		}
		if (iget(maindisk, working_inodenum, &working_inode) != 0) {
			free(path_buf);
			return 0;
		}
		pch = strtok(NULL, "/");
	}
	if (working_inode != root_inode) {
		iput(working_inode);
	}
	free(path_buf);
	return ret_inodenum;
}

/* Get all the directory entries in directory inode, return NULL if no entries */
dir_entry_t*
get_directory_entry(disk_t* disk, mem_inode_t ino, int* entry_size) {
	size_t block_num;             /* Number of data blocks in an inode */
	size_t entry_num;             /* Number of entries in a directory inode */
	size_t existing_entry;        /* Number of existing entries in a data block */
	size_t left_entry;            /* Number of entries left to exam */
	blocknum_t blocknum;          /* Data block number to read */
	buf_block_t buf;              /* Block buffer */
	dir_entry_t* dir_entries;
	dir_entry_t entry, tmp_entry;
	int i, j;

	entry_num = ino->size;
	block_num = ino->size_blocks;
	if (entry_num <= 0) {
		entry_size = 0;
		return NULL;
	}
	dir_entries = malloc(entry_num * sizeof(dir_entry_t));
	memset(dir_entries, 0, entry_num * sizeof(dir_entry_t));
	*entry_size = entry_num;
	for (i = 0; i < block_num; i++) {
		left_entry = entry_num - i * ENTRY_NUM_PER_BLOCK;
		existing_entry = (left_entry > ENTRY_NUM_PER_BLOCK ? ENTRY_NUM_PER_BLOCK : left_entry);
		blocknum = blockmap(maindisk, ino, i);
		if (blocknum == -1) {
			continue;
		}
		if (bread(disk, blocknum, &buf) != 0) {
			continue;
		}
		tmp_entry = (dir_entry_t)buf->data;
		for (j = 0; j < existing_entry; j++) {
			entry = malloc(sizeof(struct dir_entry));
			memcpy(entry, tmp_entry++, sizeof(struct dir_entry));
			dir_entries[i * ENTRY_NUM_PER_BLOCK + j] = entry;
		}
		brelse(buf);
	}
	return dir_entries;
}

char*
get_filename(char* path) {
	char* pch;
	char* filename = NULL;
	char* path_buf;

	path_buf = malloc(strlen(path) + 1);
	strcpy(path_buf, path);
	pch = strtok(path_buf, "/");
	while (pch != NULL) {
		filename = realloc(filename, strlen(pch) + 1);
		strcpy(filename, pch);
		pch = strtok(NULL, "/");
	}
	free(path_buf);
	return filename;
}

char*
get_path(char* filepath) {
	char* path;
	char* filename;
	char* path_buf;

	path_buf = malloc(strlen(filepath) + 1);
	strcpy(path_buf, filepath);
	if (path_buf[strlen(path_buf) - 1] == '/') {
		path_buf[strlen(path_buf) - 1] = '\0';
	}
	filename = get_filename(path_buf);
	if (strcmp(filename, path_buf) == 0) {
		free(filename);
		return NULL;
	}
	path = malloc(strlen(path_buf) - strlen(filename) + 1);
	strncpy(path, path_buf, (strlen(filepath) - strlen(filename)));
	path[strlen(path_buf) - strlen(filename)] = '\0';
	free(filename);
	free(path_buf);
	return path;
}
