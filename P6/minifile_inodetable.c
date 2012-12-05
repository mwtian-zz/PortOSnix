#include "minifile_inodetable.h"
#include "minifile_fs.h"
#include <string.h>
#include <stdio.h>

static struct inode_table itable; /* Inode table */
static struct mem_inode free_inode[MAX_INODE_NUM]; /* Preallocated free inodes */

static void itable_delete_from_list(mem_inode_t inode); /* Delete inode from free list */


/* Initialize inode table, return 0 on success, -1 on failure */
int
itable_init() {
	int i;
	
	/* Create empty inode hash table */
	memset(itable.inode_hashtable, 0, sizeof(mem_inode_t) * INODE_HASHTABLE_SIZE);
	
	/* Preallocated free inode list */
	free_inode[0].l_prev = NULL;
	free_inode[0].l_next = &free_inode[1];
	free_inode[0].h_prev = NULL;
	free_inode[0].h_next = NULL;
	free_inode[0].inode_lock = semaphore_create();
	if (free_inode[0].inode_lock == NULL) {
		return -1;
	}
	semaphore_initialize(free_inode[0].inode_lock, 1);
	free_inode[MAX_INODE_NUM - 1].l_prev = &free_inode[MAX_INODE_NUM - 2];
	free_inode[MAX_INODE_NUM - 1].l_next = NULL;
	free_inode[MAX_INODE_NUM - 1].h_prev = NULL;
	free_inode[MAX_INODE_NUM - 1].h_next = NULL;
	free_inode[MAX_INODE_NUM - 1].inode_lock = semaphore_create();
	if (free_inode[MAX_INODE_NUM - 1].inode_lock == NULL) {
		return -1;
	}
	semaphore_initialize(free_inode[MAX_INODE_NUM - 1].inode_lock, 1);
	for (i = 1; i < MAX_INODE_NUM - 1; i++) {
		free_inode[i].l_prev = &free_inode[i - 1];
		free_inode[i].l_next = &free_inode[i + 1];
		free_inode[i].h_prev = NULL;
		free_inode[i].h_next = NULL;
		free_inode[i].inode_lock = semaphore_create();
		if (free_inode[i].inode_lock == NULL) {
			return -1;
		}
		semaphore_initialize(free_inode[i].inode_lock, 1);
	}
	
	itable.freelist_head = &free_inode[0];
	itable.freelist_tail = &free_inode[MAX_INODE_NUM - 1];
	
	return 0;
}

/* Get inode from hash table, return 0 if found, -1 if not */
int
itable_get_from_table(inodenum_t inodenum, mem_inode_t* inode) {
	mem_inode_t thead;
	int slotnum = INODE_NUM_HASH(inodenum);
	
	if (inodenum <= 0 || inode == NULL) {
		return -1;
	}
	
	thead = itable.inode_hashtable[slotnum];
	while (thead != NULL) {
		if (thead->num == inodenum) {
			break;
		}
		thead = thead->h_next;
	}
	
	/* Not found */
	if (thead == NULL) {
		return -1;
	}
	*inode = thead;
	/* If still on free list, delete from free list */
	if ((*inode)->l_next != NULL || (*inode)->l_prev != NULL) {
		itable_delete_from_list(*inode);
	}
	return 0;
}

/* Get free inode from free list, return 0 if there is a free one, -1 if not */
int 
itable_get_free_inode(mem_inode_t* inode) {
	if (inode == NULL) {
		return -1;
	}
	*inode = itable.freelist_head;
	/* No free inode */
	if (*inode == NULL) {
		return -1;
	}
	itable_delete_from_list(*inode);
	return 0;
}

/* Put inode to hash table */
void 
itable_put_table(mem_inode_t inode) {
	int slotnum = INODE_NUM_HASH(inode->num);
	inode->h_prev = NULL;
	inode->h_next = itable.inode_hashtable[slotnum];
	if (itable.inode_hashtable[slotnum] != NULL) {
		itable.inode_hashtable[slotnum]->h_prev = inode;
	}
}

/* Delete inode from hash table */
void 
itable_delete_from_table(mem_inode_t inode) {
	int slotnum ;
	mem_inode_t hhead;
	
	/* Not on table */
	if (inode->h_next == NULL && inode->h_prev == NULL) {
		return;
	}
	
	slotnum = INODE_NUM_HASH(inode->num);
	hhead = itable.inode_hashtable[slotnum];
	if (hhead == inode) {
		hhead = inode->h_next;
	}
	if (inode->h_prev != NULL) {
		inode->h_prev->h_next = inode->h_next;
	}
	if (inode->h_next != NULL) {
		inode->h_next->h_prev = inode->h_prev;
	}
	inode->h_prev = NULL;
	inode->h_next = NULL;
}

/* Put inode back to free list */
void itable_put_list(mem_inode_t inode) {
	if (itable.freelist_tail != NULL) {
		itable.freelist_tail->l_next = inode;
		inode->l_prev = itable.freelist_tail;
		itable.freelist_tail = inode;
		inode->l_next = NULL;
	} else {
		itable.freelist_head = inode;
		itable.freelist_tail = inode;
		inode->l_prev = NULL;
		inode->l_next = NULL;
	}
	itable_delete_from_table(inode);
}

/* Delete inode from free list */
static void
itable_delete_from_list(mem_inode_t inode) {
	if (inode == NULL) {
		return;
	}
	if (itable.freelist_head == inode) {
		itable.freelist_head = inode->l_next;
	}
	if (itable.freelist_tail == inode) {
		itable.freelist_tail = inode->l_prev;
	}
	if (inode->l_prev != NULL) {
		inode->l_prev->l_next = inode->l_next;
	}
	if (inode->l_next != NULL) {
		inode->l_next->l_prev = inode->l_prev;
	}
	inode->l_prev = NULL;
	inode->l_next = NULL;
}