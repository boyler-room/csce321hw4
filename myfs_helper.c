/*CSCE321 HW4: myfs
	myfs_helper.c: helper function definitions
*/

/*Helper Function Internals
	offsort
	blkalloc
	blkfree
	newnode
	nodegood
	nodevalid
	loadpos
	advance
	...
	namepathset
	namepatheq
	dirmod
	path2node
	fsinit
*/
#include "myfs_helper.h"

/*
GOOD:
	freereg
	offblock
	direntry
	
	blkalloc
	blkfree
	offsort
	path2node
	namepathset
	namepatheq
	newnode
	nodegood
	loadpos
	
	open
	statfs
	utimens
	getattr
~OK:
	fpos
	fsheader
	inode
	
	nodevalid
	fsinit
	dirmod
	advance
	
	mkdir
	mknod
	rmdir
	unlink
	readdir
	truncate
	rename
TODO:
	documentation
	better errno use
	review internal error cases
	modify dirmod/readdir to use fpos struct
	more efficient frealloc: memcpy from alloc arr to oblks; freeblk whole oblks
	frealloc
	
	read
	write
*/

sz_blk blkalloc(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *prev=NULL;
	sz_blk alloct=0;
	
	while(alloct<count && freeoff!=NULLOFF){
		freereg fhead=*O2P(freeoff*BLKSZ);
		sz_blk freeblk=0;
		while(freeblk<(fhead.size) && alloct<count){
			buf[alloct++]=freeoff+freeblk++;
		}memset(O2P(freeoff*BLKSZ),0,freeblk*BLKSZ);
		if(freeblk==(fhead.size)){
			freeoff=fhead.next;
			if(prev!=NULL) prev->next=fhead.next;
			else fshead->freelist=fhead.next;
		}else{
			fhead.size-=freeblk;
			freeoff+=freeblk;
			if(prev!=NULL) prev->next=freeoff;
			else fshead->freelist=freeoff;
			prev=(freereg*)O2P(freeoff*BLKSZ);
			*prev=fhead;
		}
	}fshead->free-=alloct;
	return alloct;
}


void swap(blkset *A, blkset *B){ blkset t=*A; *A=*B; *B=t; }
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
void offsort(blkset *data,size_t len)
{
	size_t i=len;
	
	while(i) filter(data,--i,len);
	while(len){
		swap(data,&data[--len]);
		filter(data,0,len);
	}
}
void blkfree(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *fhead;
	sz_blk freect=0;
	
	offsort(buf,count);
	while(freect<count && *buf<(fshead->ntsize)){
		*(buf++)=NULLOFF; count--;
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
			(buf++)[freect]=NULLOFF; count--;
		}
	}while(freect<count){
		(buf++)[freect]=NULLOFF; count--;
	}fshead->free+=freect;
}

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

int nodegood(void *fsptr, nodei node)
{
	fsheader *fshead=(fsheader*)fsptr;
	return (node<0 || node>(fshead->ntsize*NODES_BLOCK-2));
}
int nodevalid(void *fsptr, nodei node)
{
	fsheader *fshead=(fsheader*)fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	return (nodegood(fsptr,node) || nodetbl[node].nlinks==0 ||
		(nodetbl[node].mode!=DIRMODE && nodetbl[node].mode!=FILEMODE));
}

void loadpos(void *fsptr, fpos *pos, nodei node)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	if(pos==NULL) return;
	if(nodevalid(fsptr,node)){
		pos->node=NONODE;
		return;
	}pos->node=node;
	pos->nblk=0;
	pos->opos=0;
	pos->dpos=0;
	pos->oblk=NULLOFF;
	pos->dblk=*nodetbl[node].blocks;
	if(pos->dblk==NULLOFF) pos->data=NULLOFF;
	else pos->data=pos->dblk*BLKSZ;
}

//when given blk, align advance to beggining of blocks/discard dpos
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
	}while(blk>0){
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

/*
	write: seek to offset
	if past eof extend file to offset, zero
	when write past end of last block, alloc new
	memcpy whole blocks
*/
/*
	if(nodetbl[node].mode!=FILEMODE) *errnoptr=EISDIR; return -1;
	advance(fsptr,&pos,0,nodetbl[node].size);
	if(pos.data==NULLOFF || count==0) return 0;
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
//make sure size and nblocks always accurate, check dirmod, etc
/*
	calc blocks to alloc/free
	calc extra bytes to expand within old last, new last blocks
	zero blocks in blkalloc?
	if expanding
		calc total dblks
		calc additional oblks
		zero last block underhang
		allocate at once, step through to load into file, acct for oblks?
		if<amt: free, return fail
		OR
		step through, how to free on fail?
	if shrinking: no error risk?
		seek to b4 blocks to free, step through
		free rest of oblk/node list
		save to list and free at once?
	change size,nblocks on success
*/
void ffree(void *fsptr, nodei node)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	//check nodegood
	//if directory fail
	
	if(nodetbl[node].blocklist!=NULLOFF){
		blkfree(fsptr,/**/,offs.blocks)
	}
	//free blocks in nodelist
	//if blocklist !null
		//load oblk, free dblks in oblk
		//while next !null
			//load next, free last
		//null blocklist
	//update size,nblocks
}

//use node.size as #entries for dirs, update getattr,advance,dirmod
//make sure oblk and nodelist entries past last always NULL
int frealloc(void *fsptr, nodei node, off_t size)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t unit;
	fpos pos;
	ssize_t sdiff;
	
	loadpos(fsptr,&pos,node);
	if(size<0 || pos.node==NONODE) return -1;
	if(nodetbl[pos.node].mode==DIRMODE){
		unit=sizeof(direntry);
	}else unit=1;
	sdiff=(size*unit+BLKSZ-1)/BLKSZ-nodetbl[node].nblocks;
	
	//if sdiff 0:
		//if size<filesize: decrement filesize to size (noop/done anyways)
		//elif size>filesize: memset data past eof in block
		//else noop
		//if sdiff>0
	if(sdiff>0){//need to allocate
		if(sdiff>fshead->free) return -1;
		advance(fsptr,&pos,0,nodetbl[node].size/unit);
		if(pos.dpos<BLKSZ/unit){
			//memset
		}
		//if not eoblk, zero entries at end of block
		//if oblk not full, alloc min sdiff,full
		//while sdiff, alloc
		//advance(fsptr,&pos,0,nodetbl[node].size)
		//zero
	}else if(sdiff<0){//need to free
		advance(fsptr,&pos,nodetbl[node].nblocks-sdiff,0);
		while(pos.data!=NULLOFF){
			blkfree(fsptr,1,&(pos.dblk));
			//if oblk empty, free
			advance(fsptr,&pos,1,0);
		}
		//step through blocks
		//free blocks after
		//free oblk if empty, load next
	}nodetbl[node].nblocks+=sdiff;
	nodetbl[node].size=size*unit;
	return 0;
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
/*advance cases
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
/*fpos implementation
nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	direntry *df, *found=NULL;
	blkset prevo=NULLOFF;
	fpos pos;
	
	if(nodevalid(fsptr,dir) || nodetbl[dir].mode!=DIRMODE) return NONODE;
	if(node!=NONODE && rename==NULL && nodegood(fsptr,node)) return NONODE;
	if(*name=='\0' || (rename!=NULL && node==NONODE && *rename=='\0')) return NONODE;
	
	loadpos(fsptr,&pos,dir);	
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
		nodetbl[node].nlinks--;//must free data manually
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
		nodetbl[node].nlinks--;//need to free manually
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
	memset(nodetbl,0,fshead->ntsize*BLKSZ-sizeof(inode));
	nodetbl[0].mode=DIRMODE;
	nodetbl[0].ctime=creation;
	nodetbl[0].mtime=creation;
	nodetbl[0].nlinks=1;
	nodetbl[0].size=0;
	nodetbl[0].nblocks=0;
	nodetbl[0].blocks[0]=NULLOFF;
	nodetbl[0].blocklist=NULLOFF;
	
	fshead->size=fssize/BLKSZ;
	return 0;
}
