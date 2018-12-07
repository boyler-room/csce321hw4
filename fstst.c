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
#define FS_PAGES	2

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

typedef size_t index;//list index
typedef size_t offset;//byte offset
typedef size_t blkset;//block offset
typedef size_t sz_blk;//block size
typedef ssize_t nodei;//node#, nodetbl index

typedef struct{
	nodei  node;//file node
	sz_blk nblk;//block # in file
	blkset oblk;//offset of offset block NULLOFF if in node.blocks
	size_t opos;//index in offset block/node.blocks
	blkset dblk;//offset of data block
	index  dpos;//byte of data in dblk for reg files, entry index for dirs 
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

//file size is #bytes requested
//file nblocks is #blocks of data
//dir size is #bytes in direntries
//dir nblocks is #blocks of dirfiles

/*
TODO:
	documentation
	modify dirmod/readdir to use fpos struct
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

//checks if a nodei refers to a possible nodetbl entry, 0 good
int nodegood(void *fsptr, nodei node)
{
	fsheader *fshead=(fsheader*)fsptr;
	return (node<0 || node>(fshead->ntsize*NODES_BLOCK-2));
}
//checks if a node refers to a valid file/dir, 0 valid
int nodevalid(void *fsptr, nodei node)
{
	fsheader *fshead=(fsheader*)fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	return (nodegood(fsptr,node) || nodetbl[node].nlinks==0 ||
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


swap(blkset *A, blkset *B)
{
	blkset t=*A;
	*A=*B; *B=t;
}
void filter(blkset *heap, size_t dex, size_t len)
{
	size_t ch0=2*dex+1,ch1=2*(dex+1);
	
	if(ch0<len){
		if(heap[ch0]>heap[dex] && (ch1>=len || heap[ch0]>=heap[ch1])){
			swap(&heap[dex],&heap[ch0]);
			filter(heap,ch0,len);
		}else if(ch1<len && heap[ch1]>heap[dex]){
			swap(&heap[dex],&heap[ch1]);
			filter(heap,ch1,len);
		}
	}
}
void offsort(blkset *data,size_t len)//heap sort
{
	size_t i=len;
	
	while(i) filter(data,--i,len);
	while(len){
		swap(data,&data[--len]);
		filter(data,0,len);
	}
}
int blkfree(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *fhead;
	sz_blk freect=0;
	
	offsort(buf,count);
	
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

//load pos with beginning position of node
nodei loadpos(void *fsptr, nodei node, fpos *pos)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	if(pos==NULL || nodevalid(fsptr,node)) return NONODE;
	
	pos->node=node;
	pos->nblk=0;
	pos->opos=0;
	pos->dpos=0;
	pos->oblk=NULLOFF;
	pos->dblk=*nodetbl[node].blocks;
	if(pos->dblk==NULLOFF)
		pos->data=NULLOFF;
	else
		pos->data=pos->dblk*BLKSZ;
	return node;
}

/*
	node==NONODE: invalid file
	oblk==NULLOFF: data block in node list of valid file
	dblk==NULLOFF: empty file
	data==NULLOFF: empty file or end of file
	
	nblk valid if node valid, file not empty
	oblk valid if node valid
	opos valid if node->oblk valid, not eof
	dblk valid if oblk->opos valid, file not empty
	dpos valid if dblk valid, not eof
	data valid if dblk->dpos valid
*/
/*cases
	bad file
		return error
	empty file
		leave fpos untouched
		return 0
	end-of-file
		leave fpos untouched
		return 0
	advance blocks only
		if next block null:
			advance entries to eof
			or: set dpos to max?max-1?, leave for entry loop
		else:
			move to next block
			advancement+=entries/block
			blk--;
	advance entries only
		
	advance both
condition going into entry loop
	dblk is last dblk
	dpos is zero
	data is dblk[dpos]
*/
//check file size for last byte/eof
//move fpos up to blk blocks and off entries/bytes forward in the dir/file, return actual advancement, -1 if failed
off_t advance(void *fsptr, fpos *pos, sz_blk blk, off_t off)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	off_t advancement=0;
	size_t unit;
	
	if(pos==NULL || nodevalid(fsptr,pos->node)) return -1;
	if(pos->data==NULLOFF || off<0) return 0;
	if(nodetbl[pos->node].mode==DIRMODE){
		unit=sizeof(direntry);
	}else unit=1;
	
	if((blk+=(off+pos->dpos)*unit/BLKSZ)>0){
		off=(off+pos->dpos)%(BLKSZ/unit);
		advancement-=pos->dpos;
		pos->dpos=0;
		pos->data=pos->dblk*BLKSZ;
	}//printf("advancing %ld blocks, %ld entries\n",blk,off);
	while(blk>0){
		pos->opos++;
		if(pos->oblk==NULLOFF){
			if(pos->opos==OFFS_NODE){
				if((pos->oblk=nodetbl[pos->node].blocklist)==NULLOFF){
					off=BLKSZ/unit;
					break;
				}offblock *offs=O2P(pos->oblk*BLKSZ);
				pos->dblk=offs->blocks[pos->opos=0];
			}else{
				if(nodetbl[pos->node].blocks[pos->opos]==NULLOFF){
					off=BLKSZ/unit;
					break;
				}pos->dblk=nodetbl[pos->node].blocks[pos->opos];
			}
		}else{
			offblock *offs=O2P(pos->oblk*BLKSZ);
			if(pos->opos==OFFS_BLOCK-1){
				if(offs->next==NULLOFF){
					off=BLKSZ/unit;
					break;
				}pos->oblk=offs->next;
				offs=(offblock*)O2P(pos->oblk);
				pos->dblk=offs->blocks[pos->opos=0];
			}else{
				if(offs->blocks[pos->opos]==NULLOFF){
					off=BLKSZ/unit;
					break;
				}pos->dblk=offs->blocks[pos->opos];
			}
		}pos->data=pos->dblk*BLKSZ;
		advancement+=BLKSZ/unit;
		pos->nblk++;
		blk--;
	}while(pos->data!=NULLOFF && off>0){
		pos->dpos++;
		if(pos->dpos==BLKSZ/unit || (pos->nblk*BLKSZ+pos->dpos*unit)>=nodetbl[pos->node].size){
			pos->data=NULLOFF;
		}else{
			pos->data=pos->dblk*BLKSZ+pos->dpos*unit;
			advancement++;
			off--;
		}
	}return advancement;
}

int frealloc(void *fsptr, nodei node, off_t size)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t unit;
	fpos pos;
	
	ssize_t sdiff;
	
	if(size<0 || loadpos(fsptr,node,&pos)==-1) return -1;
	if(nodetbl[pos.node].mode==DIRMODE){
		unit=sizeof(direntry);
	}else unit=1;
	sdiff=(size*unit+BLKSZ-1)/BLKSZ-nodetbl[node].nblocks;
	
	if(sdiff>0){//need to allocate
		if(sdiff>fshead->free) return -1;
		advance(fsptr,&pos,0,nodetbl[node].size/unit);
		if(pos.dpos<BLKSZ/unit){
			//memcpy
		}
		//if not eoblk, zero entries at end of block
		//if oblk not full, alloc min sdiff,full
		//while sdiff, alloc
		//advance(fsptr,&pos,0,nodetbl[node].size)
		//zero
	}else if(sdiff<0){//need to free
		advance(fsptr,&pos,nodetbl[node].nblocks-sdiff,0);
		//free blocks after
		//free oblk if empty, load next
	}nodetbl[node].nblocks+=sdiff;
	nodetbl[node].size=size*unit;
	return 0;
}
/*post-advancement conditions
	empty file
		node valid
		nblk 0
		oblk null
		opos 0
		dblk null
		dpos 0
		data null
	reached target entry
		node valid
		nblk # of block w/ entry
		oblk null or offblock w/ dblk
		opos pos of dblk in oblk
		dblk block w/ entry
		dpos pos of entry in dblk
		data offset to entry
	end of file:
		past last entry: no empty oblks, no empty dblks: if dpos past last, dpos>0,
				last @ dpos--, dblk @ opos-- if dpos max
		node valid
		nblk # block w/ last entry
		oblk null or offblock w/ dblk
		opos past pos of dblk in oblk (max on full)? even if dblk not full?
		dblk block w/ last entry
		dpos past last (max on full)
		data null
	therefore:
		node null ==> bad file
		oblk null ==> dblk in node list
		dblk null ==> empty file
		data null ==> end of file/empty file
		opos max ==> eof, oblk full
		dpos max ==> eof, dblk full
*/

/*dirmod ops
	while !EOF
		check match
		advance 1 entry
	if mode find: not found
	if mode rename:
		if not found: not found
		rename
	if mode remove:
		if not found: not found
		get last entry: opos--, dpos--
		overwrite found, null last
		if last was first in dblk
			free dblk from node list or oblk
			if dblk was last in oblk
				get prev oblk or node list (need prev oblk var)
				free oblk from prev/node list
	if mode add:
		if empty file or dblk full
			if no oblks
				if opos at end
					new oblk to node list
					new dblk to oblk
				else
					new dblk to node list
			else
				if opos at end
					new oblk to oblk next
				new dblk to oblk
		else
			add entry to dblk
*/

//invalid/overlong names are truncated
/*
	node	rename	function
	 NONODE	 NULL	 find node of file name, return node if found
	 NONODE	 rename	 find file name and change name to rename if rename does not exist, return node on success
	 node	 NULL	 add the file name if it does not exist and link it to node, return node on success
	 !NONODE !NULL	 find file name and remove it, unlink node and free if needed, return node on success
*/
/*nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	direntry *df, *found=NULL;
	blkset prevo=NULLOFF;
	fpos pos;
	
	if(nodevalid(fsptr,dir) || nodetbl[dir].mode!=DIRMODE) return NONODE;
	if(node!=NONODE && rename==NULL && nodegood(fsptr,node)) return NONODE;
	if(*name=='\0' || (rename!=NULL && node==NONODE && *rename=='\0')) return NONODE;
	
	loadpos(fsptr,dir,&pos);	
	while(pos.data!=NULLOFF){
		df=(direntry*)O2P(pos.dblk*BLKSZ);
		if(df[pos.dpos].node==NONODE) break;
		if(node==NONODE && rename!=NULL && namepatheq(df[pos.dpos].name,rename)){
			return NONODE;
		}if(namepatheq(df[pos.dpos].name,name)){
			if(rename!=NULL) found=&df[pos.dpos];
			else{
				if(node==NONODE) return df[pos.dpos].node;
				else return NONODE;
			}
		}prevo=pos.oblk;
		advance(fsptr,&pos,0,1);
		//printf("prev: %ld, next: %ld, data: %ld\n",prevo,pos.opos,pos.data);
	}
	/*	if(oblk==NULLOFF){
			if(block==OFFS_NODE){
				if((oblk=nodetbl[dir].blocklist)==NULLOFF){
					dblk=NULLOFF;
				}else{
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
	*//*
	if(node==NONODE){
		if(rename!=NULL && found!=NULL){
			namepathset(found->name,rename);
			return found->node;
		}return NONODE;
	}if(rename!=NULL){
		if(found==NULL) return NONODE;
		node=found->node;
		if(pos.data!=NULLOFF) pos.dpos--;
		else{
			if(pos.oblk==NULLOFF){
				pos.dblk=nodetbl[dir].blocks[--(pos.opos)];
			}else{
				offblock *offs=O2P(pos.oblk*BLKSZ);
				pos.dblk=offs->blocks[--(pos.opos)];
			}pos.dpos=FILES_DIR-1;
		}df=(direntry*)O2P(pos.dblk*BLKSZ);
		found->node=df[pos.dpos].node;
		namepathset(found->name,df[pos.dpos].name);
		df[pos.dpos].node=NONODE;
		if(pos.dpos==0){
			if(pos.oblk==NULLOFF){
				blkfree(fsptr,1,&(nodetbl[dir].blocks[pos.opos]));
			}else{
				offblock *offs=O2P(pos.oblk*BLKSZ);
				blkfree(fsptr,1,&(offs->blocks[pos.opos]));
				if(pos.opos==0){
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
		if(nodetbl[node].nlinks==1){
			frealloc(fsptr,node,0);
		}nodetbl[node].nlinks--;
		return node;
	}if(pos.data==NULLOFF){
		offblock *offs;
		if(pos.oblk==NULLOFF){
			if(pos.opos==OFFS_NODE){
				if(blkalloc(fsptr,1,&(pos.oblk))==0){
					return NONODE;
				}if(blkalloc(fsptr,1,&(pos.dblk))==0){
					blkfree(fsptr,1,&(pos.oblk));
					return NONODE;
				}nodetbl[dir].blocklist=pos.oblk;
				offs=(offblock*)O2P(pos.oblk*BLKSZ);
				offs->blocks[0]=pos.dblk;
				offs->blocks[1]=NULLOFF;
				offs->next=NULLOFF;
			}else{
				if(blkalloc(fsptr,1,&(pos.dblk))==0){
					//errno
					return NONODE;
				}nodetbl[dir].blocks[pos.opos]=pos.dblk;
				if(pos.opos<OFFS_NODE-1) nodetbl[dir].blocks[pos.opos+1]=NULLOFF;
			}
		}else{
			offs=(offblock*)O2P(pos.oblk*BLKSZ);
			if(pos.opos==OFFS_BLOCK-1){
				if(blkalloc(fsptr,1,&(pos.oblk))==0){
					//errno
					return NONODE;
				}if(blkalloc(fsptr,1,&(pos.dblk))==0){
					//errno
					blkfree(fsptr,1,&(pos.oblk));
					return NONODE;
				}offs->next=pos.oblk;
				offs=(offblock*)O2P(pos.oblk*BLKSZ);
				offs->next=NULLOFF;
				pos.opos=0;
			}else{
				if(blkalloc(fsptr,1,&(pos.dblk))==0){
					//errno
					return NONODE;
				}
			}offs->blocks[pos.opos]=pos.dblk;
			offs->blocks[1]=NULLOFF;
		}nodetbl[dir].nblocks++;
		df=(direntry*)O2P(pos.dblk*BLKSZ);
	}printf("add %s to node %ld\noblk: %ld,opos: %ld,dblk: %ld,dpos: %ld\n",
		name,node,pos.oblk,pos.opos,pos.dblk,pos.dpos);
	nodetbl[dir].size+=sizeof(direntry);
	df[pos.dpos].node=node;
	namepathset(df[pos.dpos].name,name);
	nodetbl[node].nlinks++;
	if(++(pos.dpos)<FILES_DIR) df[pos.dpos].node=NONODE;
	return node;
}*/
nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, prevo=NULLOFF;
	blkset dblk=nodetbl[dir].blocks[0];
	direntry *df, *found=NULL;
	index block=0, entry=0;
	
	if(nodevalid(fsptr,dir) || nodetbl[dir].mode!=DIRMODE) return NONODE;
	if(node!=NONODE && rename==NULL && nodegood(fsptr,node)) return NONODE;
	if(*name=='\0' || (rename!=NULL && node==NONODE && *rename=='\0')) return NONODE;
	
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
				if((oblk=nodetbl[dir].blocklist)==NULLOFF){
					dblk=NULLOFF;
				}else{
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
		if(nodetbl[node].nlinks==1){
			frealloc(fsptr,node,0);
		}nodetbl[node].nlinks--;
		return node;
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

//find node corresponding to path, if child!=NULL, return node of path's parent dir and set *child to the filename
nodei path2node(void *fsptr, const char *path, char **child)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	nodei node=0;
	index sub=1, ch=1;
	
	if(path[0]!='/') return NONODE;
	
	while(path[sub=ch]!='\0'){
		while(path[ch]!='\0'){
			if(path[ch++]=='/') break;
		}if(child!=NULL && path[ch]=='\0'){
			*child=&path[sub];
			break;
		}if((node=dirmod(fsptr,node,&path[sub],NONODE,NULL))==NONODE)
			return NONODE;
	}return node;
}

//Debug Functions===================================================================================

void printpos(fpos pos)
{
	printf("Position %ld[%ld]->%ld[%ld]->%ld in block %ld of node %ld\n",pos.oblk,pos.opos,pos.dblk,pos.dpos,pos.data,pos.nblk,pos.node);
}

void printdir(void *fsptr, nodei dir, size_t level)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, dblk=nodetbl[dir].blocks[0];
	direntry *df;
	index block=0, entry=0;
	fpos pos;
	
	loadpos(fsptr,dir,&pos);
	while(pos.data!=NULLOFF){
		df=(direntry*)O2P(pos.dblk*BLKSZ);
		if(df[pos.dpos].node==NONODE) break;
		for(int i=0;i<level;i++) printf("\t");
		printf("%s(%ld)\n",df[pos.dpos].name,df[pos.dpos].node);
		if(nodetbl[df[pos.dpos].node].mode==DIRMODE) printdir(fsptr,df[pos.dpos].node,level+1);
		advance(fsptr,&pos,0,1);
	}
	/*while(dblk!=NULLOFF){
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
	}*/
}

void printnodes(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t nodect;
	index j,k;
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
					if(oblk->blocks[j]==NULLOFF) break;
					printf("\t\tBlock @ %ld in offset block %ld @ %ld\n",oblk->blocks[j],k,offb);
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
	fpos pos;

	fsinit(fsptr, fssize);
	fsheader *fshead=fsptr;inode *nodetbl=O2P(fshead->nodetbl);
	
	nodetbl[1].mode=DIRMODE;
	//printf("%ld\n",dirmod(fsptr,0,"devolo",1,NULL));
	nodetbl[2].mode=DIRMODE;
	printfs(fsptr);
	//printf("%ld\n",dirmod(fsptr,0,"dev",1,NULL));
	//printf("%ld\n",dirmod(fsptr,0,"etc",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty0",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty1",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty2",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty3",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty4",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty5",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty6",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty7",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty8",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty9",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty10",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty11",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty12",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty13",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty14",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty15",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty16",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty17",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty18",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty19",2,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty20",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty21",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty22",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty23",1,NULL));
	loadpos(fsptr,0,&pos);
	//while(pos.data!=NULLOFF){
		printpos(pos);
		printf("advancement: %ld\n",advance(fsptr,&pos,0,nodetbl[0].size/sizeof(direntry)));
	//}
	printpos(pos);
	
	//printf("%ld\n",dirmod(fsptr,1,"input",2,NULL));
	//printf("%ld\n",dirmod(fsptr,1,"input",0,""));
	printfs(fsptr);
	/*printf("%ld\n",dirmod(fsptr,0,"dev",1,NULL));
	printf("%ld\n",dirmod(fsptr,1,"craw",2,NULL));
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
	printf("%ld\n",path2node(fsptr,"/dev",NULL));
	printf("%ld\n",path2node(fsptr,"/jim",NULL));*/
	printdir(fsptr,0,0);
	
	munmap(fsptr, fssize);
	return 0;
}
