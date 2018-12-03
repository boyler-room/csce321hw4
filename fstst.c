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
		node: file node entry, file metadata and block offsets to the first OFFS_NODE data blocks, and to a block 			with additional offsets
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

#define CEILDV(A,B)	((A+B-1)/B)
#define O2P(off)	(void*)((off)+fsptr)
#define P2O(ptr)	(offset)((ptr)-fsptr)
#define NULLOFF		(offset)0
#define NONODE		(nodei)-1
#define BLKSZ		(size_t)1024
#define NAMELEN		(256-sizeof(nodei))
#define NODES_BLOCK	(BLKSZ/sizeof(inode))
#define	FILES_DIR	(BLKSZ/sizeof(direntry))
#define	OFFS_BLOCK	(BLKSZ/sizeof(blkset))
#define	OFFS_NODE	5//#block offsets in inode; such that sizeof(inode) is 16 words
#define BLOCKS_FILE	4//average blocks per file
#define FILEMODE	(S_IFREG|0755)
#define DIRMODE		(S_IFDIR|0755)

typedef size_t offset;
typedef size_t blkset;
typedef size_t sz_blk;
typedef ssize_t nodei;

typedef struct{
	nodei node;
	char name[NAMELEN];
} direntry;
typedef struct{
	blkset next;
	blkset blocks[BLKSZ/sizeof(blkset)-1];
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
	size_t ndfree;
	sz_blk ntsize;
	offset nodetbl;
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
	inode *file;
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
/*
void makedir(void *fsptr, nodei node)
{
	
}

nodei findfile(void *fsptr, nodei dir, char *name)
{
}

void linknode(void *fsptr, nodei node, char *name, dir parent)
{
	//verify/get parent from path
	//check parent is dir
	//check if name already in dir
	//add entry to parent dirfile
	//inc nd link count
}*/

nodei newnode(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t nodect=fshead->ntsize*NODES_BLOCK-1;
	nodei i=0;
	
	while(++i<nodect){
		if(nodetbl[i].nlinks==0) return i;
	}return NONODE;
}

int namepatheq(char *name, char *path)
{
	while(*path!='/' && *path!='\0'){
		if(*name=='\0' || *name!=*path) return 0;
		name++; path++;
	}return (*name=='\0');
}

/*

//convert path to linked list?
nodei subpathnode(void *fsptr, nodei node, char* subpath)
{
	if(subpath[0]=='\0') return node;
	//find first subpath entry in
}

int nodevalid(void *fsptr, nodei node)
{
	fsheader *fshead=(fsheader*)fsptr;
	node *nodetbl=O2P(fshead->nodetbl);
	
	if(node==NONODE || node>=(fshead->ntsize*NODES_BLOCK-1)) return 0;
	if(nodetbl[node].nlinks<=0 || n) return 0;
	if() return 0;
	return 1;
}

nodei dirmod(void *fsptr, nodei dir, char *name, nodei node, char *rename)
{
	fsheader *fshead=(fsheader*)fsptr;
	node *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, dblk=nodetbl[dir].blocks[0];
	size_t off=0;
	
	if(nodetbl[dir].mode!=DIRMODE) return NONODE;
	
	while(dblk!=NULL){
		size_t fl;
		for(fl=0;fl<FILES_DIR;fl++){
			//go through dir entries
		}
		off++;
		if(oblk==NULLOFF){//in node list
			if(off==OFFS_NODE){
				off=0;
				oblk=nodetbl[dir].blocklist;
				if(oblk==NULLOFF) dblk=NULLOFF;
				else dblk=(offblock*)O2P(oblk*BLKSZ).blocks[0];
			}else dblk=nodetbl[dir].blocks[off];
		}else{//in offset block
			if
		}
	}
	//find node: node==NONODE, rename==NULL
		//return node on found
		//after loop: return NONODE
	//add if not found: node!=NONODE, rename==NULL
		//return NONODE on found
		//after loop: extend dir if needed, append new entry, return node
	//find and remove: node!=NONODE, rename!=NULL
		//save entry location on found, break?
		//after loop: if location saved overwrite w/ last entry, null last entry, shrink dir if needed
			//return node if found, NONODE if not
	//find and change name: node==NONODE, rename!=NULL
		//change name and return on found
		//after loop: return NONODE
	
	//returns node on success, NONODE on fail
}
direntry* dirfind(void *fsptr, nodei dir, char *name)
{
	//search node blocklist
	//search 
	//return on found
}
int diradd(void *fsptr, nodei dir, char *name, nodei node)
{
	//dirfind, return fail if found
	//at end of dir: extend if needed
	//set direntry node,name
}
int dirrem(void *fsptr, nodei dir, char *name)
{
	//dirfind
	//if not found ret fail
	//get last direntry
	//set found entry with last entry
	//nullout last entry, free block if needed
}

char* pathsplit(char *path)
{
	set last / to '\0'
	return ptr to name
}
*/
//split off last entry in path

nodei path2node(void *fsptr, char *path)//, int parent)//find node corresponding to path or up to parentth parent dir
{
	fsheader *fshead=(fsheader*)fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	nodei node=0;
	size_t sub=1;
	
	if(path[0]!='/') return NONODE;
	
	while(path[sub]!='\0'){
		//node=dirmod(fsptr,node,&path[sub],NONODE,NULL);
		if(node==NONODE) return NONODE;
		while(path[sub]!='\0'){
			if(path[sub++]=='/') break;
		}
	}return node;
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
		}
	}while(freect<count){
		(buf++)[freect]=NULLOFF;
		count--;
	}fshead->free+=freect;
}

void fsinit(void *fsptr, size_t fssize)
{
	fsheader *fshead=fsptr;
	freereg *fhead;
	inode *root;
	direntry *rootfile;
	
	if(fshead->size==fssize/BLKSZ) return;
	
	fshead->ntsize=(BLOCKS_FILE*(1+NODES_BLOCK)+fssize/BLKSZ)/(1+BLOCKS_FILE*NODES_BLOCK);
	fshead->ndfree=(fshead->ntsize*NODES_BLOCK)-2;
	fshead->nodetbl=sizeof(inode);
	fshead->freelist=fshead->ntsize;
	fshead->free=fssize/BLKSZ-fshead->ntsize;
	
	fhead=(freereg*)O2P(fshead->freelist*BLKSZ);
	fhead->size=fshead->free;
	fhead->next=NULLOFF;
	
	root=(inode*)O2P(fshead->nodetbl);
	root->mode=DIRMODE;
	//set timestamps
	root->nlinks=1;
	root->blocklist=NULLOFF;
	root->nblocks=blkalloc(fsptr,1,root->blocks);
	root->blocks[1]=NULLOFF;
	root->size=root->nblocks*BLKSZ;
	
	rootfile=(direntry*)O2P(root->blocks[0]*BLKSZ);
	rootfile->node=NONODE;
	
	fshead->size=fssize/BLKSZ;
}

//Debug Functions===================================================================================

void printdirtree(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset dblk=nodetbl->blocks[0];
	
	
}

void printnodes(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t nodect,j,k;
	nodei i;
	
	if(fshead->size==0) return;
	
	nodect=(fshead->ntsize*NODES_BLOCK)-1;
	printf("Node table of %ld blocks with %ld entries\n",fshead->ntsize,nodect);
	for(i=0;i<nodect;i++){
		printf("\tNode %ld, ",i);
		if(nodetbl[i].nlinks){
			blkset offb=nodetbl[i].blocklist;
			if(nodetbl[i].mode==DIRMODE) printf("directory, ");
			else if(nodetbl[i].mode==FILEMODE) printf("regular file, ");
			else printf("mode not set, ");
			printf("%ld links, %ld bytes in %ld blocks\n",nodetbl[i].nlinks,nodetbl[i].size,nodetbl[i].nblocks);
			for(j=0;j<OFFS_NODE;j++){
				if(nodetbl[i].blocks[j]==NULLOFF) break;
				printf("\t\tBlock @ %ld in node list\n",nodetbl[i].blocks[j]);
			}for(k=0;offb!=NULLOFF;k++){
				offblock *oblk=O2P(offb*BLKSZ);
				for(j=0;j<BLKSZ-1;j++){
					printf("\t\tBlock @ %ld in offset block %ld\n",oblk->blocks[j],k);
					if(oblk->blocks[j]==NULLOFF) break;
				}offb=oblk->next;
			}
		}else{
			printf("empty\n");
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
	printnodes(fsptr);
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
