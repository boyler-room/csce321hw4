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

#include "myfs_helper.h"

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

/*Implementation Details
	
*/
/*Implementation Decisions
	Fixed length file names
		truncation
	Node table size
	directories as files
	offsets/node/offset blocks
*/


/* FUSE Function Implementations */

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
	size_t unit=1;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	
	if((node=path2node(fsptr,path,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(nodetbl[node].mode==DIRMODE) unit=sizeof(direntry);
	
	stbuf->st_uid=uid;
	stbuf->st_gid=gid;
	stbuf->st_mode=nodetbl[node].mode;
	stbuf->st_size=nodetbl[node].size*unit;
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
	direntry *df;
	nodei dir;
	fpos pos;
	size_t count=0;
	char **namelist;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=O2P(fshead->nodetbl);
	
	if((dir=path2node(fsptr,path,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(nodetbl[dir].mode!=DIRMODE){
		*errnoptr=ENOTDIR;
		return -1;
	}if((namelist=malloc(nodetbl[dir].size*sizeof(char*)))==NULL){
		*errnoptr=EINVAL;
		return -1;
	}
	
	loadpos(fsptr,&pos,dir);
	while(pos.data!=NULLOFF){
		df=(direntry*)B2P(pos.dblk);
		if(df[pos.dpos].node==NONODE) break;
		namelist[count]=(char*)malloc(strlen(df[pos.dpos].name));
		if(namelist[count]==NULL){
			while(count) free(namelist[--count]);
			free(namelist);
			*errnoptr=EINVAL;
			return -1;
		}strcpy(namelist[count],df[pos.dpos].name);
		count++;
		seek(fsptr,&pos,1);
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
	const char *fname;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	
	if((pnode=path2node(fsptr,path,&fname))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((node=newnode(fsptr))==NONODE){
		*errnoptr=ENOSPC;
		return -1;
	}if(timespec_get(&creation,TIME_UTC)!=TIME_UTC){
		*errnoptr=EACCES;
		return -1;
	}if(dirmod(fsptr,pnode,fname,node,NULL)==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}
	
	nodetbl[node].mode=FILEMODE;
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
	nodei pnode, node;
	const char *fname;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	
	if((pnode=path2node(fsptr,path,&fname))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((node=dirmod(fsptr,pnode,fname,0,""))==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}if(nodetbl[node].nlinks==0){
		frealloc(fsptr,node,0);
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
	nodei pnode, node;
	const char *fname;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);
	
	if((pnode=path2node(fsptr,path,&fname))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(dirmod(fsptr,pnode,fname,0,"")==NONODE){
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
	const char *fname;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}nodetbl=(inode*)O2P(fshead->nodetbl);

	if((pnode=path2node(fsptr,path,&fname))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((node=newnode(fsptr))==NONODE){
		*errnoptr=ENOSPC;
		return -1;
	}if(timespec_get(&creation,TIME_UTC)!=TIME_UTC){
		*errnoptr=EACCES;
		return -1;
	}if(dirmod(fsptr,pnode,fname,node,NULL)==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}
	
	nodetbl[node].mode=DIRMODE;
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
	nodei pfrom, pto, file;
	const char *ffrom, *fto;
	
	if(fsinit(fsptr,fssize)==-1){
		*errnoptr=EFAULT;
		return -1;
	}if((pfrom=path2node(fsptr,from,&ffrom))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((pto=path2node(fsptr,to,&fto))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if((file=dirmod(fsptr,pfrom,ffrom,NONODE,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}
	
	if(pto==pfrom){
		if(dirmod(fsptr,pfrom,ffrom,NONODE,fto)==NONODE){
			*errnoptr=EEXIST;
			return -1;
		}return 0;
	}
	
	if(dirmod(fsptr,pto,fto,file,NULL)==NONODE){
		*errnoptr=EEXIST;
		return -1;
	}if(dirmod(fsptr,pfrom,ffrom,0,"")==NONODE){
		dirmod(fsptr,pto,fto,0,"");
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
	}
	printf("TRUNCATE CALL\n");
	printfs(fsptr);
	
	if((node=path2node(fsptr,path,NULL))==NONODE){
		*errnoptr=ENOENT;
		return -1;
	}if(frealloc(fsptr,node,offset)==-1){
		*errnoptr=EPERM;
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
	}if(path2node(fsptr,path,NULL)==NONODE){
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
                       const char *path, char *buf, size_t size, off_t off) {
	fsheader *fshead=fsptr;
	inode *nodetbl;
	nodei node;
	fpos pos;
	size_t readct=0;
	
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
	}loadpos(fsptr,&pos,node);
	seek(fsptr,&pos,off);
	while(pos.data!=NULLOFF && readct<size){//TEMPORARY
		buf[readct++]=((char*)B2P(pos.dblk))[pos.dpos];
		seek(fsptr,&pos,1);
	}return readct;
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
	
	if((node=path2node(fsptr,path,NULL))==NONODE){
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
