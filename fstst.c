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
#define FS_PAGES	2

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
	size_t nfree;
	sz_blk ntsize;
	blkset freelist;
	offset nodetbl;
} fsheader;

nodei newnode(void *fsptr)//find first free node
{
	
}

/*nodei path2node(void *fsptr, char* path)//find node corresponding to path
{
	fsheader *fshead=(fsheader*)fsptr;
	return NONODE;
}

nodei pathparent(void *fsptr, char* path)//find node of parent directory
{
	fsheader *fshead=(fsheader*)fsptr;
	return NONODE;
}

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
int frealloc()
{
	fsheader *fshead=(fsheader*)fsptr;
	//check if expand shink fits in last block of file
	return -1;
}*/

//multiple block allocation
int blkalloc(void *fsptr, size_t count, blkset *buf)//return #blocks allocated?
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *prev=NULL;
	size_t alloct=0;
	
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
void blkfree(void *fsptr, size_t count, blkset *buf)//clean
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	//end of fs?
	offsort(buf,buf,count);
	while(count--){
		//printf("cycle %ld, freeing block %ld\n",count,*buf);
		if(*buf!=NULLOFF){
			freereg *tmp=(freereg*)O2P(*buf*BLKSZ);
			if(freeoff==NULLOFF){
				//printf("empty freelist\n");
				tmp->next=fshead->freelist;
				fshead->freelist=*buf;
				tmp->size=1;
				freeoff=*buf;
			}else{
				freereg *fhead=O2P(freeoff*BLKSZ);
				if(fhead->next==NULLOFF || *buf<fhead->next){
					if(*buf==freeoff+fhead->size){
						//printf("free region extension\n");
						fhead->size++;
					}else{
						tmp->size=1;
						if(*buf<freeoff){
							//printf("block precedes freelist\n");
							tmp->next=fshead->freelist;
							fshead->freelist=*buf;
						}else{
							//printf("block between free regions\n");
							tmp->next=fhead->next;
							fhead->next=*buf;
						}fhead=tmp;
						freeoff=*buf;
					}if(fhead->next!=NULLOFF && freeoff+fhead->size==fhead->next){
						//printf("merging free regions\n");
						freereg *nxt=(freereg*)O2P(fhead->next*BLKSZ);
						fhead->next=nxt->next;
						fhead->size+=nxt->size;
					}
				}else{
					//printf("seeking nearest free region\n");
					freeoff=fhead->next;
					continue;
				}
			}*buf=NULLOFF;
			fshead->free++;
			//printf("tmp free region @ %p, block %ld, size %ld, next: %ld\n",tmp,P2O((void*)tmp)/BLKSZ,tmp->size,tmp->next);
		}//else printf("null block\n");
		buf++;
	}
}

//single block allocation
/*offset blkalloc(void *fsptr)
{
	fsheader *fsh=(fsheader*)fsptr;
	freereg *fhead;
	
	if(fsh->freelist==NULLOFF) return NULLOFF;
	fhead=(freereg*)O2P(fsh->freelist);
	//decrement free region size, shift header
	fsh->freelist=fhead->next;
	fsh->free--;
	return P2O(fhead);
}
void blkfree(void *fsptr, offset blk)
{
	fsheader *fsh=fsptr;
	freereg *fhead;
	
	if(blk==NULLOFF) return;
	if(fsh->freelist==NULLOFF){
		fsh->freelist=blk;
		fhead=(freereg*)O2P(blk);
	}else{
		if(blk<fsh->freelist)
			//if earliest, add as head
		//iterate freelist
			//if right before list entry, merge
			//if right after prev, merge
	}fsh->free++;
}*/

void fsinit(void *fsptr, size_t fssize)
{
	fsheader *fshead=fsptr;
	freereg *fhead;
	node *rootnode;
	
	if(fshead->size==fssize/BLKSZ) return;
	
	fshead->ntsize=(BLOCKS_FILE*(1+BLKSZ/sizeof(node))+fssize/BLKSZ)/(1+BLOCKS_FILE*BLKSZ/sizeof(node));
	//(1+BLKSZ/sizeof(node)+fssize/BLKSZ)/(1+BLKSZ/sizeof(node));
	fshead->nodetbl=sizeof(node);
	fshead->nfree=BLKSZ*fshead->ntsize/sizeof(node);
	fshead->freelist=fshead->ntsize;
	fshead->free=fssize/BLKSZ-fshead->ntsize;
	
	//initialize freelist head
	fhead=(freereg*)O2P(fshead->freelist*BLKSZ);
	fhead->size=fshead->free;
	fhead->next=NULLOFF;
	
	//initialize root node
	//rootnode=(node*)O2P(nodetbl);
	
	//initialize root dir
	
	fshead->size=fssize/BLKSZ;
}

//Debug Functions===================================================================================

void printfree(void *fsptr)
{
	fsheader *fshead=fsptr;
	freereg *frblk;
	blkset fblkoff;
	
	if(fshead->size==0) return;
	
	fblkoff=fshead->freelist;
	if(fblkoff==NULLOFF) printf("No free blocks\n");
	else printf("Freelist @ block %ld, total %ld blocks free\n",fblkoff,fshead->free);
	while(fblkoff!=NULLOFF){
		frblk=(freereg*)O2P(fblkoff*BLKSZ);
		printf("\tfree region @ block %ld, with %ld blocks\n",fblkoff,frblk->size);
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
	}printf("\tSize:%ld bytes in %ld blocks of size %ld\n",fsh->size*BLKSZ,fsh->size,BLKSZ);
	printf("\tNode table of %ld blocks with %ld entries\n",fsh->ntsize,fsh->ntsize*BLKSZ/sizeof(node)-1);
	printf("\tFree: %ld blocks, %ld bytes, first free block: %ld\n",fsh->free,fsh->free*BLKSZ,fsh->freelist);
}

int main()
{
	size_t fssize = PAGE_SIZE*FS_PAGES;
	void *fsptr = mmap(NULL, fssize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(fsptr==MAP_FAILED) return 1;
	blkset blks[3]={NULLOFF,NULLOFF,NULLOFF};
	
	fsinit(fsptr, fssize);
	printfs(fsptr);
	//printnodetbl(fsptr);
	printfree(fsptr);
	blkalloc(fsptr,3,blks);
	printfree(fsptr);
	blkfree(fsptr,2,blks);
	printfree(fsptr);
	blkfree(fsptr,3,blks);
	printfree(fsptr);
	
	munmap(fsptr, fssize);
	return 0;
}
