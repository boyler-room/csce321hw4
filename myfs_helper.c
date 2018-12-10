/*CSCE321 HW4: myfs
	myfs_helper.c: helper function definitions
*/

/*Helper Function Internals
	offsort
	blkalloc
	blkfree
	newnode
	nodevalid
	loadpos
	advance
	seek
	frealloc
	namepathset
	namepatheq
	dirmod
	path2node
	fsinit
*/

/*GUARANTEES
	done
	to verify
		BLKSZ%sizeof(direntry), BLKSZ%sizeof(inode) must be 0
		sizeof(offblock) must be BLKSZ
		sizeof(fsheader) must be <= sizeof(node)
		files (incl dirs) do not contain any empty offblocks or datablocks
		no orphaned blocks: any block past the nodetbl can be found in the freelist or a node or a node's blocklist
		inode.nblocks always # datablocks alloc'ed to file (found while traversing)
		inode.size always # entries in dir, always fits in nblocks
	??	unlinked node blocks[] and blocklist always valid
		all fields in linked node valid/accurate
		all newly allocated blocks filled with 0
		no free regions of size 0
		no adjacent free regions
		fpos vars always init'd via loadpos before use, verified
		fpos vars contain values as specified to match the file position/mark eof
		nodes returned from newnode are empty
		frealloc does not fail on size 0
		frealloc does not fail when shrinking
		frealloc does not work on dirs
*/
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
	nodevalid
	loadpos
	
	open
	statfs
	utimens
	getattr
	readdir
~OK:
	fpos
	fsheader
	inode
	
	fsinit
	dirmod
	advance
	seek
	
	mkdir
	mknod
	rmdir
	unlink
	truncate
	rename
TODO:
	documentation
	better errno use
	review internal error cases
	modify dirmod to use fpos struct
	more efficient frealloc: memcpy from alloc arr to oblks
	
	read
	write
*/
#include "myfs_helper.h"

sz_blk blkalloc(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *prev=NULL;
	sz_blk alloct=0;
	
	while(alloct<count && freeoff!=NULLOFF){
		freereg fhead=*(freereg*)B2P(freeoff);
		sz_blk freeblk=0;
		while(freeblk<(fhead.size) && alloct<count){
			buf[alloct++]=freeoff+freeblk++;
		}memset(B2P(freeoff),0,freeblk*BLKSZ);
		if(freeblk==(fhead.size)){
			freeoff=fhead.next;
			if(prev!=NULL) prev->next=fhead.next;
			else fshead->freelist=fhead.next;
		}else{
			fhead.size-=freeblk;
			freeoff+=freeblk;
			if(prev!=NULL) prev->next=freeoff;
			else fshead->freelist=freeoff;
			prev=(freereg*)B2P(freeoff);
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
sz_blk blkfree(void *fsptr, sz_blk count, blkset *buf)
{
	fsheader *fshead=fsptr;
	blkset freeoff=fshead->freelist;
	freereg *fhead;
	sz_blk freect=0;
	
	offsort(buf,count);
	while(freect<count && *buf<(fshead->ntsize)){
		*(buf++)=NULLOFF; count--;
	}if(freect<count && ((freeoff==NULLOFF && *buf<fshead->size) || *buf<freeoff)){
		fhead=(freereg*)B2P(freeoff=*buf);
		fhead->next=fshead->freelist;
		fshead->freelist=freeoff;
		if((freeoff+(fhead->size=1))==fhead->next){
			freereg *tmp=B2P(fhead->next);
			fhead->size+=tmp->size;
			fhead->next=tmp->next;
		}buf[freect++]=NULLOFF;
	}while(freect<count && buf[freect]<fshead->size){
		fhead=(freereg*)B2P(freeoff);
		if(buf[freect]>=(freeoff+fhead->size)){
			if(fhead->next!=NULLOFF && buf[freect]>=fhead->next){
				freeoff=fhead->next;
				continue;
			}if(buf[freect]==(freeoff+fhead->size)){
				fhead->size++;
			}else{
				freereg *tmp=B2P(freeoff=buf[freect]);
				tmp->next=fhead->next;
				tmp->size=1;
				fhead->next=freeoff;
				fhead=tmp;
			}if((freeoff+fhead->size)==fhead->next){
				freereg *tmp=B2P(fhead->next);
				fhead->size+=tmp->size;
				fhead->next=tmp->next;
			}buf[freect++]=NULLOFF;
		}else{
			(buf++)[freect]=NULLOFF; count--;
		}
	}while(freect<count){
		(buf++)[freect]=NULLOFF; count--;
	}fshead->free+=freect;
	return freect;
}

//make sure all (inc unlinked) nodetbl entries have valid data block sections
	//initialize to empty
	//set to empty on unlink?
//make sure all linked nodes have valid attrs
//initialize node?
nodei newnode(void *fsptr)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t nodect=fshead->ntsize*NODES_BLOCK-1;
	nodei i=0;
	while(++i<nodect){
		if(nodetbl[i].nlinks==0){
			nodetbl[i].size=0;
			nodetbl[i].nblocks=0;
			nodetbl[i].blocks[0]=NULLOFF;
			nodetbl[i].blocklist=NULLOFF;
			return i;
		}
	}return NONODE;
}

int nodevalid(void *fsptr, nodei node)
{
	fsheader *fshead=(fsheader*)fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	if(node<0 || node>=(fshead->ntsize*NODES_BLOCK-1)) return NODEI_BAD;
	if(nodetbl[node].nlinks==0 ||(nodetbl[node].mode!=DIRMODE && nodetbl[node].mode!=FILEMODE)) return NODEI_GOOD;
	return NODEI_LINKD;
}

void loadpos(void *fsptr, fpos *pos, nodei node)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	
	if(pos==NULL) return;
	if(nodevalid(fsptr,node)<NODEI_GOOD){
		pos->node=NONODE;
		return;
	}pos->node=node;
	pos->nblk=0;
	pos->opos=0;
	pos->dpos=0;
	pos->oblk=NULLOFF;
	pos->dblk=nodetbl[node].blocks[0];
	pos->data=pos->dblk*BLKSZ;
}

//when given blk, align advance to beggining of blocks/discard dpos
//move fpos up to blk blocks and off entries/bytes forward in the dir/file, return actual advancement, -1 if failed
//if valid, pos will always be at the beginning of a block after advance
sz_blk advance(void *fsptr, fpos *pos, sz_blk blks)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	sz_blk adv=0;
	size_t unit=1;
	
	if(pos==NULL || pos->node==NONODE || pos->dblk==NULLOFF) return 0;
	if(nodetbl[pos->node].mode==DIRMODE) unit=sizeof(direntry);
	
	if(pos->data==NULLOFF){
		if(pos->dpos*unit==BLKSZ) pos->opos--;
	}pos->dpos=0;
	while(blks){
		blkdex opos=pos->opos+1;
		if(pos->oblk==NULLOFF){
			if(opos==OFFS_NODE){
				if((pos->oblk=nodetbl[pos->node].blocklist)==NULLOFF) break;
				offblock *offs=B2P(pos->oblk);
				pos->dblk=offs->blocks[opos=0];
			}else{
				if(nodetbl[pos->node].blocks[opos]==NULLOFF) break;
				pos->dblk=nodetbl[pos->node].blocks[opos];
			}
		}else{
			offblock *offs=B2P(pos->oblk);
			if(opos==OFFS_BLOCK){
				if(offs->next==NULLOFF) break;
				pos->oblk=offs->next;
				offs=(offblock*)B2P(pos->oblk);
				pos->dblk=offs->blocks[opos=0];
			}else{
				if(offs->blocks[opos]==NULLOFF) break;
				pos->dblk=offs->blocks[opos];
			}
		}pos->opos=opos;
		adv++; blks--;
	}pos->data=pos->dblk*BLKSZ;
	pos->nblk+=adv;
	return adv;
}

//entry in file=pos.nblk*BLKSZ+pos.dpos
size_t seek(void *fsptr, fpos *pos, size_t off)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	size_t adv=0, bck=0, unit=1;
	sz_blk blks;
	
	if(pos==NULL || pos->node==NONODE || pos->data==NULLOFF) return 0;
	if(nodetbl[pos->node].mode==DIRMODE) unit=sizeof(direntry);
	
	if((blks=(off+pos->dpos)*unit/BLKSZ)>0){
		off=(off+pos->dpos)%(BLKSZ/unit);
		bck=pos->dpos;
		if((adv=advance(fsptr,pos,blks))<blks){
			off=BLKSZ/unit;
		}adv*=BLKSZ/unit;
	}while(pos->data!=NULLOFF && off>0){
		pos->dpos++;
		if((pos->nblk*BLKSZ/unit+pos->dpos)==nodetbl[pos->node].size){
			if(pos->dpos==BLKSZ/unit) pos->opos++;
			pos->data=NULLOFF;
		}else{
			pos->data=pos->dblk*BLKSZ+pos->dpos*unit;
			adv++; off--;
		}
	}return (adv-bck);
}

//expand a file blks blocks past eof
/*sz_blk expand(void *fsptr, fpos *pos, sz_blk blks)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	sz_blk adv=0;
	size_t unit=1;
	
	if(pos==NULL || pos->node==NONODE || pos->data!=NULLOFF) return 0;
	if(nodetbl[pos->node].mode==DIRMODE) unit=sizeof(direntry);
	
	if(pos->data==NULLOFF){
		if(pos->dpos*unit==BLKSZ) pos->opos--;
	}pos->dpos=0;
	while(blks){
		blkdex opos=pos->opos+1;
		if(pos->oblk==NULLOFF){
			if(opos==OFFS_NODE){
				if((pos->oblk=nodetbl[pos->node].blocklist)==NULLOFF) break;
				offblock *offs=B2P(pos->oblk);
				pos->dblk=offs->blocks[opos=0];
			}else{
				if(nodetbl[pos->node].blocks[opos]==NULLOFF) break;
				pos->dblk=nodetbl[pos->node].blocks[opos];
			}
		}else{
			offblock *offs=B2P(pos->oblk);
			if(opos==OFFS_BLOCK){
				if(offs->next==NULLOFF) break;
				pos->oblk=offs->next;
				offs=(offblock*)B2P(pos->oblk);
				pos->dblk=offs->blocks[opos=0];
			}else{
				if(offs->blocks[opos]==NULLOFF) break;
				pos->dblk=offs->blocks[opos];
			}
		}pos->opos=opos;
		adv++; blks--;
	}pos->data=pos->dblk*BLKSZ;
	pos->nblk+=adv;
	return adv;
}*/

//make sure size and nblocks always accurate, check dirmod, etc
/*
	if expanding
		calc total dblks
		calc additional oblks
		zero last block underhang
		allocate at once, step through to load into file, acct for oblks?
		if<amt: free, return fail
		OR
		step through, how to free on fail?
*/

//use node.size as #entries for dirs, update getattr,advance,dirmod
//make sure oblk and nodelist entries past last always NULL
//only changes regular files
int frealloc(void *fsptr, nodei node, size_t size)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	fpos pos;
	ssize_t blkdiff;
	sz_blk blksize;
	
	loadpos(fsptr,&pos,node);
	if(pos.node==NONODE || nodetbl[pos.node].mode==DIRMODE) return -1;
	
	blksize=CLDIV(size,BLKSZ);
	blkdiff=blksize-nodetbl[node].nblocks;
	if(blkdiff<0){
		sz_blk fct=0,adv=0;
		offblock *offs;
		if(blksize<=OFFS_NODE){
			fct=OFFS_NODE-blksize;
			advance(fsptr,&pos,blksize);
			if(fct!=0){
				adv=advance(fsptr,&pos,fct);
				blkfree(fsptr,fct,&(nodetbl[node].blocks[blksize]));
			}blkfree(fsptr,1,&(nodetbl[node].blocklist));
		}else{
			advance(fsptr,&pos,blksize-1);
			offs=(offblock*)B2P(pos.oblk);
			advance(fsptr,&pos,1);
			if(pos.opos>0){
				blkset prev=pos.oblk;
				fct=OFFS_BLOCK-pos.opos;
				adv=advance(fsptr,&pos,fct);
				blkfree(fsptr,fct,&(offs->blocks[OFFS_BLOCK-fct]));
			}offs->next=NULLOFF;
		}while(adv==fct){
			offs=(offblock*)B2P(pos.oblk);
			blkset prev=pos.oblk;
			fct=OFFS_BLOCK;
			adv=advance(fsptr,&pos,fct);
			blkfree(fsptr,fct,offs->blocks);
			blkfree(fsptr,1,&prev);
		}
	}else if(size>nodetbl[node].size){
		seek(fsptr,&pos,nodetbl[node].size);
		if(pos.dblk!=NULLOFF && pos.dpos<BLKSZ){
			memset(O2P(pos.dblk*BLKSZ+pos.dpos),0,BLKSZ-pos.dpos);
			pos.opos++;
		}if(blkdiff>0){
			blkset *tblks;
			sz_blk noblks,ext,alloct;
			
			if(pos.oblk==NULLOFF){
				noblks=(blkdiff+pos.opos+(OFFS_BLOCK-OFFS_NODE)-1)/OFFS_BLOCK;
			}else{
				noblks=(blkdiff+pos.opos-1)/OFFS_BLOCK;
			}
			
			if((tblks=(blkset*)malloc((blkdiff+noblks)*sizeof(blkset)))==NULL) return -1;
			if(blkalloc(fsptr,blkdiff+noblks,tblks)<(blkdiff+noblks)){
				blkfree(fsptr,blkdiff+noblks,tblks);
				free(tblks);
				return -1;
			}
			
			nodetbl[node].size=nodetbl[node].nblocks*BLKSZ;
			seek(fsptr,&pos,BLKSZ);
			offblock *offs;
			alloct=0;
			while(alloct<(noblks+blkdiff)){
				if(pos.oblk==NULLOFF){
					if(pos.opos==OFFS_NODE){
						nodetbl[node].blocklist=tblks[alloct++];
						offs=(offblock*)B2P(nodetbl[node].blocklist);
						pos.opos=0; pos.oblk=nodetbl[node].blocklist;
						offs->blocks[0]=tblks[alloct];
					}else{
						nodetbl[node].blocks[pos.opos]=tblks[alloct];
					}
				}else{
					offs=(offblock*)B2P(pos.oblk);
					if(pos.opos==OFFS_BLOCK){
						offs->next=tblks[alloct++];
						pos.oblk=offs->next;
						offs=(offblock*)B2P(offs->next);
						pos.opos=0;
					}offs->blocks[pos.opos]=tblks[alloct];
				}alloct++;
				nodetbl[node].size+=BLKSZ;
				nodetbl[node].nblocks++;
				pos.opos++;
			}
			/*if(pos.oblk==NULLOFF){
				ext=MIN(OFFS_NODE,blkdiff)-pos.opos;
				if(ext>0) memcpy(&(nodetbl[node].blocks[pos.opos]),tblks,ext*sizeof(blkset));
			}else{
				offblock *offs=B2P(pos.oblk);
				ext=MIN(OFFS_BLOCK,blkdiff)-pos.opos;
				if(ext>0) memcpy(&(offs->blocks[pos.opos]),tblks,ext*sizeof(blkset));
			}nodetbl[node].nblocks+=ext;
			nodetbl[node].size=nodetbl[node].nblocks*BLKSZ;
			seek(fsptr,&pos,ext*BLKSZ);
			blkdiff-=ext;
			alloct=ext;
			while(blkdiff>0){
				offblock *offs;
				ext=MIN(OFFS_BLOCK,blkdiff);
				if(pos.oblk==NULLOFF){
					nodetbl[node].blocklist=tblks[alloct++];
					offs=(offblock*)B2P(nodetbl[node].blocklist);
				}else{
					offs=(offblock*)B2P(pos.oblk);
					offs->next=tblks[alloct++];
					offs=(offblock*)B2P(offs->next);
				}memcpy(offs->blocks,&tblks[alloct],ext*sizeof(blkset));
				nodetbl[node].nblocks+=ext;
				nodetbl[node].size=nodetbl[node].nblocks*BLKSZ;
				seek(fsptr,&pos,ext*BLKSZ);
				blkdiff-=ext;
				alloct+=ext;
			}*/free(tblks);
		}
	}nodetbl[node].nblocks=blksize;
	nodetbl[node].size=size;
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
/*nodei dirmod2(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	direntry *df, *found=NULL;
	fpos pos;
	
	if(nodevalid(fsptr,dir)<NODEI_LINKD || nodetbl[dir].mode!=DIRMODE) return NONODE;
	if(node!=NONODE && rename==NULL && nodevalid(fsptr,node)<NODEI_GOOD) return NONODE;
	if(*name=='\0' || (rename!=NULL && node==NONODE && *rename=='\0')) return NONODE;
	
	loadpos(fsptr,&pos,dir);	
	while(pos.data!=NULLOFF){
		df=(direntry*)B2P(pos.dblk);
		if(df[pos.dpos].node==NONODE) break;
		printf("%s\n",df[pos.dpos].name);
		if(rename!=NULL && namepatheq(df[pos.dpos].name,rename)){
			return NONODE;
		}if(namepatheq(df[pos.dpos].name,name)){
			if(rename!=NULL) found=&df[pos.dpos];
			else return df[pos.dpos].node;
		}seek(fsptr,&pos,1);
	}if(node==NONODE){
		if(rename!=NULL && found!=NULL){
			namepathset(found->name,rename);
			return found->node;
		}return NONODE;
	}

	if(pos.data==NULLOFF){
		offblock *offs;
		if(pos.oblk==NULLOFF){
			if(pos.opos==OFFS_NODE){
				if(blkalloc(fsptr,1,&oblk)==0){
					return NONODE;
				}if(blkalloc(fsptr,1,&dblk)==0){
					blkfree(fsptr,1,&oblk);
					return NONODE;
				}nodetbl[dir].blocklist=oblk;
				offs=(offblock*)B2P(oblk);
				offs->blocks[0]=dblk;
				offs->blocks[1]=NULLOFF;
				offs->next=NULLOFF;
			}else{
				if(blkalloc(fsptr,1,&dblk)==0){
					return NONODE;
				}nodetbl[dir].blocks[block]=dblk;
				if(block<OFFS_NODE-1) nodetbl[dir].blocks[block+1]=NULLOFF;
			}
		}else{
			offs=(offblock*)B2P(oblk);
			if(block==OFFS_BLOCK){
				if(blkalloc(fsptr,1,&oblk)==0){
					return NONODE;
				}if(blkalloc(fsptr,1,&dblk)==0){
					blkfree(fsptr,1,&oblk);
					return NONODE;
				}offs->next=oblk;
				offs=(offblock*)B2P(oblk);
				offs->next=NULLOFF;
				block=0;
			}else{
				if(blkalloc(fsptr,1,&dblk)==0){
					return NONODE;
				}
			}offs->blocks[block]=dblk;
			offs->blocks[1]=NULLOFF;
		}nodetbl[dir].nblocks++;
		df=(direntry*)B2P(dblk);
	}nodetbl[dir].size++;
	df[entry].node=node;
	namepathset(df[entry].name,name);
	nodetbl[node].nlinks++;
	if(++entry<FILES_DIR) df[entry].node=NONODE;
	return node;
	/*	if(oblk==NULLOFF){
			if(block==OFFS_NODE){
				if((oblk=nodetbl[dir].blocklist)==NULLOFF){
					dblk=NULLOFF;
				}else{
					offblock *offs=B2P(oblk);
					dblk=offs->blocks[block=0];
				}
			}else dblk=nodetbl[dir].blocks[block];
		}else{
			offblock *offs=B2P(oblk);
			if(block==OFFS_BLOCK){
				if(offs->next==NULLOFF) dblk=NULLOFF;
				else{
					prevo=oblk;
					oblk=offs->next;
					offs=(offblock*)B2P(oblk);
					dblk=offs->blocks[block=0];
				}
			}else dblk=offs->blocks[block];
		}
	*//*
	
}*/
nodei dirmod(void *fsptr, nodei dir, const char *name, nodei node, const char *rename)
{
	fsheader *fshead=fsptr;
	inode *nodetbl=O2P(fshead->nodetbl);
	blkset oblk=NULLOFF, prevo=NULLOFF;
	blkset dblk=nodetbl[dir].blocks[0];
	direntry *df, *found=NULL;
	blkdex block=0, entry=0;
	
	if(nodevalid(fsptr,dir)<NODEI_LINKD || nodetbl[dir].mode!=DIRMODE) return NONODE;
	if(node!=NONODE && rename==NULL && nodevalid(fsptr,node)<NODEI_GOOD) return NONODE;
	if(*name=='\0' || (rename!=NULL && node==NONODE && *rename=='\0')) return NONODE;
	
	while(dblk!=NULLOFF){
		df=(direntry*)B2P(dblk);
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
					offblock *offs=B2P(oblk);
					dblk=offs->blocks[block=0];
				}
			}else dblk=nodetbl[dir].blocks[block];
		}else{
			offblock *offs=B2P(oblk);
			if(block==OFFS_BLOCK){
				if(offs->next==NULLOFF) dblk=NULLOFF;
				else{
					prevo=oblk;
					oblk=offs->next;
					offs=(offblock*)B2P(oblk);
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
		if(nodetbl[node].mode==DIRMODE && nodetbl[node].nlinks==1 && nodetbl[node].size>0) return NONODE;
		if(dblk!=NULLOFF) entry--;
		else{
			if(oblk==NULLOFF){
				dblk=nodetbl[dir].blocks[--block];
			}else{
				offblock *offs=B2P(oblk);
				dblk=offs->blocks[--block];
			}entry=FILES_DIR-1;
		}df=(direntry*)B2P(dblk);
		found->node=df[entry].node;
		namepathset(found->name,df[entry].name);
		df[entry].node=NONODE;
		if(entry==0){
			if(oblk==NULLOFF){
				blkfree(fsptr,1,&(nodetbl[dir].blocks[block]));
			}else{
				offblock *offs=B2P(oblk);
				blkfree(fsptr,1,&(offs->blocks[block]));
				if(block==0){
					if(prevo==NULLOFF){
						blkfree(fsptr,1,&(nodetbl[dir].blocklist));
					}else{
						offs=(offblock*)B2P(prevo);
						blkfree(fsptr,1,&(offs->next));
					}
				}
			}nodetbl[dir].nblocks--;
		}nodetbl[dir].size--;
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
				offs=(offblock*)B2P(oblk);
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
			offs=(offblock*)B2P(oblk);
			if(block==OFFS_BLOCK){
				if(blkalloc(fsptr,1,&oblk)==0){
					//errno
					return NONODE;
				}if(blkalloc(fsptr,1,&dblk)==0){
					//errno
					blkfree(fsptr,1,&oblk);
					return NONODE;
				}offs->next=oblk;
				offs=(offblock*)B2P(oblk);
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
		df=(direntry*)B2P(dblk);
	}nodetbl[dir].size++;
	df[entry].node=node;
	namepathset(df[entry].name,name);
	nodetbl[node].nlinks++;
	if(++entry<FILES_DIR) df[entry].node=NONODE;
	return node;
}

nodei path2node(void *fsptr, const char *path, const char **child)
{
	nodei node=0;
	size_t sub=1, ch=1;
	
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
	struct timespec creation;
	
	if(fssize<2*BLKSZ) return -1;
	if(fshead->size==fssize/BLKSZ) return 0;
	if(timespec_get(&creation,TIME_UTC)!=TIME_UTC) return -1;
	
	fshead->ntsize=(BLOCKS_FILE*(1+NODES_BLOCK)+fssize/BLKSZ)/(1+BLOCKS_FILE*NODES_BLOCK);
	fshead->nodetbl=sizeof(inode);
	fshead->freelist=fshead->ntsize;
	fshead->free=fssize/BLKSZ-fshead->ntsize;
	
	fhead=(freereg*)B2P(fshead->freelist);
	fhead->size=fshead->free;
	fhead->next=NULLOFF;
	
	nodetbl=(inode*)O2P(fshead->nodetbl);
	memset(nodetbl,0,fshead->ntsize*BLKSZ-sizeof(inode));
	nodetbl[0].mode=DIRMODE;
	nodetbl[0].ctime=creation;
	nodetbl[0].mtime=creation;
	nodetbl[0].nlinks=1;
	
	fshead->size=fssize/BLKSZ;
	return 0;
}
