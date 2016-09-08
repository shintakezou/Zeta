#	Makefile for zeta

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -std=c99 -Wno-unused-parameter

TARGETS=zeta

zeta: 
	$(CC) $(CFLAGS) -o zeta bitboard.c clconfig.c clrun.c zeta.c -I. -L. -lm -lOpenCL 

clean:
	rm -f ${TARGETS}

