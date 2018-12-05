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
#define FILES_DIR	(BLKSZ/sizeof(direntry))
#define OFFS_BLOCK	(BLKSZ/sizeof(blkset))
#define OFFS_NODE	5//#block offsets in inode; such that sizeof(inode) is 16 words
#define BLOCKS_FILE	4//average blocks per file
#define FILEMODE	(S_IFREG|0755)
#define DIRMODE		(S_IFDIR|0755)

typedef size_t offset;
typedef size_t blkset;
typedef size_t sz_blk;
typedef ssize_t nodei;

typedef struct{
	blkset oblk;
	size_t opos;
	blkset dblk;
	size_t dpos;
	offset data;
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

//file size is #bytes requested
//file nblocks is #blocks of data?
//dir size is #bytes in direntries
//dir nblocks is #blocks of dirfiles?

/*
TODO:
	documentation
	modify dirmod/readdir to use fpos struct
	in-place offsort
	better path2parent
	frealloc
harder
	read
	write
*/

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


int nodevalid(void *fsptr, nodei node)//0 valid
{
	fsheader *fshead=(fsheader*)fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	return (node<0 || node>=(fshead->ntsize*NODES_BLOCK-1) ||
		(nodetbl[node].mode!=DIRMODE && nodetbl[node].mode!=FILEMODE));
}

void namepathset(char *name, const char *path)
{
	size_t len=0;
	if(name==path) return;
	while(path[len]!='/' && path[len]!='\0'){
		name[len]=path[len];
		if(++len==NAMELEN-1) break;
	}name[len]='\0';
}

int namepatheq(char *name, const char *path)
{
	size_t len=0;
	if(name==path) return 1;
	while(path[len]!='/' && path[len]!='\0'){
		if(name[len]=='\0' || name[len]!=path[len]) return 0;
		if(++len==NAMELEN) return 1;
	}return (name[len]=='\0');
}

char* pathsplit(const char *path)
{
	size_t cur=0;
	while(path[cur]!='\0'){
		if(path[cur++]=='/'){
			path=&path[cur];
			cur=0;
		}
	}return path;
}
/*

//convert path to linked list?
nodei subpathnode(void *fsptr, nodei node, char* subpath)
{
	if(subpath[0]=='\0') return node;
	//find first subpath entry in
}
*/

int fsinit(void *fsptr, size_t fssize)
{
	fsheader *fshead=fsptr;
	freereg *fhead;
	inode *nodetbl;
	nodei node;
	struct timespec creation;
	
	if(fssize<2*BLKSZ) return -1;
	if(fshead->size==fssize/BLKSZ) return 0;
	if(timespec_get(&creation,TIME_UTC)!=TIME_UTC) return -1;
	
	fshead->ntsize=(BLOCKS_FILE*(1+NODES_BLOCK)+fssize/BLKSZ)/(1+BLOCKS_FILE*NODES_BLOCK);
	fshead->nodetbl=sizeof(inode);
	fshead->freelist=fshead->ntsize;
	fshead->free=fssize/BLKSZ-fshead->ntsize;
	
	fhead=(freereg*)O2P(fshead->freelist*BLKSZ);
	fhead->size=fshead->free;
	fhead->next=NULLOFF;
	
	nodetbl=(inode*)O2P(fshead->nodetbl);
	nodetbl[0].mode=DIRMODE;
	nodetbl[0].ctime=creation;
	nodetbl[0].mtime=creation;
	nodetbl[0].nlinks=1;
	nodetbl[0].size=0;
	nodetbl[0].nblocks=0;
	nodetbl[0].blocks[0]=NULLOFF;
	nodetbl[0].blocklist=NULLOFF;
	for(node=1;node<fshead->ntsize*NODES_BLOCK-1;node++){
		nodetbl[node].nlinks=0;
	}
	
	fshead->size=fssize/BLKSZ;
	return 0;
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

void offsort(blkset *data, blkset *work, size_t len)
{
	blkset odd, *even=data;
	size_t i,j,k;
	int oddf=len%2;
	
	if(len<2) return;
	if(oddf) odd=*(even++);
	
	offsort(even+len/2,work,len/2);
	for(i=0;i<len/2;i++) work[i]=even[i];
	offsort(work,work+len/2,len/2);
	
	for(k=0,i=0,j=0;k<len;k++){
		if(oddf && (j>=len/2 || odd<=work[j]) && (i>=len/2 || odd<=even[i+len/2])){
			data[k]=odd;
			oddf=0;
		}else{
			if(i>=len/2 || (j<len/2 && work[j]<=even[i+len/2])) data[k]=work[j++];
			else data[k]=even[(i++)+len/2];
		}
	}
}
int blkfree(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *fhead;
	sz_blk freect=0;
	
	blkset *tmp=malloc(count*sizeof(blkset));
	if(tmp==NULL) return 0;
	offsort(buf,tmp,count);//replace with nlogn inplace sort to regain void status
	free(tmp);
	
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
	return freect;
}

int frealloc(void *fsptr, nodei node, off_t size)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	fpos pos={};
	
	ssize_t sdiff;
	
	if(nodevalid(node)) return -1;
	sdiff=(size+BLKSZ-1)/BLKSZ-file->nblocks;
	
	if(sdiff>0){
		if(sdiff>fshead->free) return -1;
		//seek end of blocklist
		//alloc
		//zero
	}else if(sdiff<0){
		//find last sdiff blocks
		//free
	}nodetbl[node].nblocks+=sdiff;
	nodetbl[node].size=size;
	return 0;
}

/*
int fseek(void *fsptr, nodei node, off_t off)
{
	
}
*/

//invalid/overlong names are truncated
/*
	node	rename	function
	 NONODE	 NULL	 find node of file name, return node if found
	 NONODE	 rename	 find file name and change name to rename if rename does not exist, return node on success
	 node	 NULL	 add the file name if it does not exist and link it to node, return node on success
	 !NONODE !NULL	 find file name and remove it, unlink node and free if needed, return node on success
*/
nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, prevo=NULLOFF;
	blkset dblk=nodetbl[dir].blocks[0];
	direntry *df, *found=NULL;
	size_t block=0, entry=0;
	
	if(nodevalid(fsptr,dir) || nodetbl[dir].mode!=DIRMODE) return NONODE;
	//if(node!=NONODE && rename==NULL && nodevalid(fsptr,node)) return NONODE;
	
	while(dblk!=NULLOFF){
		df=(direntry*)O2P(dblk*BLKSZ);
		while(entry<FILES_DIR){
			if(df[entry].node==NONODE) break;
			if(node==NONODE && rename!=NULL && namepatheq(df[entry].name,rename)){
				return NONODE;
			}if(namepatheq(df[entry].name,name)){
				if(rename!=NULL) found=&df[entry];
				else{
					if(node==NONODE) return df[entry].node;
					else return NONODE;
				}
			}entry++;
		}if(entry<FILES_DIR) break;
		block++; entry=0;
		if(oblk==NULLOFF){
			if(block==OFFS_NODE){
				oblk=nodetbl[dir].blocklist;
				if(oblk==NULLOFF) dblk=NULLOFF;
				else{
					offblock *offs=O2P(oblk*BLKSZ);
					dblk=offs->blocks[block=0];
				}
			}else dblk=nodetbl[dir].blocks[block];
		}else{
			offblock *offs=O2P(oblk*BLKSZ);
			if(block==OFFS_BLOCK-1){
				if(offs->next==NULLOFF) dblk=NULLOFF;
				else{
					prevo=oblk;
					oblk=offs->next;
					offs=(offblock*)O2P(oblk*BLKSZ);
					dblk=offs->blocks[block=0];
				}
			}else dblk=offs->blocks[block];
		}
	}if(node==NONODE){
		if(rename!=NULL && found!=NULL){
			namepathset(found->name,rename);
			return found->node;
		}return NONODE;
	}if(rename!=NULL){
		if(found==NULL) return NONODE;
		node=found->node;
		if(dblk!=NULLOFF) entry--;
		else{
			if(oblk==NULLOFF){
				dblk=nodetbl[dir].blocks[--block];
			}else{
				offblock *offs=O2P(oblk*BLKSZ);
				dblk=offs->blocks[--block];
			}entry=FILES_DIR-1;
		}df=(direntry*)O2P(dblk*BLKSZ);
		found->node=df[entry].node;
		namepathset(found->name,df[entry].name);
		df[entry].node=NONODE;
		if(entry==0){
			if(oblk==NULLOFF){
				blkfree(fsptr,1,&(nodetbl[dir].blocks[block]));
			}else{
				offblock *offs=O2P(oblk*BLKSZ);
				blkfree(fsptr,1,&(offs->blocks[block]));
				if(block==0){
					if(prevo==NULLOFF){
						blkfree(fsptr,1,&(nodetbl[dir].blocklist));
					}else{
						offs=(offblock*)O2P(prevo*BLKSZ);
						blkfree(fsptr,1,&(offs->next));
					}
				}
			}nodetbl[dir].nblocks--;
		}nodetbl[dir].size-=sizeof(direntry);
		//update dir node times?
		if(--(nodetbl[node].nlinks)==0){
			frealloc(fsptr,node,0);
		}return node;
	}if(dblk==NULLOFF){
		offblock *offs;
		if(oblk==NULLOFF){
			if(block==OFFS_NODE){
				if(blkalloc(fsptr,1,&oblk)==0){
					//errno
					return NONODE;
				}if(blkalloc(fsptr,1,&dblk)==0){
					//errno
					blkfree(fsptr,1,&oblk);
					return NONODE;
				}nodetbl[dir].blocklist=oblk;
				offs=(offblock*)O2P(oblk*BLKSZ);
				offs->blocks[0]=dblk;
				offs->blocks[1]=NULLOFF;
				offs->next=NULLOFF;
			}else{
				if(blkalloc(fsptr,1,&dblk)==0){
					//errno
					return NONODE;
				}nodetbl[dir].blocks[block]=dblk;
				if(block<OFFS_NODE-1) nodetbl[dir].blocks[block+1]=NULLOFF;
			}
		}else{
			offs=(offblock*)O2P(oblk*BLKSZ);
			if(block==OFFS_BLOCK-1){
				if(blkalloc(fsptr,1,&oblk)==0){
					//errno
					return NONODE;
				}if(blkalloc(fsptr,1,&dblk)==0){
					//errno
					blkfree(fsptr,1,&oblk);
					return NONODE;
				}offs->next=oblk;
				offs=(offblock*)O2P(oblk*BLKSZ);
				offs->next=NULLOFF;
				block=0;
			}else{
				if(blkalloc(fsptr,1,&dblk)==0){
					//errno
					return NONODE;
				}
			}offs->blocks[block]=dblk;
			offs->blocks[1]=NULLOFF;
		}nodetbl[dir].nblocks++;
		df=(direntry*)O2P(dblk*BLKSZ);
	}nodetbl[dir].size+=sizeof(direntry);
	df[entry].node=node;
	namepathset(df[entry].name,name);
	nodetbl[node].nlinks++;
	if(++entry<FILES_DIR) df[entry].node=NONODE;
	return node;
}

/*
	recursive path2node:
	wrapper: check root, pass in root node, address of pcount
	pathwalk(fsptr,parent,path,&pcount)
		load/check next in path, on '\0':
			ret parent
		dirmod find child
		if NONODE: ret NONODE
		node=pathwalk(fsptr,child,post-path,&pcount)
		if(pcount) pcount--, return parent
		else return node
*/
nodei path2node(void *fsptr, const char *path)//, int parent)//find node corresponding to path or up to parentth parent dir
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	nodei node=0;
	size_t sub=1;
	
	if(path[0]!='/') return NONODE;
	
	while(path[sub]!='\0'){
		node=dirmod(fsptr,node,&path[sub],NONODE,NULL);
		if(node==NONODE) return NONODE;
		while(path[sub]!='\0'){
			if(path[sub++]=='/') break;
		}
	}return node;
}
nodei path2parent(void *fsptr, const char *path)//, int parent)//find node corresponding to path or up to parentth parent dir
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	char name[NAMELEN];
	nodei node=0;
	size_t sub=1;
	
	if(path[0]!='/') return NONODE;
	
	while(path[sub]!='\0'){
		namepathset(name,&path[sub]);
		while(path[sub]!='\0'){
			if(path[sub++]=='/') break;
		}if(path[sub]=='\0') return node;
		node=dirmod(fsptr,node,name,NONODE,NULL);
		if(node==NONODE) return NONODE;
	}return node;
}

//Debug Functions===================================================================================

void printdir(void *fsptr, nodei dir, size_t level)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, dblk=nodetbl[dir].blocks[0];
	direntry *df;
	size_t block=0, entry=0;
	
	while(dblk!=NULLOFF){
		df=(direntry*)O2P(dblk*BLKSZ);
		while(entry<FILES_DIR){
			if(df[entry].node==NONODE) break;
			for(int i=0;i<level;i++) printf("\t");
			printf("%s(%ld)\n",df[entry].name,df[entry].node);
			if(nodetbl[df[entry].node].mode==DIRMODE) printdir(fsptr,df[entry].node,level+1);
			entry++;
		}if(entry<FILES_DIR) break;
		block++; entry=0;
		if(oblk==NULLOFF){
			if(block==OFFS_NODE){
				oblk=nodetbl[dir].blocklist;
				if(oblk==NULLOFF) dblk=NULLOFF;
				else{
					offblock *offs=O2P(oblk*BLKSZ);
					dblk=offs->blocks[block=0];
				}
			}else dblk=nodetbl[dir].blocks[block];
		}else{
			offblock *offs=O2P(oblk*BLKSZ);
			oblk=offs->next;
			if(block==OFFS_BLOCK-1){
				if(oblk==NULLOFF) dblk=NULLOFF;
				else dblk=((offblock*)O2P(oblk*BLKSZ))->blocks[block=0];
			}else dblk=offs->blocks[block];
		}
	}
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
	blkset a[8];

	fsinit(fsptr, fssize);
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	printfs(fsptr);
	printf("%ld\n",dirmod(fsptr,0,"dev",1,NULL));
	nodetbl[1].mode=DIRMODE;
	printf("%ld\n",dirmod(fsptr,1,"craw",2,NULL));
	printdir(fsptr,0,0);
	printfs(fsptr);
	//printf("%ld\n",dirmod(fsptr,1,"jimbo",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"craw",0,"a"));
	//printf("%ld\n",dirmod(fsptr,1,"devf",0,""));
	//printf("%ld\n",dirmod(fsptr,1,"jimbo",NONODE,"craws"));
	printdir(fsptr,0,0);
	//printf("%ld\n",dirmod(fsptr,0,"loop",NONODE,NULL));
	//printf("%ld\n",dirmod(fsptr,1,"froot",2,NULL));
	//printf("%ld\n",dirmod(fsptr,1,"loop",NONODE,NULL));
	printfs(fsptr);
	printf("%ld\n",path2node(fsptr,"/dev"));
	printf("%ld\n",path2parent(fsptr,"/jim"));
	
	munmap(fsptr, fssize);
	return 0;
}
