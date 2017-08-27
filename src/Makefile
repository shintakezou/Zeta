#	Makefile for zeta

CC=gcc
#CC=/usr/bin/i686-w64-mingw32-gcc
#CC=/usr/bin/x86_64-w64-mingw32-gcc
CFLAGS=-Wall -Wextra -std=c99

TARGETS=zeta

zeta: 
	xxd -i zetaperft.cl zetaclperft.h
	sed -i 's/unsigned int/const size_t/g' ./zetaclperft.h
	sed -i 's/unsigned char/const char/g' ./zetaclperft.h
	xxd -i zeta1stgen.cl zetacl1stgen.h
	sed -i 's/unsigned int/const size_t/g' ./zetacl1stgen.h
	sed -i 's/unsigned char/const char/g' ./zetacl1stgen.h
	xxd -i zeta2ndgen.cl zetacl2ndgen.h
	sed -i 's/unsigned int/const size_t/g' ./zetacl2ndgen.h
	sed -i 's/unsigned char/const char/g' ./zetacl2ndgen.h
	xxd -i zeta3rdgen.cl zetacl3rdgen.h
	sed -i 's/unsigned int/const size_t/g' ./zetacl3rdgen.h
	sed -i 's/unsigned char/const char/g' ./zetacl3rdgen.h
	$(CC) $(CFLAGS) -o zeta bitboard.c clconfig.c clquery.c clrun.c timer.c zeta.c -lm -I. -L. -lOpenCL 


clean:
	rm -f zetaclperft.h
	rm -f zetacl1stgen.h
	rm -f zetacl2ndgen.h
	rm -f zetacl3rdgen.h
	rm -f ${TARGETS}

