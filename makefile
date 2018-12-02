default: myfs.c implementation.c
	gcc -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs
debug: myfs.c implementation.c
	gcc -g -O0 -Wall myfs.c implementation.c `pkg-config fuse --cflags --libs` -o myfs
test: fstst.c
	gcc -Wall -o fstst fstst.c
clean:
	rm fstst myfs 
