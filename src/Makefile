#	Makefile for zeta

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -std=c99

TARGETS=zeta

zeta: 
	# .cl files as .h string include
	xxd -i zetaperft.cl zetaclperft.h
	sed -i 's/unsigned int/const size_t/g' ./zetaclperft.h
	sed -i 's/unsigned char/const char/g' ./zetaclperft.h
	xxd -i zeta.cl zetacl.h
	sed -i 's/unsigned int/const size_t/g' ./zetacl.h
	sed -i 's/unsigned char/const char/g' ./zetacl.h
	$(CC) $(CFLAGS) -o zeta bit.c bench.c bitboard.c clconfig.c clquery.c clrun.c io.c search.c test.c timer.c xboard.c zeta.c -lm -I. -L. -lOpenCL 

clean:
	rm -f zetacl.h
	rm -f zetaclperft.h
	rm -f ${TARGETS}

