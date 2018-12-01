#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>

#define PAGE_SIZE	4096
#define FS_PAGES	21

#define CEILDV(A,B)	((A+B-1)/B)
#define BLKSZ		(sz_blk)1024
#define NULLOFF		(size_t)0
#define O2P(off)	(void*)(off+fsptr)
#define P2O(ptr)	(offset)(ptr-fsptr)
#define	FILES_DIR	4
#define	BLOCKS_NODE	5//such that sizeof(node) is 16 words
#define NAMELEN		256

typedef size_t offset;
typedef size_t sz_blk;
typedef ssize_t nodei;
typedef struct node node;
typedef struct fsheader fsheader;
typedef struct freeblk freeblk;
typedef struct dirfile dirfile;

//use signed word for inodes not ino_t, -1 is error condition/null entry

struct node{
	mode_t mode;
	nlink_t nlinks;
	off_t size;
	sz_blk nblocks;
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;
	
	offset blocks[BLOCKS_NODE];
	offset blocklist;
};
struct fsheader{
	size_t size;
	sz_blk free;
	sz_blk ntsize;
	offset freelist;
	offset nodetbl;
};
struct freeblk{
	sz_blk size;
	offset next;
	offset prev;
};
struct dirfile{
	char name[NAMELEN];
	nodei node;
};

nodei newnode()
{
	//find a free node
	return -1;
}

/* Implements an emulation of the stat system call on the filesystem 
   of size fssize pointed to by fsptr. 
   
   If path can be followed and describes a file or directory 
   that exists and is accessable, the access information is 
   put into stbuf. 

   On success, 0 is returned. On failure, -1 is returned and 
   the appropriate error code is put into *errnoptr.

   man 2 stat documents all possible error codes and gives more detail
   on what fields of stbuf need to be filled in. Essentially, only the
   following fields need to be supported:

   st_uid      the value passed in argument
   st_gid      the value passed in argument
   st_mode     (as fixed values S_IFDIR | 0755 for directories,
                                S_IFREG | 0755 for files)
   st_nlink    (as many as there are subdirectories (not files) for directories
                (including . and ..),
                1 for files)
   st_size     (supported only for files, where it is the real file size)
   st_atim
   st_mtim

*/
int __myfs_getattr_implem(void *fsptr, size_t fssize, int *errnoptr, uid_t uid, gid_t gid, const char *path, struct stat *stbuf)
{
	fsinit(fsptr,fssize);
	//path2node(path);
	return 0;
}

/* Implements an emulation of the mknod system call for regular files
   on the filesystem of size fssize pointed to by fsptr.

   This function is called only for the creation of regular files.

   If a file gets created, it is of size zero and has default
   ownership and mode bits.

   The call creates the file indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mknod.

*/
int __myfs_mknod_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
  /* STUB */
  return -1;
}



/* Implements an emulation of the rmdir system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call deletes the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The function call must fail when the directory indicated by path is
   not empty (if there are files or subdirectories other than . and ..).

   The error codes are documented in man 2 rmdir.

*/
int __myfs_rmdir_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
  /* STUB */
  return -1;
}

/* Implements an emulation of the mkdir system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call creates the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mkdir.

*/
int __myfs_mkdir_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path) {
  /* STUB */
  return -1;
}

int ffree()
{
	//remove directory entry, free dblock if necc
	//decrement link count
	//if !zero return
	//go through node blocklist
		//add block to free list(and merge)
	//free blocklist blocks
	//null node block offsets
	return -1;
}
int falloc()
{
	//check space
	//find a free node
	//init node info
	//trawl the freelist
		//go through blocks in free region
			//add to node list
			//if list full, expand
		//resize/remove free region
	//add directory entry
	return -1;
}
int frealloc()
{
	//check if expand shink fits in last block of file
	return -1;
}

void ndlink()
{
}

void ndunlink()
{
}

void fblkadd()
void fblkrem()

offset blkalloc(void *fsptr)
{
	fsheader *fsh=(fsheader*)fsptr;
	freeblk *fhead;
	
	if(fsh->freelist==NULLOFF) return NULLOFF;
	fhead=(freeblk*)O2P(fsh->freelist);
	//decrement free region size, shift header
	fsh->freelist=fhead->next;
	fsh->free--;
	return P2O(fhead);
}
void blkfree(void *fsptr, offset blk)
{
	fsheader *fsh=fsptr;
	freeblk *fhead;
	
	if(blk==NULLOFF) return;
	if(fsh->freelist==NULLOFF){
		fsh->freelist=blk;
		fhead=(freeblk*)O2P(blk);
	}else{
		if(blk<fsh->freelist)
			//if earliest, add as head
		//iterate freelist
			//if right before list entry, merge
			//if right after prev, merge
	}fsh->free++;
}

void fsinit(void *fsptr, size_t fssize)
{
	fsheader *fsh=fsptr;
	freeblk *fhead;
	node *rootnode;
	if(fsh->size==fssize) return;
	
	fsh->ntsize=(1+BLKSZ/sizeof(node)+fssize/BLKSZ)/(1+BLKSZ/sizeof(node));
	fsh->nodetbl=sizeof(node);
	fsh->freelist=fsh->ntsize*BLKSZ;
	fsh->free=fssize/BLKSZ-fsh->ntsize;
	
	//initialize freelist head
	fhead=(freeblk*)O2P(fsh->freelist);
	fhead->size=fsh->free;
	fhead->next=NULLOFF;
	fhead->prev=NULLOFF;
	
	//initialize root node
	rootnode=(node*)O2P(nodetbl);
	
	//initialize root dir
	
	fsh->size=fssize;
}

void printfree(void *fsptr)
{
	fsheader *fsh=fsptr;
	freeblk *frblk;
	offset fblkoff;
	
	if(fsh->size==0) return;
	
	fblkoff=fsh->freelist;
	while(fblkoff!=NULLOFF){
		frblk=(freeblk*)O2P(fblkoff);
		printf("Free region @ %ld, with %ld blocks\n",fblkoff,frblk->size);
		fblkoff=frblk->next;
	}
}

void printdirtree(void *fsptr)
{

}

void printnodetbl(void *fsptr)
{
	fsheader *fsh=fsptr;
	node *nodetbl=(node*)O2P(fsh->nodetbl);
	nodei i, nodect;
	
	if(fsh->size==0) return;
	
	nodect=(fsh->ntsize*BLKSZ/sizeof(node))-1;
	for(i=0;i<nodect;i++){
		printf("Node %ld:\n",i);
		if(nodetbl[i].nlinks){
			if(nodetbl[i].mode &S_IFDIR) printf("\tdirectory\n");
			printf("\tlinks: %ld, size: %ld",nodetbl[i].nlinks,nodetbl[i].size);
		}else{
			printf("\tempty\n");
		}
	}
}

void printfs(void *fsptr)
{
	fsheader *fsh=fsptr;
	printf("Filesystem @%p\n",fsptr);
	if(fsh->size==0){
		printf("\tempty\n");
		return;
	}printf("\tSize:%ld bytes in %ld blocks of size %ld\n",fsh->size,fsh->size/BLKSZ,BLKSZ);
	printf("\tNode table of %ld blocks with %ld entries\n",fsh->ntsize,fsh->ntsize*BLKSZ/sizeof(node)-1);
	printf("\tFree: %ld blocks, %ld bytes, first free block @ %ld\n",fsh->free,fsh->free*BLKSZ,fsh->freelist);
}

int main()
{
	size_t fssize = PAGE_SIZE*FS_PAGES;
	void *fsptr = mmap(NULL, fssize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(fsptr==MAP_FAILED) return 1;
	
	fsinit(fsptr, fssize);
	printfs(fsptr);
	printfree(fsptr);
	printnodetbl(fsptr);
	
	munmap(fsptr, fssize);
	return 0;
}
