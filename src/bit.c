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

/* population count, Donald Knuth SWAR style */
/* as described on CWP */
/* http://chessprogramming.wikispaces.com/Population+Count#SWAR-Popcount */
s32 popcount(u64 x) 
{
  x =  x                        - ((x >> 1)  & 0x5555555555555555);
  x = (x & 0x3333333333333333)  + ((x >> 2)  & 0x3333333333333333);
  x = (x                        +  (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
  x = (x * 0x0101010101010101) >> 56;
  return (s32)x;
}
/*  pre condition: x != 0; */
s32 first1(u64 x)
{
  return popcount((x&-x)-1);
}
/*  pre condition: x != 0; */
s32 popfirst1(u64 *a)
{
  u64 b = *a;
  *a &= (*a-1);  /* clear lsb  */
  return popcount((b&-b)-1); /* return pop count of isolated lsb */
}
/* Sqaures in between, pure calculation */
/* http://chessprogramming.wikispaces.com/Square+Attacked+By#Legality */
u64 sqinbetween(Square sq1, Square sq2)
{
  const u64 m1   = 0xFFFFFFFFFFFFFFFF;
  const u64 a2a7 = 0x0001010101010100;
  const u64 b2g7 = 0x0040201008040200;
  const u64 h1b7 = 0x0002040810204080;
  u64 btwn, line, rank, file;

  btwn  = (m1 << sq1) ^ (m1 << sq2);
  file  =   (sq2 & 7) - (sq1   & 7);
  rank  =  ((sq2 | 7) -  sq1) >> 3 ;
  line  =      (   (file  &  7) - 1) & a2a7; // a2a7 if same file
  line += 2 * ((   (rank  &  7) - 1) >> 58); // b1g1 if same rank
  line += (((rank - file) & 15) - 1) & b2g7; // b2g7 if same diagonal
  line += (((rank + file) & 15) - 1) & h1b7; // h1b7 if same antidiag
  line *= btwn & -btwn; // mul acts like shift by smaller square
  return line & btwn;   // return the bits on that line inbetween
}
/* bit twiddling
  bb_work=bb_temp&-bb_temp;  // get lsb 
  bb_temp&=bb_temp-1;       // clear lsb
*/

