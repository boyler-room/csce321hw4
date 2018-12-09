/*CSCE321 HW4: myfs
	myfs_helper.h: helper constants, types, and function declarations
*/

/*Helper Constants
	O2P
	P2O
	CLDV
	MIN
	FILEMODE
	DIRMODE
	NODEI_BAD
	NODEI_GOOD
	NODEI_LINKD
	NULLOFF
	NONODE
	BLKSZ
	NAMELEN
	NODES_BLOCK
	FILES_DIR
	OFFS_NODE
	OFFS_BLOCK
*/
/*Helper Types
	blkdex
	nodei
	offset
	blkset
	sz_blk
	
*/
/*Helper Functions
	offsort
	blkalloc
	blkfree
	newnode
	nodegood
		return 0 on good -1 on bad
	nodevalid
		return 0 on valid -1 on bad
	loadpos
	advance
	seek
	...
	frealloc
	namepathset
	namepatheq
	dirmod
	path2node
		find node corresponding to path, or NONODE if path does not refer to an existing file
		if child!=NULL, instead returns node of path's parent dir and sets *child to the filename
	fsinit
*/

//file size is #bytes requested
//file nblocks is #blocks of data
//dir size is #bytes in direntries
//dir nblocks is #blocks of dirfiles

//do not remove subdirs in dirmod if not empty?

//ceiling(A/B)=(A+B-1)/B

#include <stddef.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#define O2P(off)	(void*)((off)+fsptr)
#define P2O(ptr)	(offset)((ptr)-fsptr)
#define B2P(blk)	(void*)((blk)*BLKSZ+fsptr)
#define P2B(ptr)	(blkset)(((ptr)-fsptr)/BLKSZ)
#define MIN(A,B)	((A<=B)?(A):(B))
#define CLDIV(A,B)	((A+B-1)/B)

#define FILEMODE	(S_IFREG|0755)
#define DIRMODE		(S_IFDIR|0755)
#define NODEI_BAD	0
#define NODEI_GOOD	1
#define NODEI_LINKD	2

#define NULLOFF		(offset)0
#define NONODE		(nodei)-1
#define BLKSZ		(size_t)1024
#define NAMELEN		(256-sizeof(nodei))
#define NODES_BLOCK	(BLKSZ/sizeof(inode))
#define FILES_DIR	(BLKSZ/sizeof(direntry))
#define OFFS_BLOCK	(BLKSZ/sizeof(blkset)-1)
#define OFFS_NODE	5
#define BLOCKS_FILE	4

typedef size_t blkdex;//list index
typedef size_t offset;//byte offset
typedef size_t blkset;//block offset
typedef size_t sz_blk;//block size
typedef ssize_t nodei;//node#, nodetbl index

typedef struct{
	nodei node;//file node
	sz_blk nblk;//block # in file
	blkset oblk;//offset of offset block NULLOFF if in node.blocks
	blkdex opos;//index in offset block/node.blocks
	blkset dblk;//offset of data block
	blkdex dpos;//byte of data in dblk for reg files, entry index for dirs 
	offset data;//offset to data
} fpos;
typedef struct{
	nodei node;
	char name[NAMELEN];
} direntry;
typedef struct{
	blkset next;
	blkset blocks[OFFS_BLOCK];
} offblock;
typedef struct{
	mode_t mode;
	size_t nlinks;
	size_t size;
	sz_blk nblocks;
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;
	
	blkset blocks[OFFS_NODE];
	blkset blocklist;
} inode;
typedef struct{
	sz_blk size;
	blkset next;
} freereg;
typedef struct{
	sz_blk size;
	sz_blk free;
	blkset freelist;
	sz_blk ntsize;
	offset nodetbl;
} fsheader;

sz_blk blkalloc(void *fsptr, sz_blk count, blkset *buf);
sz_blk blkfree(void *fsptr, sz_blk count, blkset *buf);
nodei newnode(void *fsptr);
int nodevalid(void *fsptr, nodei node);
void loadpos(void *fsptr, fpos *pos, nodei node);
sz_blk advance(void *fsptr, fpos *pos, sz_blk blks);
size_t seek(void *fsptr, fpos *pos, size_t off);
int frealloc(void *fsptr, nodei node, size_t size);
void namepathset(char *name, const char *path);
int namepatheq(char *name, const char *path);
nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename);
nodei path2node(void *fsptr, const char *path, const char **child);
int fsinit(void *fsptr, size_t fssize);
