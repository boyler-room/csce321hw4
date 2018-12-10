/*CSCE321 HW4: myfs
	fstst.c: debug/visualization and testing functions
*/

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
		freereg: free region header
		offblock: block with additional block offsets to data
		fpos: position file structure
		direntry: file entry in dir, contains name and node index, WIP
	Helper functions:
		fsinit() - check if the filesystem has been initialized, and do so if not
		blkalloc() - allocate the first count or fewer blocks, putting the offsets in buf, returns # blocks allocated
		blkfree() - free up to count blocks from the offsets in buf, uses offsort(heapsort) to sort buf first
		newnode() - finds the index of the first empty node
		nodegood() - determine if a nodei is within the allocated nodetbl
		nodevalid() - determine if a node refers to a valid file/directory
		namepatheq() - compare a segment of a path with a filename, like strcmp
		namepathset() - set a filename with a segment of a path, like strcpy
		path2node() - find the node of the file at path or its parent dir, if it exists
		dirmod() - find, rename, add, or remove file in directory
		loadpos() - initialize pos to the beginning of file at node, if valid
		advance() - advance pos by up to blk blocks and off entries/bytes
	Debug functions:
		printfs() - print global fs data
		printfree() - list free regions
		printnodes() - print node table entries
		printdir() - print directory tree
*/

#include <sys/mman.h>
#define PAGE_SIZE	4096
#define FS_PAGES	1

#include "myfs_helper.h"



// functions im working on
int __myfs_read_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *path, char *buf, size_t size, off_t off) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei node;
	fpos pos;
	size_t readct=0, nblks;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	printf("READ CALL\n");
	printfs(fsptr);
	
	if((node=path2node(fsptr,path,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(nodetbl[node].mode!=FILEMODE){
		*errnoptr=EISDIR;
		return -1;
	}if(size==0) return 0;

	 if(off>=nodetbl[node].size){ 
	// 	size_t offsize=MIN((off+BLKSZ-1)*BLKSZ,(off+size));
	 	printf("off >= node.size");
	 }
	while(pos.data!=NULLOFF && readct<size){
		printf("...\n");
		char* blk=B2P(pos.dblk);
		buf[readct++]=blk[pos.dpos];
		seek(fsptr,&pos,1);
	}printf("%ld bytes to go\n",size-writect);
	while(react<size){
		if(pos.data==NULLOFF){
			printf("pos.data == NULLOFF");

		}
		loadpos(fsptr,&pos,node);
		seek(fsptr,&pos,off+readct);
		char* blk=B2P(pos.dblk);
		buf[readct++]=blk[pos.dpos];
		seek(fsptr,&pos,1);
	}


	loadpos(fsptr,&pos,node);
	seek(fsptr,&pos,off);

	printf("name: %s, node: %ld\n",path,node);
	readct=0;
	while(readct<size){
		printf("%c",buf[readct++]);
	}printf("\n");
	return readct;

	// while(pos.data!=NULLOFF && readct<size){//TEMPORARY
	// 	buf[readct++]=((char*)B2P(pos.dblk))[pos.dpos];
	// 	seek(fsptr,&pos,1);
	// }return readct;

	/*
	uh=BLKSZ-pos.dpos, if not start of blk (%BLKSZ?)
	if(count<=uh) memcpy only count bytes, return count
	br=(count-uh)/BLKSZ
	oh=(count-uh)%BLKSZ
	advance to start of next block
	while(!eof && <br)
		if block not full/last block
			memcpy # in block
			readct+=#
			return readct
		memcpy dblk
		inc readct
		dec br
		advance to next block
	if eof return readct
	if last blk/not full && oh>avail
		oh=avail
	memcpy oh
	readct+=oh
	return readct
*/
	//check notdir
	//loadpos(fsptr,node,&pos);
	/*if(advance(fsptr,&pos,0,offset)<offset) return -1;
	while !eof && count<size
		copy byte to buffer
		inc count
		advance
	return count
	*/
  /* STUB */
  return -1;
}


int __myfs_write_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path, const char *buf, size_t size, off_t off) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei node;
	fpos pos;
	size_t writect=0, nblks;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	printf("WRITE CALL: %ld bytes @ offset %ld\n",size,off);
	printfs(fsptr);
	
	if((node=path2node(fsptr,path,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(size==0) return 0;
	
	if(off>=nodetbl[node].size){
		size_t offsize=MIN(((off+BLKSZ-1)/BLKSZ)*BLKSZ,(off+size));
		printf("start size: %ld from %ld past %ld\n",offsize,off,nodetbl[node].size);
		if(frealloc(fsptr,node,MIN(offsize,(off+size)))==-1){
			*errnoptr=EINVAL;
			return -1;
		}
	}loadpos(fsptr,&pos,node);
	seek(fsptr,&pos,off);
	while(pos.data!=NULLOFF && writect<size){
		printf("...\n");
		char* blk=B2P(pos.dblk);
		blk[pos.dpos]=buf[writect++];
		seek(fsptr,&pos,1);
	}printf("%ld bytes to go\n",size-writect);
	while(writect<size){
		if(pos.data==NULLOFF){
			size_t extsize=MIN((off+size),(nodetbl[node].nblocks+1)*BLKSZ);
			printf("extend size: %ld past %ld\n",extsize,nodetbl[node].size);
			if(frealloc(fsptr,node,extsize)==-1) break;
		}
		loadpos(fsptr,&pos,node);
		seek(fsptr,&pos,off+writect);
		char* blk=B2P(pos.dblk);
		blk[pos.dpos]=buf[writect++];
		seek(fsptr,&pos,1);
	}printfs(fsptr);
	/*
		//calc # blocks past end needed
		if>0
			//malloc,blkalloc up to calc'd, save actual alloc'd
			//for alloc'd blocks
				copy data to blocks
				insert blocks into file
				on fail, free remaining blocks
	*/
	//update size
	printf("name: %s, node: %ld\n",path,node);
	writect=0;
	while(writect<size){
		printf("%c",buf[writect++]);
	}printf("\n");
	return writect;
	/*
	while(pos.data!=NULLOFF && writect<size){//TEMPORARY
		printf("write: %c",buf[writect]);
		
	}return writect;*/
  /* STUB */
  return -1;
}











//debug functions

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
	blkdex block=0, entry=0;
	fpos pos;
	
	loadpos(fsptr,&pos,dir);
	while(pos.data!=NULLOFF){
		df=(direntry*)B2P(pos.dblk);
		if(df[pos.dpos].node==NONODE) break;
		for(int i=0;i<level;i++) printf("\t");
		printf("%s(%ld)\n",df[pos.dpos].name,df[pos.dpos].node);
		if(nodetbl[df[pos.dpos].node].mode==DIRMODE) printdir(fsptr,df[pos.dpos].node,level+1);
		seek(fsptr,&pos,1);
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
			size_t unit=1;
			if(nodetbl[i].mode==DIRMODE){
				printf("directory, ");
				unit=sizeof(direntry);
			}else if(nodetbl[i].mode==FILEMODE) printf("regular file, ");
			else printf("mode not set, ");
			printf("%ld links, %ld bytes in %ld blocks\n",nodetbl[i].nlinks,nodetbl[i].size*unit,nodetbl[i].nblocks);
			for(j=0;j<OFFS_NODE;j++){
				if(nodetbl[i].blocks[j]==NULLOFF) break;
				printf("\t\tBlock @ %ld in node list\n",nodetbl[i].blocks[j]);
			}for(k=0;offb!=NULLOFF;k++){
				offblock *oblk=B2P(offb);
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
		frblk=(freereg*)B2P(fblkoff);
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
	printf("/(0)\n");
	printdir(fsptr,0,1);
}

//test functions

void test_allocation();
void test_dirmod();
void test_path2node();
void test_frealloc();

int main()
{
	size_t fssize = PAGE_SIZE*FS_PAGES;
	void *fsptr = mmap(NULL, fssize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(fsptr==MAP_FAILED) return 1;
	//blkset b[4*FS_PAGES];
	fpos pos;

	fsinit(fsptr, fssize);
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	printfs(fsptr);
	
	nodetbl[1].mode=DIRMODE;
	printf("%ld\n",dirmod(fsptr,0,"devolo",1,NULL));
	//nodetbl[1].mode=FILEMODE;
	printf("%ld\n",dirmod(fsptr,1,"a",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"b",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"c",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"d",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"e",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"f",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"g",2,NULL));
	/*printf("%ld\n",dirmod(fsptr,1,"h",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"i",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"j",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"k",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"l",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"m",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"n",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"o",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"p",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"q",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"r",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"s",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"t",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"u",2,NULL));
	nodetbl[1].mode=FILEMODE;
	nodetbl[1].size*=sizeof(direntry);*//*
	printfs(fsptr);
	printf("resize: %d\n",frealloc(fsptr,1,1*1024));
	printfs(fsptr);
	printf("resize: %d\n",frealloc(fsptr,1,2*1024));
	printfs(fsptr);
	printf("resize: %d\n",frealloc(fsptr,1,0*1024));*/
	//printf("%ld\n",dirmod(fsptr,0,"dev",1,NULL));
	//printf("%ld\n",dirmod(fsptr,0,"etc",2,NULL));
	/*printf("%ld\n",dirmod(fsptr,0,"tty0",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty1",1,NULL));
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
	printf("%ld\n",dirmod(fsptr,0,"tty20",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty21",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty22",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty23",1,NULL));
	loadpos(fsptr,&pos,0);
	while(pos.data!=NULLOFF){
		printpos(pos);
		printf("advancement: %ld\n",seek(fsptr,&pos,2));
		//printf("advancement: %ld\n",advance(fsptr,&pos,nodetbl[0].nblocks));
	}
	printpos(pos);*/
	
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
	//printdir(fsptr,0,0);
	
	munmap(fsptr, fssize);
	return 0;
}
