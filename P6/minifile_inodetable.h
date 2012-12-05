#ifndef __MINIFILE_INODETABLE_H__
#define __MINIFILE_INODETABLE_H__

#include "minifile_fs.h"

#define MAX_INODE_NUM 300     
#define INODE_HASHTABLE_SIZE 307   
#define INODE_NUM_HASH(n) ((n) % INODE_HASHTABLE_SIZE)

/* Inode table */
struct inode_table {
	mem_inode_t inode_hashtable[INODE_HASHTABLE_SIZE];   /* Inode hash table */
	mem_inode_t freelist_head;                           /* Inode free list head */
	mem_inode_t freelist_tail;                           /* Inode free list tail */
};

extern void itable_init();    /* Initialize inode table */

/* Get inode from hash table, return 0 if found, -1 if not */
extern int itable_get_from_table(inodenum_t, mem_inode_t* inode); 
/* Get free inode from free list, return 0 if there is a free one, -1 if not */
extern int itable_get_free_inode(mem_inode_t* inode);
/* Put inode to hash table */
extern void itable_put_table(mem_inode_t inode);
/* Delete inode from hash table */
extern void itable_delete_from_table(mem_inode_t inode);
/* Put inode back to free list, but can still be in hash table */
extern void itable_put_list(mem_inode_t inode);


#endif /* __MINIFILE_INODETABLE_H__ */