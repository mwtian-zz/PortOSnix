#include "minifile_fs.h"
#include "minifile_cache.h"

/* Super block management */
static int sblock_get(mem_inode_t *inodep, blocknum_t n);
static void sblock_put(mem_inode_t inode);
static int sblock_update(mem_inode_t inode);


int sblock_get(sblock_t *sb)
{

}

void sblock_put(sblock_t sb)
{

}

int sblock_update(sblock_t sb)
{

}

/* Get a free block */
blocknum_t
balloc()
{

}

void
bfree(blocknum_t n)
{

}

blocknum_t
ialloc()
{

}

void
ifree(blocknum_t n)
{

}

int
iclear(blocknum_t n)
{

}

int iget(mem_inode_t *inodep, blocknum_t n)
{

}

void iput(mem_inode_t inode)
{

}

int iupdate(mem_inode_t inode)
{

}
