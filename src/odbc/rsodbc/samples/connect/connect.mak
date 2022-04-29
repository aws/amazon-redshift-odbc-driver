CC=gcc
LIBC=-lc
CCFLAGS=-g


DEFS=-DODBCVER=0x0350 -DODBC64 -DLINUX
INCLUDE=-I..

LD=ld

LIBPATH=-L..

LIBS=$(LIBPATH) -lodbc 


connect:	connect.c
	$(CC) -o $@ $(DEFS) $(CCFLAGS) $(INCLUDE) connect.c $(LIBS) $(LIBC)

clean:
	rm connect
