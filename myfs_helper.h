/*CSCE321 HW4: myfs
	myfs_helper.h: helper constants, types, and function declarations
*/

/*Helper Constants
	O2P
	P2O
	FILEMODE
	DIRMODE
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
	index
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
	...
	namepathset
	namepatheq
	dirmod
	path2node
		find node corresponding to path, or NONODE if path does not refer to an existing file
		if child!=NULL, instead returns node of path's parent dir and sets *child to the filename
	fsinit
*/

//BLKSZ%sizeof(direntry), BLKSZ%sizeof(inode) must be 0
//sizeof(offblock) must be BLKSZ
//sizeof(fsheader) must be <= sizeof(node)

//file size is #bytes requested
//file nblocks is #blocks of data
//dir size is #bytes in direntries
//dir nblocks is #blocks of dirfiles

//new B2O,O2B to replace O2P on blksets?
//merge nodegood,nodevalid: use range of return values to represent degree of validity
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

#define FILEMODE	(S_IFREG|0755)
#define DIRMODE		(S_IFDIR|0755)
#define NULLOFF		(offset)0
#define NONODE		(nodei)-1
#define BLKSZ		(size_t)1024
#define NAMELEN		(256-sizeof(nodei))
#define NODES_BLOCK	(BLKSZ/sizeof(inode))
#define FILES_DIR	(BLKSZ/sizeof(direntry))
#define OFFS_BLOCK	(BLKSZ/sizeof(blkset))
#define OFFS_NODE	5
#define BLOCKS_FILE	4

typedef size_t index;//list index
typedef size_t offset;//byte offset
typedef size_t blkset;//block offset
typedef size_t sz_blk;//block size
typedef ssize_t nodei;//node#, nodetbl index

typedef struct{
	nodei node;//file node
	sz_blk nblk;//block # in file
	blkset oblk;//offset of offset block NULLOFF if in node.blocks
	index opos;//index in offset block/node.blocks
	blkset dblk;//offset of data block
	index dpos;//byte of data in dblk for reg files, entry index for dirs 
	offset data;//offset to data
} fpos;
typedef struct{
	nodei node;
	char name[NAMELEN];
} direntry;
typedef struct{
	blkset next;
	blkset blocks[OFFS_BLOCK-1];
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

nodei newnode(void *fsptr);
int nodegood(void *fsptr, nodei node);
int nodevalid(void *fsptr, nodei node);
void namepathset(char *name, const char *path);
int namepatheq(char *name, const char *path);
int fsinit(void *fsptr, size_t fssize);
sz_blk blkalloc(void *fsptr, sz_blk count, blkset *buf);
void blkfree(void *fsptr, sz_blk count, blkset *buf);
void loadpos(void *fsptr, fpos *pos, nodei node);
off_t advance(void *fsptr, fpos *pos, sz_blk blk, off_t off);
int frealloc(void *fsptr, nodei node, off_t size);
nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename);
nodei path2node(void *fsptr, const char *path, char **child);
