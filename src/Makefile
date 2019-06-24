#	Makefile for zeta

CC=gcc
CFLAGS=-Wall -Wextra -std=c99
CLIBS=-lm -I../CL/ -lOpenCL
#CC=/usr/bin/i686-w64-mingw32-gcc -L../libs/
#CLIBS=-lm -I../CL/ -L../libs/ -lOpenCLWIN32
#CC=/usr/bin/x86_64-w64-mingw32-gcc  -L../libs/
#CLIBS=-lm -I../CL/ -L../libs/ -lOpenCLWIN64

TARGETS=zeta

zeta: 
	# .cl files as .h string include
	xxd -i zetaperft.cl zetaclperft.h
	sed -i 's/unsigned int/const size_t/g' ./zetaclperft.h
	sed -i 's/unsigned char/const char/g' ./zetaclperft.h
	xxd -i zeta.cl zetacl.h
	sed -i 's/unsigned int/const size_t/g' ./zetacl.h
	sed -i 's/unsigned char/const char/g' ./zetacl.h
	$(CC) $(CFLAGS) -o zeta bit.c bench.c bitboard.c clconfig.c clquery.c clrun.c io.c search.c test.c timer.c xboard.c zeta.c $(CLIBS)

clean:
	rm -f zetacl.h
	rm -f zetaclperft.h
	rm -f ${TARGETS}

