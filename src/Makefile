#	Makefile for zeta

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -std=c99

TARGETS=zeta

zeta: 
	xxd -i zeta.cl zetacl.h
	sed -i 's/unsigned int/const size_t/g' ./zetacl.h
	sed -i 's/unsigned char/const char/g' ./zetacl.h
	$(CC) $(CFLAGS) -o zeta bitboard.c clconfig.c clrun.c timer.c zeta.c -I. -L. -lOpenCL 


clean:
	rm -f ${TARGETS}

