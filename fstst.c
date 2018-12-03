/*
	Filesystem structure: [ node blocks | file blocks ]
		Node blocks: [ fs header | node | ... ]
		File blocks can be: file data, blocks with additional block offsets, directory file blocks
		Directories are stored as regular files, but hold the flag S_IFDIR instead of S_IFREG in their node.mode
	Types:
		offset: byte offset from start of filesystem, use O2P and P2O to convert between them and pointers
		blkset: offset from start of filesystem in blocks, as with offset use NULLOFF to represent invalid offsets
		sz_blk: size in blocks
		nodei: index into node table, NONODE represents an invalid/null node
		fsheader: filesystem global header, should always be at fsptr
		node: file node entry, file metadata and block offsets to the first BLOCKS_NODE data blocks, and to a block 			with additional offsets
		offblock: block with additional block offsets to data
		dir: dir file block, WIP, gaps in file list should be closed when created
		direntry: file entry in dir, contains name and node index, WIP
	Helper functions:
		fsinit() - check if the filesystem has been initialized, and do so if not
		blkalloc() - allocate the first count or fewer blocks, putting the offsets in buf, returns # blocks allocated
		blkfree() - free up to count blocks from the offsets in buf
		newnode() - finds the index of the first empty node
	Debug functions:
		printfs() - print global fs data
		printfree() - list free regions
		printnodetbl() - print node table entries
		printdirtree() - print directory structure
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>

#define PAGE_SIZE	4096
#define FS_PAGES	1

#define FILEMODE	(S_IFREG|0755)
#define DIRMODE		(S_IFDIR|0755)
#define CEILDV(A,B)	((A+B-1)/B)
#define BLKSZ		(size_t)1024
#define NULLOFF		(offset)0
#define NONODE		(nodei)-1
#define O2P(off)	(void*)((off)+fsptr)
#define P2O(ptr)	(offset)((ptr)-fsptr)
#define	FILES_DIR	4//average files per dir?
#define BLOCKS_FILE	4//average blocks per file
#define	BLOCKS_NODE	5//#block offsets in node; such that sizeof(node) is 16 words
#define NAMELEN		(256-sizeof(nodei))

typedef size_t offset;
typedef size_t blkset;
typedef size_t sz_blk;
typedef ssize_t nodei;

typedef struct{
	nodei node;
	char name[NAMELEN];
} direntry;
typedef struct{
	direntry files[BLKSZ/sizeof(direntry)];
} dir;
typedef struct{
	blkset next;
	blkset blocks[BLKSZ-sizeof(offset)];
} offblock;
typedef struct{
	mode_t mode;
	size_t nlinks;
	size_t size;
	sz_blk nblocks;
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;
	
	blkset blocks[BLOCKS_NODE];
	blkset blocklist;
} node;
typedef struct{
	sz_blk size;
	blkset next;
} freereg;
typedef struct{
	sz_blk size;
	sz_blk free;
	size_t ndfree;
	sz_blk ntsize;
	offset nodetbl;
	blkset freelist;
} fsheader;

/*
TODO:
	open
		verify path (path2node)
	readdir
	mkdir
	mknod
	rmdir
	unlink
	rename
	truncate
	read
	write

void ndlink()
{
	fsheader *fshead=(fsheader*)fsptr;
}

void ndunlink()
{
	fsheader *fshead=(fsheader*)fsptr;
}

int ffree()
{
	fsheader *fshead=(fsheader*)fsptr;
	//remove directory entry, free dblock if necc
	//decrement link count
	//if !zero return
	//go through node blocklist
		//add block to free list(and merge)
	//free blocklist blocks
	//null node block offsets
	return -1;
}
nodei falloc()
{
	fsheader *fshead=(fsheader*)fsptr;
	//check space, incl offset blocks
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

int frealloc(void *fsptr, nodei nd, off_t size)
{
	fsheader *fshead=(fsheader*)fsptr;
	node *file;
	ssize_t sdiff;
	if(nd==NONODE) return -1;
	file=O2P(fshead->nodetbl)[nd];
	sdiff=CEILDIV(size,BLKSZ)-file->nblocks;
	
	if(sdiff>0){
		//check if enough blocks in fs
		//seek end of blocklist
		//alloc
		//zero
	}else if(sdiff<0){
		//find last sdiff blocks
		//free
	}//update size
	return 0;
}*/
void makedir(void *fsptr, nodei nd)
{
	
}

void findentry(void *fsptr, nodei dir, char *name)

void linknode(void *fsptr, nodei nd, char *name, dir parent)
{
	//verify/get parent from path
	//check parent is dir
	//check if name already in dir
	//add entry to parent dirfile
	//inc nd link count
}

nodei newnode(void *fsptr)
{
	fsheader *fshead=fsptr;
	node *nodetbl=(node*)O2P(fshead->nodetbl);
	size_t nodect=(fshead->ntsize*BLKSZ/sizeof(node))-1;
	nodei i=0;
	
	while(++i<nodect){
		if(nodetbl[i].nlinks==0) return i;
	}return NONODE;
}

//recursive method?
nodei path2node(void *fsptr, char *path, int parent)//find node corresponding to path or up to parentth parent dir
{
	fsheader *fshead=(fsheader*)fsptr;
	//check for root in path
	//cnodei=root node
	//for subs in path
		//get cnode of cnodei
		//load path sub til next '/'
		//if end of path '\0'
			//ret nodei
		//if file: break/ret error
		//find loaded sub in cnode blocks
		//get cnodei
	return NONODE;
}

sz_blk blkalloc(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *prev=NULL;
	sz_blk alloct=0;
	
	while(freeoff!=NULLOFF && alloct<count){
		freereg *fhead=O2P(freeoff*BLKSZ);
		sz_blk freeblk=0;
		while(freeblk<(fhead->size) && alloct<count){
			buf[alloct++]=freeoff+freeblk++;
		}if(freeblk==(fhead->size)){
			if(prev!=NULL) prev->next=fhead->next;
			else fshead->freelist=fhead->next;
		}else{
			freeoff+=freeblk;
			if(prev!=NULL) prev->next=freeoff;
			else fshead->freelist=freeoff;
			*(freereg*)O2P(freeoff*BLKSZ)=*fhead;
			fhead=(freereg*)O2P(freeoff*BLKSZ);
			fhead->size-=freeblk;
			prev=fhead;
		}freeoff=fhead->next;
	}fshead->free-=alloct;
	return alloct;
}

void offsort(blkset *offs, blkset *targ, size_t len)//clean
{
	size_t i=0,j=0;
	blkset *A,*B;
	
	if(targ==NULL || len==0) return;
	if(len==1) *targ=*offs;
	else if(len==2){
		blkset tmp=offs[(offs[1]>offs[0])];
		targ[0]=offs[(offs[1]<offs[0])];
		targ[1]=tmp;
	}else{
		A=(blkset*)malloc(sizeof(blkset)*len/2);
		offsort(offs,A,len/2);
		B=(blkset*)malloc(sizeof(blkset)*(len-len/2));
		offsort(offs+len/2,B,len-len/2);
		while((i+j)<len){
			if(j==(len-len/2) || (i<len/2 && A[i]<B[j])){
				targ[i+j]=A[i++];
			}else{
				targ[i+j]=B[j++];
			}
		}free(A);
		free(B);
	}
}
void blkfree(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *fhead;
	sz_blk freect=0;
	
	offsort(buf,buf,count);
	while(freect<count && *buf<(fshead->ntsize)){
		*(buf++)=NULLOFF;
		count--;
	}if(freect<count && ((freeoff==NULLOFF && *buf<fshead->size) || *buf<freeoff)){
		fhead=(freereg*)O2P((freeoff=*buf)*BLKSZ);
		fhead->next=fshead->freelist;
		fshead->freelist=freeoff;
		if((freeoff+(fhead->size=1))==fhead->next){
			freereg *tmp=O2P(fhead->next*BLKSZ);
			fhead->size+=tmp->size;
			fhead->next=tmp->next;
		}buf[freect++]=NULLOFF;
	}while(freect<count && buf[freect]<fshead->size){
		fhead=(freereg*)O2P(freeoff*BLKSZ);
		if(buf[freect]>=(freeoff+fhead->size)){
			if(fhead->next!=NULLOFF && buf[freect]>=fhead->next){
				freeoff=fhead->next;
				continue;
			}if(buf[freect]==(freeoff+fhead->size)){
				fhead->size++;
			}else{
				freereg *tmp=O2P((freeoff=buf[freect])*BLKSZ);
				tmp->next=fhead->next;
				fhead->next=freeoff;
				fhead=tmp;
			}if((freeoff+fhead->size)==fhead->next){
				freereg *tmp=O2P(fhead->next*BLKSZ);
				fhead->size+=tmp->size;
				fhead->next=tmp->next;
			}buf[freect++]=NULLOFF;
		}else{
			(buf++)[freect]=NULLOFF;
			count--;
		]
	}while(freect<count){
		(buf++)[freect]=NULLOFF;
		count--;
	}fshead->free+=freect;
}

void fsinit(void *fsptr, size_t fssize)
{
	fsheader *fshead=fsptr;
	freereg *fhead;
	node *root;
	
	if(fshead->size==fssize/BLKSZ) return;
	
	fshead->ntsize=(BLOCKS_FILE*(1+BLKSZ/sizeof(node))+fssize/BLKSZ)/(1+BLOCKS_FILE*BLKSZ/sizeof(node));
	fshead->ndfree=(fshead->ntsize*BLKSZ/sizeof(node))-2;
	fshead->nodetbl=sizeof(node);
	fshead->nfree=BLKSZ*fshead->ntsize/sizeof(node);
	fshead->freelist=fshead->ntsize;
	fshead->free=fssize/BLKSZ-fshead->ntsize;
	
	fhead=(freereg*)O2P(fshead->freelist*BLKSZ);
	fhead->size=fshead->free;
	fhead->next=NULLOFF;
	
	//initialize root node
	root=(node*)O2P(fshead->nodetbl);
	root->mode=DIRMODE;
	//set timestamps
	root->nlinks=1;
	for(int i=0;i<BLOCKS_NODE;i++)
		root->blocks[i]=NULLOFF;
	root->blocklist=NULLOFF;
	root->nblocks=blkalloc(fsptr,1,root->blocks);
	root->size=root->nblocks*BLKSZ;
	//init dirfile
	
	fshead->size=fssize/BLKSZ;
}

//Debug Functions===================================================================================

void printdirtree(void *fsptr)
{

}

void printnodes(void *fsptr)
{
	fsheader *fshead=fsptr;
	node *nodetbl=O2P(fshead->nodetbl);
	size_t nodect;
	nodei i;
	
	if(fshead->size==0) return;
	
	nodect=(fshead->ntsize*BLKSZ/sizeof(node))-1;
	printf("\tNode table of %ld blocks with %ld entries\n",fshead->ntsize,nodect);
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

void printfree(void *fsptr)
{
	fsheader *fshead=fsptr;
	freereg *frblk;
	blkset fblkoff;
	
	if(fshead->size==0) return;
	
	fblkoff=fshead->freelist;
	printf("\tFree: %ld bytes in %ld blocks, freelist @ block %ld\n",fshead->free*BLKSZ,fshead->free,fblkoff);
	while(fblkoff!=NULLOFF){
		frblk=(freereg*)O2P(fblkoff*BLKSZ);
		printf("\t\tfree region @ block %ld, with %ld blocks\n",fblkoff,frblk->size);
		fblkoff=frblk->next;
	}
}

void printfs(void *fsptr)
{
	fsheader *fshead=fsptr;
	
	printf("Filesystem @%p\n",fsptr);
	if(fshead->size==0){
		printf("\tempty\n");
		return;
	}printf("\tSize:%ld bytes in %ld %ld byte blocks\n",fshead->size*BLKSZ,fshead->size,BLKSZ);
	printfree(fsptr);
	//printnodes(fsptr);
}

int main()
{
	size_t fssize = PAGE_SIZE*FS_PAGES;
	void *fsptr = mmap(NULL, fssize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(fsptr==MAP_FAILED) return 1;
	
	fsinit(fsptr, fssize);
	printfs(fsptr);
	
	munmap(fsptr, fssize);
	return 0;
}
