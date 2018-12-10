/*CSCE321 HW4: myfs
	fstst.c: debug/visualization and testing functions
*/

#include <sys/mman.h>
#define PAGE_SIZE	4096
#define FS_PAGES	1

#include "myfs_helper.h"

//DEBUG FUNCTIONS===================================================================================
void printpos(fpos pos)
{
	printf("Position %ld[%ld]->%ld[%ld]->%ld in block %ld of node %ld\n",
		pos.oblk,pos.opos,pos.dblk,pos.dpos,pos.data,pos.nblk,pos.node);
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

//TEST FUNCTIONS====================================================================================
void test_allocation(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset b[4];
	
	printfree(fsptr);
	printf("allocated: %ld", blkalloc(fsptr,4,b));
	printfree(fsptr);
	printf("freed: %ld", blkfree(fsptr,2,&(b[1])));
	printfree(fsptr);
}
void test_dirmod(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	printf("%ld\n",dirmod(fsptr,0,"tty1",1,NULL));
	nodetbl[1].mode=DIRMODE;
	printf("%ld\n",dirmod(fsptr,0,"tty2",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty3",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty4",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty5",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty6",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty7",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty8",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty9",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty10",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty11",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty12",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty13",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty14",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty15",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty16",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty17",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty18",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty19",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty20",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty21",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty22",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty23",1,NULL));
	printfs(fsptr);
}
void test_path2node(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	printf("%ld\n",dirmod(fsptr,0,"tty1",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty2",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty3",3,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty4",4,NULL));
	nodetbl[1].mode=DIRMODE;
	nodetbl[2].mode=DIRMODE;
	nodetbl[3].mode=DIRMODE;
	nodetbl[4].mode=DIRMODE;
	
	printf("%ld\n",dirmod(fsptr,1,"tty5",5,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty6",6,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty7",6,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty8",7,NULL));
	nodetbl[3].mode=DIRMODE;
	nodetbl[4].mode=DIRMODE;
	nodetbl[6].mode=DIRMODE;
	nodetbl[2].mode=DIRMODE;
	
	printf("%ld\n",dirmod(fsptr,3,"tty9",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty10",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty11",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty12",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty13",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty14",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty15",2,NULL));
	
	printf("%ld\n",dirmod(fsptr,6,"tty16",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty17",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty18",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty19",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty20",4,NULL));
	
	printf("%ld\n",dirmod(fsptr,2,"tty21",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty22",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty23",1,NULL));
	printfs(fsptr);
}
void test_frealloc(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	nodetbl[1].mode=DIRMODE;
	printf("%ld\n",dirmod(fsptr,0,"devolo",1,NULL));
	printf("%ld\n",dirmod(fsptr,1,"a",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"b",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"c",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"d",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"e",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"f",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"g",2,NULL));
	printf("%ld\n",dirmod(fsptr,1,"h",2,NULL));
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
	nodetbl[1].size*=sizeof(direntry);
	printfs(fsptr);
	printf("resize: %d\n",frealloc(fsptr,1,1*1024));
	printfs(fsptr);
	printf("resize: %d\n",frealloc(fsptr,1,2*1024));
	printfs(fsptr);
	printf("resize: %d\n",frealloc(fsptr,1,0*1024));
	printfs(fsptr);
}
void test_seek(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	fpos pos;
	
	printf("%ld\n",dirmod(fsptr,0,"tty1",1,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty2",2,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty3",3,NULL));
	printf("%ld\n",dirmod(fsptr,0,"tty4",4,NULL));
	
	printf("%ld\n",dirmod(fsptr,1,"tty5",5,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty6",6,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty7",6,NULL));
	printf("%ld\n",dirmod(fsptr,1,"tty8",7,NULL));
	
	printf("%ld\n",dirmod(fsptr,3,"tty9",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty10",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty11",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty12",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty13",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty14",2,NULL));
	printf("%ld\n",dirmod(fsptr,3,"tty15",2,NULL));
	
	printf("%ld\n",dirmod(fsptr,6,"tty16",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty17",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty18",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty19",2,NULL));
	printf("%ld\n",dirmod(fsptr,6,"tty20",4,NULL));
	
	printf("%ld\n",dirmod(fsptr,2,"tty21",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty22",1,NULL));
	printf("%ld\n",dirmod(fsptr,2,"tty23",1,NULL));
	printfs(fsptr);
	
	loadpos(fsptr,&pos,0);
	while(pos.data!=NULLOFF){
		printpos(pos);
		printf("advancement: %ld\n",seek(fsptr,&pos,2));
		//printf("advancement: %ld\n",advance(fsptr,&pos,nodetbl[0].nblocks));
	}
	printpos(pos);
}

int main()
{
	size_t fssize = PAGE_SIZE*FS_PAGES;
	void *fsptr = mmap(NULL, fssize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if(fsptr==MAP_FAILED) return 1;

	fsinit(fsptr,fssize);
	printfs(fsptr);

	//test_frealloc(fsptr);
	
	munmap(fsptr, fssize);
	return 0;
}
