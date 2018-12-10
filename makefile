CC=gcc
CFLAGS=-Wall `pkg-config fuse --cflags --libs`
DFLAGS=-g -O0
BDIR=./build

.PHONY: default debug test clean

#build fuse version
default: myfs.c $(BDIR)/implementation.o $(BDIR)/myfs_helper.o
	$(CC) -o $(BDIR)/myfs $^ $(CFLAGS)

#build fuse version for debugging
debug: CFLAGS+=$(DFLAGS)
debug: myfs.c $(BDIR)/implementation.o $(BDIR)/myfs_helper.o
	$(CC) -o $(BDIR)/myfs $^ $(CFLAGS)

#build test version
test: fstst.c $(BDIR)/myfs_helper.o
	$(CC) -o $(BDIR)/fstst $^ $(CFLAGS)

$(BDIR)/implementation.o: implementation.c myfs_helper.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(BDIR)/myfs_helper.o: myfs_helper.c myfs_helper.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm $(BDIR)/*
