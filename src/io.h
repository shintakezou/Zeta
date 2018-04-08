/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2018-03-25
  Updated at:   2018
  License:      GPL >= v2

  Copyright (C) 2011-2018 Srdja Matovic

  Zeta is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  Zeta is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#include "types.h"        // types and defaults and macros 

#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

void printbitboard(Bitboard board);
void printmove(Move move);
Move can2move(char *usermove, Bitboard *board, bool stm);
void move2can(Move move, char * movec);
void printmovecan(Move move);
void printboard(Bitboard *board);
void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply);
bool setboard(Bitboard *board, char *fenstring);
bool read_and_init_config(char configfile[]);

#endif /* IO_H_INCLUDED */

