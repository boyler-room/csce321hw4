CC=gcc
CFLAGS=-Wall `pkg-config fuse --cflags --libs`
DFLAGS=-g -O0
BDIR=./build

#default: myfs.c implementation.c
#	gcc -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs
#debug: myfs.c implementation.c
#	gcc -g -O0 -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs
.PHONY: default debug test clean

default: myfs.c $(BDIR)/implementation.o
	$(CC) -o $(BDIR)/myfs $^ $(CFLAGS)

debug: CFLAGS+=$(DFLAGS)
debug: myfs.c $(BDIR)/implementation.o
	$(CC) -o $(BDIR)/myfs $^ $(CFLAGS)

test: fstst.c $(BDIR)/myfs_helper.o
	$(CC) -o $(BDIR)/fstst $^ $(CFLAGS)

$(BDIR)/implementation.o: implementation.c $(BDIR)/myfs_helper.o
	$(CC) -c -o $@ $^ $(CFLAGS)

$(BDIR)/myfs_helper.o: myfs_helper.c myfs_helper.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm $(BDIR)/*
