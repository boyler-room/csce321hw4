/*

  MyFS: a tiny file-system written for educational purposes

  MyFS is 

  Copyright 2018 by

  University of Alaska Anchorage, College of Engineering.

  Contributors: Christoph Lauter
                Chandra Boyle
                Devin Boyle and
                Derek Crain

  and based on 

  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs

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


/* The filesystem you implement must support all the 13 operations
   stubbed out below. There need no be support for access rights,
   links, symbolic links. There needs to be support for access and
   modification times and information for statfs.

   The filesystem must run in memory, using the memory of size 
   fssize pointed to by fsptr. The memory comes from mmap and 
   is backed with a file if a backup-file is indicated. When
   the filesystem is unmounted, the memory is written back to 
   that backup-file. When the filesystem is mounted again from
   the backup-file, the same memory appears at the newly mapped
   in virtual address. The filesystem datastructures hence must not
   store any pointer directly to the memory pointed to by fsptr; it
   must rather store offsets from the beginning of the memory region.

   When a filesystem is mounted for the first time, the whole memory
   region of size fssize pointed to by fsptr reads as zero-bytes. When
   a backup-file is used and the filesystem is mounted again, certain
   parts of the memory, which have previously been written, may read
   as non-zero bytes. The size of the memory region is at least 2048
   bytes.

   CAUTION:

   * You MUST NOT use any global variables in your program for reasons
   due to the way FUSE is designed.

   You can find ways to store a structure containing all "global" data
   at the start of the memory region representing the filesystem.

   * You MUST NOT store (the value of) pointers into the memory region
   that represents the filesystem. Pointers are virtual memory
   addresses and these addresses are ephemeral. Everything will seem
   okay UNTIL you remount the filesystem again.

   You may store offsets/indices (of type size_t) into the
   filesystem. These offsets/indices are like pointers: instead of
   storing the pointer, you store how far it is away from the start of
   the memory region. You may want to define a type for your offsets
   and to write two functions that can convert from pointers to
   offsets and vice versa.

   * You may use any function out of libc for your filesystem,
   including (but not limited to) malloc, calloc, free, strdup,
   strlen, strncpy, strchr, strrchr, memset, memcpy. However, your
   filesystem MUST NOT depend on memory outside of the filesystem
   memory region. Only this part of the virtual memory address space
   gets saved into the backup-file. As a matter of course, your FUSE
   process, which implements the filesystem, MUST NOT leak memory: be
   careful in particular not to leak tiny amounts of memory that
   accumulate over time. In a working setup, a FUSE process is
   supposed to run for a long time!

   It is possible to check for memory leaks by running the FUSE
   process inside valgrind:

   valgrind --leak-check=full ./myfs --backupfile=test.myfs ~/fuse-mnt/ -f

   However, the analysis of the leak indications displayed by valgrind
   is difficult as libfuse contains some small memory leaks (which do
   not accumulate over time). We cannot (easily) fix these memory
   leaks inside libfuse.

   * Avoid putting debug messages into the code. You may use fprintf
   for debugging purposes but they should all go away in the final
   version of the code. Using gdb is more professional, though.

   * You MUST NOT fail with exit(1) in case of an error. All the
   functions you have to implement have ways to indicated failure
   cases. Use these, mapping your internal errors intelligently onto
   the POSIX error conditions.

   * And of course: your code MUST NOT SEGFAULT!

   It is reasonable to proceed in the following order:

   (1)   Design and implement a mechanism that initializes a filesystem
         whenever the memory space is fresh. That mechanism can be
         implemented in the form of a filesystem handle into which the
         filesystem raw memory pointer and sizes are translated.
         Check that the filesystem does not get reinitialized at mount
         time if you initialized it once and unmounted it but that all
         pieces of information (in the handle) get read back correctly
         from the backup-file. 

   (2)   Design and implement functions to find and allocate free memory
         regions inside the filesystem memory space. There need to be 
         functions to free these regions again, too. Any "global" variable
         goes into the handle structure the mechanism designed at step (1) 
         provides.

   (3)   Carefully design a data structure able to represent all the
         pieces of information that are needed for files and
         (sub-)directories.  You need to store the location of the
         root directory in a "global" variable that, again, goes into the 
         handle designed at step (1).
          
   (4)   Write __myfs_getattr_implem and debug it thoroughly, as best as
         you can with a filesystem that is reduced to one
         function. Writing this function will make you write helper
         functions to traverse paths, following the appropriate
         subdirectories inside the file system. Strive for modularity for
         these filesystem traversal functions.

   (5)   Design and implement __myfs_readdir_implem. You cannot test it
         besides by listing your root directory with ls -la and looking
         at the date of last access/modification of the directory (.). 
         Be sure to understand the signature of that function and use
         caution not to provoke segfaults nor to leak memory.

   (6)   Design and implement __myfs_mknod_implem. You can now touch files 
         with 

         touch foo

         and check that they start to exist (with the appropriate
         access/modification times) with ls -la.

   (7)   Design and implement __myfs_mkdir_implem. Test as above.

   (8)   Design and implement __myfs_truncate_implem. You can now 
         create files filled with zeros:

         truncate -s 1024 foo

   (9)   Design and implement __myfs_statfs_implem. Test by running
         df before and after the truncation of a file to various lengths. 
         The free "disk" space must change accordingly.

   (10)  Design, implement and test __myfs_utimens_implem. You can now 
         touch files at different dates (in the past, in the future).

   (11)  Design and implement __myfs_open_implem. The function can 
         only be tested once __myfs_read_implem and __myfs_write_implem are
         implemented.

   (12)  Design, implement and test __myfs_read_implem and
         __myfs_write_implem. You can now write to files and read the data 
         back:

         echo "Hello world" > foo
         echo "Hallo ihr da" >> foo
         cat foo

         Be sure to test the case when you unmount and remount the
         filesystem: the files must still be there, contain the same
         information and have the same access and/or modification
         times.

   (13)  Design, implement and test __myfs_unlink_implem. You can now
         remove files.

   (14)  Design, implement and test __myfs_unlink_implem. You can now
         remove directories.

   (15)  Design, implement and test __myfs_rename_implem. This function
         is extremely complicated to implement. Be sure to cover all 
         cases that are documented in man 2 rename. The case when the 
         new path exists already is really hard to implement. Be sure to 
         never leave the filessystem in a bad state! Test thoroughly 
         using mv on (filled and empty) directories and files onto 
         inexistant and already existing directories and files.

   (16)  Design, implement and test any function that your instructor
         might have left out from this list. There are 13 functions 
         __myfs_XXX_implem you have to write.

   (17)  Go over all functions again, testing them one-by-one, trying
         to exercise all special conditions (error conditions): set
         breakpoints in gdb and use a sequence of bash commands inside
         your mounted filesystem to trigger these special cases. Be
         sure to cover all funny cases that arise when the filesystem
         is full but files are supposed to get written to or truncated
         to longer length. There must not be any segfault; the user
         space program using your filesystem just has to report an
         error. Also be sure to unmount and remount your filesystem,
         in order to be sure that it contents do not change by
         unmounting and remounting. Try to mount two of your
         filesystems at different places and copy and move (rename!)
         (heavy) files (your favorite movie or song, an image of a cat
         etc.) from one mount-point to the other. None of the two FUSE
         processes must provoke errors. Find ways to test the case
         when files have holes as the process that wrote them seeked
         beyond the end of the file several times. Your filesystem must
         support these operations at least by making the holes explicit 
         zeros (use dd to test this aspect).

   (18)  Run some heavy testing: copy your favorite movie into your
         filesystem and try to watch it out of the filesystem.

*/

/* Helper types and functions */

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
	size_t ndfree;
	sz_blk ntsize;
	offset nodetbl;
} fsheader;

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

void namepathset(char *name, const char *path)
{
	size_t len=0;
	while(path[len]!='/' && path[len]!='\0'){
		name[len]=path[len];
		if(++len==NAMELEN-1) break;
	}name[len]='\0';
}

int namepatheq(char *name, const char *path)
{
	size_t len=0;
	while(path[len]!='/' && path[len]!='\0'){
		if(name[len]=='\0' || name[len]!=path[len]) return 0;
		if(++len==NAMELEN) return 1;
	}return (name[len]=='\0');
}

char* pathsplit(char *path)
{
	size_t cur=0;
	while(path[cur]!='\0'){
		if(path[cur++]=='/'){
			path=&path[cur];
			cur=0;
		}
	}return path;
}

int fsinit(void *fsptr, size_t fssize)
{
	fsheader *fshead=fsptr;
	freereg *fhead;
	inode *root;
	
	if(fssize<2*BLKSZ) return -1;
	if(fshead->size==fssize/BLKSZ) return 0;
	
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
	if(timespec_get(&(root->ctime),TIME_UTC)!=TIME_UTC) return -1;
	root->mtime=root->ctime;
	root->nlinks=1;
	root->size=0;
	root->nblocks=0;
	root->blocks[0]=NULLOFF;
	root->blocklist=NULLOFF;
	//null out other nodes?
	
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

nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, dblk=nodetbl[dir].blocks[0];
	direntry *df;
	size_t block=0, entry=0;
	
	if(nodetbl[dir].nlinks==0 || nodetbl[dir].mode!=DIRMODE) return NONODE;
	//check node and dir fit in nodetbl?
	//update dir size,nblocks,nlinks on add/remove
	
	while(dblk!=NULLOFF){
		df=(direntry*)O2P(dblk*BLKSZ);
		while(entry<FILES_DIR){
			if(df[entry].node==NONODE) break;
			if(namepatheq(df[entry].name,name)){
				if(rename!=NULL){
					if(node==NONODE){
						namepathset(df[entry].name,rename);
						return df[entry].node;
					}else{
						//seek to end of dir
						//overwrite found w/ last
						//nullout last
						//if empty dblock, free
						return df[entry].node;
					}
				}else{
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
					oblk=offs->next;
					offs=(offblock*)O2P(oblk*BLKSZ);
					dblk=offs->blocks[block=0];
				}
			}else dblk=offs->blocks[block];
		}
	}if(node==NONODE || rename!=NULL) return NONODE;
	if(dblk==NULLOFF){
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
		}df=(direntry*)O2P(dblk*BLKSZ);
	}nodetbl[dir].size+=sizeof(direntry);
	df[entry].node=node;
	namepathset(df[entry].name,name);
	nodetbl[node].nlinks++;
	if(++entry<FILES_DIR) df[entry].node=NONODE;
	return node;
}

nodei path2node(void *fsptr, const char *path)
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
nodei path2parent(void *fsptr, char *path)//find node corresponding to parent dir
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

/* End of helper functions */

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
int __myfs_getattr_implem(void *fsptr, size_t fssize, int *errnoptr,
                          uid_t uid, gid_t gid,
                          const char *path, struct stat *stbuf) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((node=path2node(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}
	
	stbuf->st_uid=uid;
	stbuf->st_gid=gid;
	stbuf->st_mode=nodetbl[node].mode;
	stbuf->st_size=nodetbl[node].size;
	stbuf->st_nlink=nodetbl[node].nlinks;
	stbuf->st_atim=nodetbl[node].atime;
	stbuf->st_mtim=nodetbl[node].mtime;
	stbuf->st_ctim=nodetbl[node].ctime;
	return 0;
}

/* Implements an emulation of the readdir system call on the filesystem 
   of size fssize pointed to by fsptr. 

   If path can be followed and describes a directory that exists and
   is accessable, the names of the subdirectories and files 
   contained in that directory are output into *namesptr. The . and ..
   directories must not be included in that listing.

   If it needs to output file and subdirectory names, the function
   starts by allocating (with calloc) an array of pointers to
   characters of the right size (n entries for n names). Sets
   *namesptr to that pointer. It then goes over all entries
   in that array and allocates, for each of them an array of
   characters of the right size (to hold the i-th name, together 
   with the appropriate '\0' terminator). It puts the pointer
   into that i-th array entry and fills the allocated array
   of characters with the appropriate name. The calling function
   will call free on each of the entries of *namesptr and 
   on *namesptr.

   The function returns the number of names that have been 
   put into namesptr. 

   If no name needs to be reported because the directory does
   not contain any file or subdirectory besides . and .., 0 is 
   returned and no allocation takes place.

   On failure, -1 is returned and the *errnoptr is set to 
   the appropriate error code. 

   The error codes are documented in man 2 readdir.

   In the case memory allocation with malloc/calloc fails, failure is
   indicated by returning -1 and setting *errnoptr to EINVAL.

*/
int __myfs_readdir_implem(void *fsptr, size_t fssize, int *errnoptr,
                          const char *path, char ***namesptr) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	blkset oblk, dblk;
	nodei dir;
	direntry *df;
	size_t count, block=0, entry=0;
	char **namelist;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=O2P(fshead->nodetbl);
	if((dir=path2node(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(nodetbl[dir].mode!=DIRMODE){
		*errnoptr=ENOTDIR;
		return -1;
	}
	
	oblk=NULLOFF;
	dblk=nodetbl[dir].blocks[0];
	count=nodetbl[dir].size/sizeof(direntry);
	if((namelist=malloc(count*sizeof(char*)))==NULL){
		*errnoptr=EINVAL;
		return -1;
	}count=0;
	
	while(dblk!=NULLOFF){
		df=(direntry*)O2P(dblk*BLKSZ);
		while(entry<FILES_DIR){
			if(df[entry].node==NONODE) break;
			namelist[count]=(char*)malloc(strlen(df[entry].name));
			if(namelist[count]==NULL){
				while(count) free(namelist[--count]);
				free(namelist);
				*errnoptr=EINVAL;
				return -1;
			}strcpy(namelist[count],df[entry].name);
			//malloc,copy,append df[entry].name
			//on fail, go back thru list and free
			entry++; count++;
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
	}*namesptr=namelist;
	return count;
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
int __myfs_mknod_implem(void *fsptr, size_t fssize, int *errnoptr, const char *path) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	struct timespec creation;
	nodei pnode, node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((pnode=path2parent(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((node=newnode(fsptr))==NONODE){
		*errnoptr=ENOSPC;
		return -1;
	}if(timespec_get(&creation,TIME_UTC)!=TIME_UTC){
		*errnoptr=EACCES;
		return -1;
	}if(dirmod(fsptr,pnode,pathsplit(path),node,NULL)==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}
	
	nodetbl[node].mode=FILEMODE;
	nodetbl[node].size=0;
	nodetbl[node].nblocks=0;
	nodetbl[node].blocks[0]=NULLOFF;
	nodetbl[node].blocklist=NULLOFF;
	nodetbl[node].ctime=creation;
	nodetbl[node].mtime=creation;
	return 0;
}

/* Implements an emulation of the unlink system call for regular files
   on the filesystem of size fssize pointed to by fsptr.

   This function is called only for the deletion of regular files.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 unlink.

*/
int __myfs_unlink_implem(void *fsptr, size_t fssize, int *errnoptr, const char *path) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei pnode;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((pnode=path2parent(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(dirmod(fsptr,pnode,pathsplit(path),0,"")==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}return 0;
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
int __myfs_rmdir_implem(void *fsptr, size_t fssize, int *errnoptr, const char *path) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei pnode;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((pnode=path2parent(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(nodetbl[pnode].size>0){
		*errnoptr=ENOTEMPTY;
		return -1;
	}if(dirmod(fsptr,pnode,pathsplit(path),0,"")==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}return 0;
}

/* Implements an emulation of the mkdir system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call creates the directory indicated by path.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 mkdir.

*/
int __myfs_mkdir_implem(void *fsptr, size_t fssize, int *errnoptr, const char *path) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	struct timespec creation;
	nodei pnode, node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((pnode=path2parent(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((node=newnode(fsptr))==NONODE){
		*errnoptr=ENOSPC;
		return -1;
	}if(timespec_get(&creation,TIME_UTC)!=TIME_UTC){
		*errnoptr=EACCES;
		return -1;
	}if(dirmod(fsptr,pnode,pathsplit(path),node,NULL)==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}
	
	nodetbl[node].mode=DIRMODE;
	nodetbl[node].size=0;
	nodetbl[node].nblocks=0;
	nodetbl[node].blocks[0]=NULLOFF;
	nodetbl[node].blocklist=NULLOFF;
	nodetbl[node].ctime=creation;
	nodetbl[node].mtime=creation;
	return 0;
}

/* Implements an emulation of the rename system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call moves the file or directory indicated by from to to.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   Caution: the function does more than what is hinted to by its name.
   In cases the from and to paths differ, the file is moved out of 
   the from path and added to the to path.

   The error codes are documented in man 2 rename.

*/
int __myfs_rename_implem(void *fsptr, size_t fssize, int *errnoptr,
                         const char *from, const char *to) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei pfrom, pto, file;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((pfrom=path2parent(fsptr,from))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((pto=path2parent(fsptr,to))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((file=dirmod(fsptr,pfrom,pathsplit(from),NONODE,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}
	
	if(pto==pfrom){
		if(dirmod(fsptr,pfrom,pathsplit(from),NONODE,pathsplit(to))==NONODE){
			*errnoptr=EEXIST;
			return -1;
		}return 0;
	}
	
	if(dirmod(fsptr,pto,pathsplit(to),file,NULL)==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}if(dirmod(fsptr,pfrom,pathsplit(from),0,"")==NONODE){
		dirmod(fsptr,pto,pathsplit(to),0,"");
		*errnoptr=EACCES;
		return -1;
	}return 0;
}

/* Implements an emulation of the truncate system call on the filesystem 
   of size fssize pointed to by fsptr. 

   The call changes the size of the file indicated by path to offset
   bytes.

   When the file becomes smaller due to the call, the extending bytes are
   removed. When it becomes larger, zeros are appended.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 truncate.

*/
int __myfs_truncate_implem(void *fsptr, size_t fssize, int *errnoptr,
                           const char *path, off_t offset) {
	fsheader *fshead=fsptr;
	nodei node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}if((node=path2node(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(frealloc(fsptr,node,offset)==-1){
		*errnoptr=EINVAL;
		return -1;
	}return 0;
}

/* Implements an emulation of the open system call on the filesystem 
   of size fssize pointed to by fsptr, without actually performing the opening
   of the file (no file descriptor is returned).

   The call just checks if the file (or directory) indicated by path
   can be accessed, i.e. if the path can be followed to an existing
   object for which the access rights are granted.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The two only interesting error codes are 

   * EFAULT: the filesystem is in a bad state, we can't do anything

   * ENOENT: the file that we are supposed to open doesn't exist (or a
             subpath).

   It is possible to restrict ourselves to only these two error
   conditions. It is also possible to implement more detailed error
   condition answers.

   The error codes are documented in man 2 open.

*/
int __myfs_open_implem(void *fsptr, size_t fssize, int *errnoptr, const char *path) {
	fsheader *fshead=fsptr;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}if(path2node(fsptr,path)==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}return 0;
}

/* Implements an emulation of the read system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call copies up to size bytes from the file indicated by 
   path into the buffer, starting to read at offset. See the man page
   for read for the details when offset is beyond the end of the file etc.
   
   On success, the appropriate number of bytes read into the buffer is
   returned. The value zero is returned on an end-of-file condition.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 read.

*/
int __myfs_read_implem(void *fsptr, size_t fssize, int *errnoptr,
                       const char *path, char *buf, size_t size, off_t offset) {
	fsheader *fshead=fsptr;
	nodei node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}if((node=path2node(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}
	//check notdir
	//verify/seek to offset
	//read up to size bytes to buf
  /* STUB */
  return -1;
}

/* Implements an emulation of the write system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call copies up to size bytes to the file indicated by 
   path into the buffer, starting to write at offset. See the man page
   for write for the details when offset is beyond the end of the file etc.
   
   On success, the appropriate number of bytes written into the file is
   returned. The value zero is returned on an end-of-file condition.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 write.

*/
int __myfs_write_implem(void *fsptr, size_t fssize, int *errnoptr,
                        const char *path, const char *buf, size_t size, off_t offset) {
	fsheader *fshead=fsptr;
	nodei node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}if((node=path2node(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}
  /* STUB */
  return -1;
}

/* Implements an emulation of the utimensat system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call changes the access and modification times of the file
   or directory indicated by path to the values in ts.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 utimensat.

*/
int __myfs_utimens_implem(void *fsptr, size_t fssize, int *errnoptr,
                          const char *path, const struct timespec ts[2]) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei node;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	if((node=path2node(fsptr,path))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}
	
	nodetbl[node].atime=ts[0];
	nodetbl[node].mtime=ts[1];
	return 0;
}

/* Implements an emulation of the statfs system call on the filesystem 
   of size fssize pointed to by fsptr.

   The call gets information of the filesystem usage and puts in 
   into stbuf.

   On success, 0 is returned.

   On failure, -1 is returned and *errnoptr is set appropriately.

   The error codes are documented in man 2 statfs.

   Essentially, only the following fields of struct statvfs need to be
   supported:

   f_bsize   fill with what you call a block (typically 1024 bytes)
   f_blocks  fill with the total number of blocks in the filesystem
   f_bfree   fill with the free number of blocks in the filesystem
   f_bavail  fill with same value as f_bfree
   f_namemax fill with your maximum file/directory name, if your
             filesystem has such a maximum

*/
int __myfs_statfs_implem(void *fsptr, size_t fssize, int *errnoptr,
                         struct statvfs* stbuf) {
	fsheader *fshead=fsptr;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}
	
	stbuf->f_bsize=BLKSZ;
	stbuf->f_blocks=fshead->size;
	stbuf->f_bfree=fshead->free;
	stbuf->f_bavail=fshead->free;
	stbuf->f_namemax=NAMELEN-1;
	return 0;
}
