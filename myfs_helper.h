/*CSCE321 HW4: myfs
	myfs_helper.h: helper constants, types, and function declarations
*/

/*Helper Constants
	P2O				Convert pointer to byte offset, refers to existing fsptr: bad practice
	O2P				Convert byte offset to pointer, '''
	B2P				Convert block offset to pointer, '''
	CLDV			Take ceiling of division
	MIN				Take minimum of two values, better implemented in a function
	FILEMODE		mode for regular files
	DIRMODE			mode for directories
	NODEI_BAD		nodevalid return when inode index invalid
	NODEI_GOOD		nodevalid return when inode at index valid, but not linked to directory
	NODEI_LINKD		nodevalid return when inode at index valid and linked
	NULLOFF			NULL value for offsets and blksets
	NONODE			indicates invalid or nonexistent node
	BLKSZ			size of blocks in fs
	NAMELEN			max length of file names (including '\0')
	NODES_BLOCK		number of inodes in a block
	FILES_DIR		number of direntries in a block
	OFFS_NODE		number of data block offsets in an inode
	OFFS_BLOCK		number of data block offsets in an offblock
*/
/*Helper Types
	nodei			used for indices into the node table -> file identifiers
	blkdex			used for indices into a list of blocks
	offset			byte offset from the beggining of the file system
	blkset			block offset from the beginning of the file system
	sz_blk			size in blocks
	
	fpos			used to store a position in a file
		node			file node, NONODE for invalid files
		nblk			number of the current block within the file
		oblk			blkset of offset block containing current block, NULLOFF when the block is in the inode list
		opos			index of current block within offset block, 1 past at EOF if dpos is at the end of a block
		dblk			blkset of current block, NULLOFF for empty files
		dpos			position within current block: bytes for regular files, entries for directories, 1 past at EOF
		data			offset to data position, NULLOFF at EOF
	direntry		directory entry
		node			file/subdir inode number, NONODE for the entry past the last in a directory, if one exists
		name			name of the file or subdirectory
	offblock		block containing offsets to data blocks
		next			blkset to next offblock if one exists, otherwise NULLOFF
		blocks			data block blksets, all blksets past the last are NULLOFF
	inode			file/directory metadata and location of file data
		mode			unix mode of the file, set to FILEMODE for regular files, DIRMODE for directories
		nlinks			number of links to node
		size			file size, in bytes, or number of entries in a directory
		nblocks			total number of data blocks allocated to the file, excludes offblocks
		atime			time of last access
		mtime			time of last modification
		ctime			creation time/time of last change to inode
		blocks			blksets to first OFFS_NODE or fewer data blocks
		blocklist		blkset to first offblock, or NULLOFF
	freereg			continuous region of free blocks
		next			blkset of next free region, or NULLOFF
		size			number of blocks in the free region
	fsheader		global filesystem header
		size			size of the filesystem in blocks, used to determine if the filesystem has been initialized
		free			number of free blocks in the filesystem
		freelist		blkset of first freereg, or NULLOFF
		ntsize			number of blocks used for the node table
		nodetbl			offset to the node table
*/
/*Helper Functions
	offsort,filter,swap
		heap sort, used by blkfree
	blkalloc(fsptr, count, *buf)
		allocates up to count blocks and places their blksets in buf, returns number of blocks allocated
	blkfree(fsptr, count, *buf)
		frees up to count blocks from buf, sets values in buf to NULLOFF, returns number of blocks freed
	newnode(fsptr)
		finds the first unlinked node in the node table, returns NONODE if one deos not exist
	nodevalid(fsptr, node)
		checks validity of node, returns one of NODEI_BAD, NODEI_GOOD, NODEI_LINKD as described above
	loadpos(fsptr, *pos, node)
		loads fpos pos to the beginning of the file at node, returns 0 on success, -1 when given a bad node of fpos
	advance(fsptr, *pos, blks)
		moves pos ahead in the file up to the next blks blocks, at the start of the block, returns actual advancement
	seek(fsptr, *pos, off)
		moves pos ahead up to off bytes/entries in the file/dir, returns actual advancement
	frealloc(fsptr, node, off)
		tries to change file size to exactly off bytes, only for regular files, returns 0 on success, -1 on failure
	namepathset(*name, *path)
		like strcpy, copies path to name, but also considers '/' to inicate the end of path
	namepatheq(*name, *path)
		similar to strcmp, checks if name and path equal, treats '/' as above, returns 1 if equal, 0 otherwise
	dirmod(fsptr, dir, *name, node, *rename)
		performs operations on directory dir based on the values of node and rename, returns NONODE on failure
		node  NONODE, rename  NULL:	searches for name in dir and returns the node of the entry if found
		node  valid,  rename  NULL:	add an entry with name name if one does not exist  and link to node, returns node
		node  NONODE, rename !NULL:	find name in dir and changes its name to rename if not already present
		node !NONODE, rename !NULL:	find name in dir and remove it, return node of removed entry on success
	path2node(fsptr, *path, **child)
		finds node of the file corresponding to path, returns NONODE if one does not exist
		if child!=NULL, instead returns node of path's parent dir and sets *child to the filename
	fsinit(fsptr,fssize)
		check if the filesystem has been initialized, if not, initialize it to as many blocks fit in fssize
		always succeeds: only two blocks are needed for a working filesystem, and fssize is given as at least 2048
*/

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

#define P2O(ptr)	(offset)((ptr)-fsptr)
#define O2P(off)	(void*)((off)+fsptr)
#define B2P(blk)	(void*)((blk)*BLKSZ+fsptr)
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

typedef size_t blkdex;
typedef size_t offset;
typedef size_t blkset;
typedef size_t sz_blk;
typedef ssize_t nodei;

typedef struct{
	nodei node;
	sz_blk nblk;
	blkset oblk;
	blkdex opos;
	blkset dblk;
	blkdex dpos;
	offset data;
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
void fsinit(void *fsptr, size_t fssize);
