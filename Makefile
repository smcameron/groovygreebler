
MYCFLAGS=-g -Wall --pedantic -fsanitize=address

PNGLIBS:=$(shell pkg-config --libs libpng)
PNGCFLAGS:=$(shell pkg-config --cflags libpng)

all:	groovygreebler

mathutils.o:	mathutils.c mathutils.h Makefile
	$(CC) ${MYCFLAGS} -c mathutils.c

mtwist.o:	mtwist.c mtwist.h Makefile
	$(CC) ${MYCFLAGS} -c mtwist.c

quat.o:	quat.c quat.h mathutils.h Makefile
	$(CC) ${MYCFLAGS} -c quat.c

png_utils.o:	png_utils.c png_utils.h
	$(CC) ${MYCFLAGS} ${PNGCFLAGS} -c png_utils.c

groovygreebler:	groovygreebler.c mtwist.o quat.o mathutils.o png_utils.o Makefile
	$(CC) ${MYCFLAGS} -o groovygreebler groovygreebler.c mtwist.o quat.o mathutils.o png_utils.o -lm ${PNGLIBS}

clean:
	rm -f *.o groovygreebler

