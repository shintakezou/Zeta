/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2016-09
  License:      GPL >= v2

  Copyright (C) 2011-2016 Srdja Matovic

  Zeta is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  Zeta is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

#include <inttypes.h>   // for nice u64 printf 
 
#include "CL/cl.h"    // for OpenCL data types etc.

// OpenCL data types to own
typedef cl_ulong U64;
typedef cl_uint U32;
typedef cl_int S32;
typedef cl_uchar U8;
typedef cl_bool bool;
// boolean val
#define true  1
#define false 0

typedef U64 Move;
typedef U64 Bitboard;
typedef U32 Cr;
typedef U64 Hash;

typedef S32 Score;
typedef U8 Square;
typedef U8 Piece;

typedef struct {
    Move move;
    Score score;
    S32 lock;
    S32 visits;
    S32 child;
    S32 children;
    S32 parent;
}  NodeBlock;

#define VERSION "098e"

// engine defaults
#define MAXGAMEPLY  1024    // max ply a game can reach
#define MAXMOVES    256     // max amount of legal moves per position
#define TIMESPARE   100     // 100 milliseconds spare
// colors
#define WHITE 0
#define BLACK 1
// scores
#define INF             1000000
#define MATESCORE        999000
#define DRAWSCORE       0
#define STALEMATESCORE  0
// limit
#define MAXGAMEPLY      1024    // max ply a game can reach
#define MAXMOVES        256     // max amount of legal moves per position
// piece encodings
#define PNONE   0
#define PAWN    1
#define KNIGHT  2
#define KING    3
#define BISHOP  4
#define ROOK    5
#define QUEEN   6
// defaults
#define ILL     64          // illegal sqaure, for castle square hack
// bitboard masks, computation prefered over lookup
#define SETMASKBB(sq)       (1ULL<<(sq))
#define CLRMASKBB(sq)       (~(1ULL<<(sq)))
// u64 defaults 
#define BBEMPTY             0x0000000000000000ULL
#define BBFULL              0xFFFFFFFFFFFFFFFFULL
#define MOVENONE            0x0000000000000000ULL
#define HASHNONE            0x0000000000000000ULL
#define CRNONE              0x0000000000000000ULL
#define SCORENONE           0x0000000000000000ULL
// square helpers
#define MAKESQ(file,rank)   ((rank)<<3|(file))
#define GETRANK(sq)         ((sq)>>3)
#define GETFILE(sq)         ((sq)&7)
#define GETRRANK(sq,color)  ((color)?(((sq)>>3)^7):((sq)>>3))
#define FLIP(sq)            ((sq)^7)
#define FLOP(sq)            ((sq)^56)
#define FLIPFLOP(sq)        (((sq)^56)^7)
// piece helpers 
#define GETPIECE(board,sq)  ( \
                               ((board[0]>>(sq))&0x1)\
                           |  (((board[1]>>(sq))&0x1)<<1) \
                           |  (((board[2]>>(sq))&0x1)<<2) \
                           |  (((board[3]>>(sq))&0x1)<<3) \
                             )
#define GETPIECETYPE(board,sq) ( \
                              (((board[1]>>(sq))&0x1)) \
                           |  (((board[2]>>(sq))&0x1)<<1) \
                           |  (((board[3]>>(sq))&0x1)<<2) \
                             )
// file enumeration
enum Files
{
  FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NONE
};
#define BBFILEA             0x0101010101010101
#define BBFILEB             0x0202020202020202
#define BBFILEC             0x0404040404040404
#define BBFILED             0x0808080808080808
#define BBFILEE             0x1010101010101010
#define BBFILEF             0x2020202020202020
#define BBFILEG             0x4040404040404040
#define BBFILEH             0x8080808080808080
#define BBNOTHFILE          0x7F7F7F7F7F7F7F7F
#define BBNOTAFILE          0xFEFEFEFEFEFEFEFE
// rank enumeration
enum Ranks
{
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NONE
};
#define BBRANK8             0xFF00000000000000
#define BBRANK7             0x00FF000000000000
#define BBRANK5             0x000000FF00000000
#define BBRANK4             0x00000000FF000000
#define BBRANK2             0x000000000000FF00
#define BBRANK1             0x00000000000000FF
#define BBA1H8              0x8040201008040201
#define BBA8H1              0x0102040810204080
// square enumeration 
enum Squares
{
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8
};
// is score a mate in n 
#define ISMATE(val)           ((((val)>MATESCORE&&(val)<INF)||((val)<-MATESCORE&&(val)>-INF))?true:false)
// is score default inf
#define ISINF(val)            (((val)==INF||(val)==-INF)?true:false)

#endif // TYPES_H_INCLUDED

