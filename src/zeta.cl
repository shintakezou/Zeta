/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
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

// 1st generation gpgpu devices
// for setting termination flag
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics         : enable

// 2nd generation gpgpu devices
// for collecting movecounter and score via local atomics
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics          : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics      : enable

// 3rd generation gpgpu devices
// for collecting bbAttacks and Hash
#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics            : enable

// otpional
//#pragma OPENCL EXTENSION cl_khr_byte_addressable_store          : enable

// hasck for nvidia OpenCL 1.0 devices
#if __OPENCL_VERSION__ < 110
#define OLDSCHOOL
#endif

typedef ulong   u64;
typedef uint    u32;
typedef int     s32;
typedef short   s16;
typedef uchar    u8;
typedef char     s8;

typedef u64     Bitboard;
typedef u64     Cr;
typedef u64     Hash;

typedef u32     Move;
typedef u32     TTMove;
typedef s32     Score;
typedef s16     TTScore;
typedef u8      Square;
typedef u8      Piece;


// transposition table entry
typedef struct
{
  Hash hash;
  Move bestmove;
  TTScore score;
  u8 flag;
  u8 depth;
} TTE;
// abdada transposition table entry
typedef struct
{
  Hash hash;
  s32 lock;     // s32 needed for global atomics
  s32 ply;      // s32 needed for global atomics
  s32 sd;       // s32 needed for global atomics
  TTScore score;
  s16 depth;
} ABDADATTE;
// tunebale search params
#define LMRR            1 // late move reduction 
#define NULLR           2 // null move reduction 
#define RANDBRO         1 // how many brothers searched before randomized order
#define RANDABDADA   true // abdada, rand move order 
#define RANDWORKERS    64 // abdada, at how many workers to randomize move order
// TT node type flags
#define FAILLOW         0
#define EXACTSCORE      1
#define FAILHIGH        2
// iterative search modes
#define SEARCH          1
#define NULLMOVESEARCH  2
#define LMRSEARCH       4
#define IIDSEARCH       8
// node states
#define STATENONE       0
#define KIC             1
#define QS              2
#define EXT             4
#define LMR             8
#define IID            16
#define IIDDONE        32
// ABDADA modes
#define ITER1          64
#define ITER2         128
// defaults
#define VERSION      "099k"
// quad bitboard array index definition
#define QBBBLACK        0     // pieces white
#define QBBP1           1     // piece type first bit
#define QBBP2           2     // piece type second bit
#define QBBP3           3     // piece type third bit
#define QBBPMVD         4     // piece moved flags, for castle rights
#define QBBHASH         5     // 64 bit board Zobrist hash
#define QBBHMC          6     // half move clock
/* move encoding 
   0  -  5  square from
   6  - 11  square to
  12  - 17  square capture
  18  - 21  piece from
  22  - 25  piece to
  26  - 29  piece capture
*/
// engine defaults
#define MAXPLY              64      // max internal search ply
#define MAXGAMEPLY          1024    // max ply a game can reach
#define MAXMOVES            256     // max amount of legal moves per position
// colors
#define BLACK               1
#define WHITE               0
// score indexing
#define ALPHA               0
#define BETA                1
// scores
#define INF                 32000
#define MATESCORE           30000
#define DRAWSCORE           0
#define STALEMATESCORE      0
#define INFMOVESCORE        0x7FFFFFFF
// piece type enumeration
#define PNONE               0
#define PAWN                1
#define KNIGHT              2
#define KING                3
#define BISHOP              4
#define ROOK                5
#define QUEEN               6
// bitboard masks, computation prefered over lookup
#define SETMASKBB(sq)       (1UL<<(sq))
#define CLRMASKBB(sq)       (~(1UL<<(sq)))
// u64 defaults
#define BBEMPTY             0x0000000000000000UL
#define BBFULL              0xFFFFFFFFFFFFFFFFUL
#define MOVENONE            0x0000000000000000UL
#define NULLMOVE            0x0000000000000041UL
#define HASHNONE            0x0000000000000000UL
#define CRNONE              0x0000000000000000UL
#define SCORENONE           0x0000000000000000UL
// set masks
#define SMMOVE              0x0000003FFFFFFFFFUL
#define SMCRALL             0x8900000000000091UL
// clear masks
#define CMMOVE              0xFFFFFFC000000000UL
#define CMCRALL             0x76FFFFFFFFFFFF6EUL
// castle right masks
#define SMCRWHITE           0x0000000000000091UL
#define SMCRWHITEQ          0x0000000000000011UL
#define SMCRWHITEK          0x0000000000000090UL
#define SMCRBLACK           0x9100000000000000UL
#define SMCRBLACKQ          0x1100000000000000UL
#define SMCRBLACKK          0x9000000000000000UL
// move helpers
#define MAKEPIECE(p,c)     ((((Piece)p)<<1)|(Piece)c)
#define JUSTMOVE(move)     (move&SMMOVE)
#define GETCOLOR(p)        ((p)&0x1)
#define GETPTYPE(p)        (((p)>>1)&0x7)      // 3 bit piece type encoding
#define GETSQFROM(mv)      ((mv)&0x3F)         // 6 bit square
#define GETSQTO(mv)        (((mv)>>6)&0x3F)    // 6 bit square
#define GETSQCPT(mv)       (((mv)>>12)&0x3F)   // 6 bit square
#define GETPFROM(mv)       (((mv)>>18)&0xF)    // 4 bit piece encoding
#define GETPTO(mv)         (((mv)>>22)&0xF)    // 4 bit piece encoding
#define GETPCPT(mv)        (((mv)>>26)&0xF)    // 4 bit piece encodinge
// pack move into 32 bits
#define MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt) \
( \
     sqfrom      | (sqto<<6)  | (sqcpt<<12) \
  | (pfrom<<18)  | (pto<<22)  | (pcpt<<26) \
)
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
                           |  (((board[QBBP1]>>(sq))&0x1)<<1) \
                           |  (((board[QBBP2]>>(sq))&0x1)<<2) \
                           |  (((board[QBBP3]>>(sq))&0x1)<<3) \
                             )
#define GETPIECETYPE(board,sq) ( \
                              (((board[QBBP1]>>(sq))&0x1)) \
                           |  (((board[QBBP2]>>(sq))&0x1)<<1) \
                           |  (((board[QBBP3]>>(sq))&0x1)<<2) \
                             )
// file enumeration
#define FILE_A              0
#define FILE_B              1
#define FILE_C              2
#define FILE_D              3
#define FILE_E              4
#define FILE_F              5
#define FILE_G              6
#define FILE_H              7
// file bitmasks
#define BBFILEA             0x0101010101010101UL
#define BBFILEB             0x0202020202020202UL
#define BBFILEC             0x0404040404040404UL
#define BBFILED             0x0808080808080808UL
#define BBFILEE             0x1010101010101010UL
#define BBFILEF             0x2020202020202020UL
#define BBFILEG             0x4040404040404040UL
#define BBFILEH             0x8080808080808080UL
#define BBNOTHFILE          0x7F7F7F7F7F7F7F7FUL
#define BBNOTAFILE          0xFEFEFEFEFEFEFEFEUL
// rank enumeration
#define RANK_1              0
#define RANK_2              1
#define RANK_3              2
#define RANK_4              3
#define RANK_5              4
#define RANK_6              5
#define RANK_7              6
#define RANK_8              7
// rank bitmasks
#define BBRANK7             0x00FF000000000000UL
#define BBRANK5             0x000000FF00000000UL
#define BBRANK4             0x00000000FF000000UL
#define BBRANK2             0x000000000000FF00UL
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
// is score a draw, unprecise
#define ISDRAW(val)   ((val==DRAWSCORE)?true:false)
// is score a mate in n
#define ISMATE(val)   ((((val)>MATESCORE&&(val)<INF)||((val)<-MATESCORE&&(val)>-INF))?true:false)
// is score default inf
#define ISINF(val)    (((val)==INF||(val)==-INF)?true:false)
// rotate left based zobrist hashing
__constant Hash Zobrist[18]=
{
  0x9D39247E33776D41, 0x2AF7398005AAA5C7, 0x44DB015024623547, 0x9C15F73E62A76AE2,
  0x75834465489C0C89, 0x3290AC3A203001BF, 0x0FBBAD1F61042279, 0xE83A908FF2FB60CA,
  0x0D7E765D58755C10, 0x1A083822CEAFE02D, 0x9605D5F0E25EC3B0, 0xD021FF5CD13A2ED5,
  0x40BDF15D4A672E32, 0x011355146FD56395, 0x5DB4832046F3D9E5, 0x239F8B2D7FF719CC,
  0x05D1A1AE85B49AA1, 0x679F848F6E8FC971
};
//  piece square tables based on proposal by Tomasz Michniewski
//  https://chessprogramming.wikispaces.com/Simplified+evaluation+function

// piece values
// pnone, pawn, knight, king, bishop, rook, queen
//__constant Score EvalPieceValues[7] = {0, 100, 300, 0, 300, 500, 900};
__constant Score EvalPieceValues[7] = {0, 100, 400, 0, 400, 600, 1200};
// square control bonus, black view
// flop square for white-index: sq^56
__constant Score EvalControl[64] =
{
    0,  0,  5,  5,  5,  5,  0,  0,
    5,  0,  5,  5,  5,  5,  0,  5,
    0,  0, 10,  5,  5, 10,  0,  0,
    0,  5,  5, 10, 10,  5,  5,  0,
    0,  5,  5, 10, 10,  5,  5,  0,
    0,  0, 10,  5,  5, 10,  0,  0,
    0,  0,  5,  5,  5,  5,  0,  0,
    0,  0,  5,  5,  5,  5,  0,  0
};
// piece square tables, black view
// flop square for white-index: sq^56
__constant Score EvalTable[7*64] =
{
    // piece none 
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,

    // pawn
    0,  0,  0,  0,  0,  0,  0,  0,
   50, 50, 50, 50, 50, 50, 50, 50,
   30, 30, 30, 30, 30, 30, 30, 30,
    5,  5,  5, 10, 10,  5,  5,  5,
    3,  3,  3,  8,  8,  3,  3,  3,
    2,  2,  2,  2,  2,  2,  2,  2,
    0,  0,  0, -5, -5,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,

     // knight
  -50,-40,-30,-30,-30,-30,-40,-50,
  -40,-20,  0,  0,  0,  0,-20,-40,
  -30,  0, 10, 15, 15, 10,  0,-30,
  -30,  5, 15, 20, 20, 15,  5,-30,
  -30,  0, 15, 20, 20, 15,  0,-30,
  -30,  5, 10, 15, 15, 10,  5,-30,
  -40,-20, 0,   5,  5,  0,-20,-40,
  -50,-40,-30,-30,-30,-30,-40,-50,

    // king 
   -1, -1, -1, -1, -1, -1, -1, -1,
   -1,  0,  0,  0,  0,  0,  0, -1,
   -1,  0,  0,  0,  0,  0,  0, -1,
   -1,  0,  0,  0,  0,  0,  0, -1,
   -1,  0,  0,  0,  0,  0,  0, -1,
   -1,  0,  0,  0,  0,  0,  0, -1,
   -1,  0,  0,  0,  0,  0,  0, -1,
   -1, -1, -1, -1, -1, -1, -1, -1,

    // bishop
  -20,-10,-10,-10,-10,-10,-10,-20,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5, 10, 10,  5,  0,-10,
  -10,  5,  5, 10, 10,  5,  5,-10,
  -10,  0, 10, 10, 10, 10,  0,-10,
  -10, 10, 10, 10, 10, 10, 10,-10,
  -10,  5,  0,  0,  0,  0,  5,-10,
  -20,-10,-10,-10,-10,-10,-10,-20,

    // rook
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0,

    // queen
  -20,-10,-10, -5, -5,-10,-10,-20,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5,  5,  5,  5,  0,-10,
   -5,  0,  5,  5,  5,  5,  0, -5,
   -5,  0,  5,  5,  5 , 5 , 0, -5,
  -10,  0,  5,  5,  5,  5,  0,-10,
  -10,  0,  0,  0,  0 , 0 , 0,-10,
  -20,-10,-10, -5, -5,-10,-10,-20
};
// OpenCL 1.2 has popcount function
#if __OPENCL_VERSION__ < 120
// population count, Donald Knuth SWAR style
// as described on CWP
// http://chessprogramming.wikispaces.com/Population+Count#SWAR-Popcount
u8 count1s(u64 x) 
{
  x =  x                        - ((x >> 1)  & 0x5555555555555555);
  x = (x & 0x3333333333333333)  + ((x >> 2)  & 0x3333333333333333);
  x = (x                        +  (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
  x = (x * 0x0101010101010101) >> 56;
  return (u8)x;
}
#else
// wrapper
u8 count1s(u64 x) 
{
  return (u8)popcount(x);
}
#endif
//  pre condition: x != 0;
u8 first1(u64 x)
{
  return count1s((x&-x)-1);
}
//  pre condition: x != 0;
u8 popfirst1(u64 *a)
{
  u64 b = *a;
  *a &= (*a-1);  // clear lsb 
  return count1s((b&-b)-1); // return pop count of isolated lsb
}
// bit twiddling hacks
//  bb_work=bb_temp&-bb_temp;  // get lsb 
//  bb_temp&=bb_temp-1;       // clear lsb

/*
  // PRNG
  // xorshift32
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
*/

// apply move on board, quick during move generation
void domovequick(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Bitboard pto    = GETPTO(move);
  Bitboard bbTemp = BBEMPTY;

  // check for edges
  if (move==MOVENONE)
    return;

  // unset square from, square capture and square to
  bbTemp = CLRMASKBB(sqfrom)&CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  // set piece to
  board[QBBBLACK] |= (pto&0x1)<<sqto;
  board[QBBP1]    |= ((pto>>1)&0x1)<<sqto;
  board[QBBP2]    |= ((pto>>2)&0x1)<<sqto;
  board[QBBP3]    |= ((pto>>3)&0x1)<<sqto;
}
// restore board again, quick during move generation
void undomovequick(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Bitboard pfrom  = GETPFROM(move);
  Bitboard pcpt   = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;

  // check for edges
  if (move==MOVENONE)
    return;

  // unset square capture, square to
  bbTemp = CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  // restore piece capture
  board[QBBBLACK] |= (pcpt&0x1)<<sqcpt;
  board[QBBP1]    |= ((pcpt>>1)&0x1)<<sqcpt;
  board[QBBP2]    |= ((pcpt>>2)&0x1)<<sqcpt;
  board[QBBP3]    |= ((pcpt>>3)&0x1)<<sqcpt;

  // restore piece from
  board[QBBBLACK] |= (pfrom&0x1)<<sqfrom;
  board[QBBP1]    |= ((pfrom>>1)&0x1)<<sqfrom;
  board[QBBP2]    |= ((pfrom>>2)&0x1)<<sqfrom;
  board[QBBP3]    |= ((pfrom>>3)&0x1)<<sqfrom;
}
// apply move on board
void domove(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Bitboard pfrom  = GETPFROM(move);
  Bitboard pto    = GETPTO(move);
  Bitboard bbTemp = BBEMPTY;
  Bitboard pcastle= PNONE;

  // check for edges
  if (move==MOVENONE)
    return;

  // check for nullmove
  if (move==NULLMOVE)
    return;

  // unset square from, square capture and square to
  bbTemp = CLRMASKBB(sqfrom)&CLRMASKBB(sqcpt)&CLRMASKBB(sqto);

  // handle castle rook, queenside
  pcastle = (GETPTYPE(pfrom)==KING&&sqfrom-sqto==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
    bbTemp  &= CLRMASKBB(sqfrom-4); // unset castle rook from
  // handle castle rook, kingside
  pcastle = (GETPTYPE(pfrom)==KING&&sqto-sqfrom==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
    bbTemp  &= CLRMASKBB(sqfrom+3); // unset castle rook from

  // unset pieces
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  // set piece to
  board[QBBBLACK] |= (pto&0x1)<<sqto;
  board[QBBP1]    |= ((pto>>1)&0x1)<<sqto;
  board[QBBP2]    |= ((pto>>2)&0x1)<<sqto;
  board[QBBP3]    |= ((pto>>3)&0x1)<<sqto;

  // handle castle rook, queenside
  pcastle = (GETPTYPE(pfrom)==KING&&sqfrom-sqto==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
  {
    // set castle rook to
    board[QBBBLACK] |= (pcastle&0x1)<<(sqto+1);
    board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto+1);
    board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto+1);
    board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto+1);
  }

  // handle castle rook, kingside
  pcastle = (GETPTYPE(pfrom)==KING&&sqto-sqfrom==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
  {
    // set castle rook to
    board[QBBBLACK] |= (pcastle&0x1)<<(sqto-1);
    board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto-1);
    board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto-1);
    board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto-1);
  }
}
// restore board again
void undomove(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Bitboard pfrom  = GETPFROM(move);
  Bitboard pcpt   = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;
  Bitboard pcastle= PNONE;

  // check for edges
  if (move==MOVENONE)
    return;

  // check for nullmove
  if (move==NULLMOVE)
    return;

  // unset square capture, square to
  bbTemp = CLRMASKBB(sqcpt)&CLRMASKBB(sqto);

  // handle castle rook, queenside
  pcastle = (GETPTYPE(pfrom)==KING&&sqfrom-sqto==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
    bbTemp  &= CLRMASKBB(sqto+1); // unset castle rook to
  // handle castle rook, kingside
  pcastle = (GETPTYPE(pfrom)==KING&&sqto-sqfrom==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
    bbTemp  &= CLRMASKBB(sqto-1); // restore castle rook from

  // unset pieces
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  // restore piece capture
  board[QBBBLACK] |= (pcpt&0x1)<<sqcpt;
  board[QBBP1]    |= ((pcpt>>1)&0x1)<<sqcpt;
  board[QBBP2]    |= ((pcpt>>2)&0x1)<<sqcpt;
  board[QBBP3]    |= ((pcpt>>3)&0x1)<<sqcpt;

  // restore piece from
  board[QBBBLACK] |= (pfrom&0x1)<<sqfrom;
  board[QBBP1]    |= ((pfrom>>1)&0x1)<<sqfrom;
  board[QBBP2]    |= ((pfrom>>2)&0x1)<<sqfrom;
  board[QBBP3]    |= ((pfrom>>3)&0x1)<<sqfrom;

  // handle castle rook, queenside
  pcastle = (GETPTYPE(pfrom)==KING&&sqfrom-sqto==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
  {
    // restore castle rook from
    board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom-4);
    board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom-4);
    board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom-4);
    board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom-4);
  }
  // handle castle rook, kingside
  pcastle = (GETPTYPE(pfrom)==KING&&sqto-sqfrom==2)?
              MAKEPIECE(ROOK,GETCOLOR(pfrom))
             :PNONE;
  if (pcastle)
  {
    // set castle rook to
    board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom+3);
    board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom+3);
    board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom+3);
    board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom+3);
  }
}
Hash computehash(__private Bitboard *board, bool stm, Bitboard bbCR)
{
  Piece piece;
  Bitboard bbWork;
  Square sq;
  Square sqep = 0x0;
  Hash hash = HASHNONE;
  Hash zobrist;
  u8 side;

  // for each color
  for (side=WHITE;side<=BLACK;side++)
  {
    bbWork = (side==BLACK)?
                board[QBBBLACK]
               :(board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]));
    // for each piece
    while(bbWork)
    {
      sq    = popfirst1(&bbWork);
      piece = GETPIECE(board,sq);
      zobrist = Zobrist[GETCOLOR(piece)*6+GETPTYPE(piece)-1];
      hash ^= ((zobrist<<sq)|(zobrist>>(64-sq)));; // rotate left 64
    }
  }
  // castle rights
  if ((~bbCR)&SMCRWHITEQ)
    hash^= Zobrist[12];
  if ((~bbCR)&SMCRWHITEK)
    hash^= Zobrist[13];
  if ((~bbCR)&SMCRBLACKQ)
    hash^= Zobrist[14];
  if ((~bbCR)&SMCRBLACKK)
    hash^= Zobrist[15];

  // en passant flag
  bbWork = (~bbCR&0x000000FFFF000000);
  sqep = (bbWork)?first1(bbWork):0x0;

  if (sqep)
    hash ^= ((Zobrist[16]<<GETFILE(sqep))|(Zobrist[16]>>(64-GETFILE(sqep))));

  // site to move
  if (stm)
    hash ^= 0x1UL;

  return hash;
}
// precomputed attack tables for move generation and square in check
__constant Bitboard AttackTablesPawnPushes[2*64] = 
{
  // white pawn pushes
  0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x1010000,0x2020000,0x4040000,0x8080000,0x10100000,0x20200000,0x40400000,0x80800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10000000000,0x20000000000,0x40000000000,0x80000000000,0x100000000000,0x200000000000,0x400000000000,0x800000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000,0x100000000000000,0x200000000000000,0x400000000000000,0x800000000000000,0x1000000000000000,0x2000000000000000,0x4000000000000000,0x8000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  // black pawn pushes
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10100000000,0x20200000000,0x40400000000,0x80800000000,0x101000000000,0x202000000000,0x404000000000,0x808000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000
};
// piece attack tables
__constant Bitboard AttackTables[7*64] = 
{
  // white pawn
  0x200,0x500,0xa00,0x1400,0x2800,0x5000,0xa000,0x4000,0x20000,0x50000,0xa0000,0x140000,0x280000,0x500000,0xa00000,0x400000,0x2000000,0x5000000,0xa000000,0x14000000,0x28000000,0x50000000,0xa0000000,0x40000000,0x200000000,0x500000000,0xa00000000,0x1400000000,0x2800000000,0x5000000000,0xa000000000,0x4000000000,0x20000000000,0x50000000000,0xa0000000000,0x140000000000,0x280000000000,0x500000000000,0xa00000000000,0x400000000000,0x2000000000000,0x5000000000000,0xa000000000000,0x14000000000000,0x28000000000000,0x50000000000000,0xa0000000000000,0x40000000000000,0x200000000000000,0x500000000000000,0xa00000000000000,0x1400000000000000,0x2800000000000000,0x5000000000000000,0xa000000000000000,0x4000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  // black pawn
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x5,0xa,0x14,0x28,0x50,0xa0,0x40,0x200,0x500,0xa00,0x1400,0x2800,0x5000,0xa000,0x4000,0x20000,0x50000,0xa0000,0x140000,0x280000,0x500000,0xa00000,0x400000,0x2000000,0x5000000,0xa000000,0x14000000,0x28000000,0x50000000,0xa0000000,0x40000000,0x200000000,0x500000000,0xa00000000,0x1400000000,0x2800000000,0x5000000000,0xa000000000,0x4000000000,0x20000000000,0x50000000000,0xa0000000000,0x140000000000,0x280000000000,0x500000000000,0xa00000000000,0x400000000000,0x2000000000000,0x5000000000000,0xa000000000000,0x14000000000000,0x28000000000000,0x50000000000000,0xa0000000000000,0x40000000000000,
  // knight
  0x20400,0x50800,0xa1100,0x142200,0x284400,0x508800,0xa01000,0x402000,0x2040004,0x5080008,0xa110011,0x14220022,0x28440044,0x50880088,0xa0100010,0x40200020,0x204000402,0x508000805,0xa1100110a,0x1422002214,0x2844004428,0x5088008850,0xa0100010a0,0x4020002040,0x20400040200,0x50800080500,0xa1100110a00,0x142200221400,0x284400442800,0x508800885000,0xa0100010a000,0x402000204000,0x2040004020000,0x5080008050000,0xa1100110a0000,0x14220022140000,0x28440044280000,0x50880088500000,0xa0100010a00000,0x40200020400000,0x204000402000000,0x508000805000000,0xa1100110a000000,0x1422002214000000,0x2844004428000000,0x5088008850000000,0xa0100010a0000000,0x4020002040000000,0x400040200000000,0x800080500000000,0x1100110a00000000,0x2200221400000000,0x4400442800000000,0x8800885000000000,0x100010a000000000,0x2000204000000000,0x4020000000000,0x8050000000000,0x110a0000000000,0x22140000000000,0x44280000000000,0x88500000000000,0x10a00000000000,0x20400000000000,
  // king
  0x302,0x705,0xe0a,0x1c14,0x3828,0x7050,0xe0a0,0xc040,0x30203,0x70507,0xe0a0e,0x1c141c,0x382838,0x705070,0xe0a0e0,0xc040c0,0x3020300,0x7050700,0xe0a0e00,0x1c141c00,0x38283800,0x70507000,0xe0a0e000,0xc040c000,0x302030000,0x705070000,0xe0a0e0000,0x1c141c0000,0x3828380000,0x7050700000,0xe0a0e00000,0xc040c00000,0x30203000000,0x70507000000,0xe0a0e000000,0x1c141c000000,0x382838000000,0x705070000000,0xe0a0e0000000,0xc040c0000000,0x3020300000000,0x7050700000000,0xe0a0e00000000,0x1c141c00000000,0x38283800000000,0x70507000000000,0xe0a0e000000000,0xc040c000000000,0x302030000000000,0x705070000000000,0xe0a0e0000000000,0x1c141c0000000000,0x3828380000000000,0x7050700000000000,0xe0a0e00000000000,0xc040c00000000000,0x203000000000000,0x507000000000000,0xa0e000000000000,0x141c000000000000,0x2838000000000000,0x5070000000000000,0xa0e0000000000000,0x40c0000000000000,
  // bishop
  0x8040201008040200,0x80402010080500,0x804020110a00,0x8041221400,0x182442800,0x10204885000,0x102040810a000,0x102040810204000,0x4020100804020002,0x8040201008050005,0x804020110a000a,0x804122140014,0x18244280028,0x1020488500050,0x102040810a000a0,0x204081020400040,0x2010080402000204,0x4020100805000508,0x804020110a000a11,0x80412214001422,0x1824428002844,0x102048850005088,0x2040810a000a010,0x408102040004020,0x1008040200020408,0x2010080500050810,0x4020110a000a1120,0x8041221400142241,0x182442800284482,0x204885000508804,0x40810a000a01008,0x810204000402010,0x804020002040810,0x1008050005081020,0x20110a000a112040,0x4122140014224180,0x8244280028448201,0x488500050880402,0x810a000a0100804,0x1020400040201008,0x402000204081020,0x805000508102040,0x110a000a11204080,0x2214001422418000,0x4428002844820100,0x8850005088040201,0x10a000a010080402,0x2040004020100804,0x200020408102040,0x500050810204080,0xa000a1120408000,0x1400142241800000,0x2800284482010000,0x5000508804020100,0xa000a01008040201,0x4000402010080402,0x2040810204080,0x5081020408000,0xa112040800000,0x14224180000000,0x28448201000000,0x50880402010000,0xa0100804020100,0x40201008040201,
  // rook
  0x1010101010101fe,0x2020202020202fd,0x4040404040404fb,0x8080808080808f7,0x10101010101010ef,0x20202020202020df,0x40404040404040bf,0x808080808080807f,0x10101010101fe01,0x20202020202fd02,0x40404040404fb04,0x80808080808f708,0x101010101010ef10,0x202020202020df20,0x404040404040bf40,0x8080808080807f80,0x101010101fe0101,0x202020202fd0202,0x404040404fb0404,0x808080808f70808,0x1010101010ef1010,0x2020202020df2020,0x4040404040bf4040,0x80808080807f8080,0x1010101fe010101,0x2020202fd020202,0x4040404fb040404,0x8080808f7080808,0x10101010ef101010,0x20202020df202020,0x40404040bf404040,0x808080807f808080,0x10101fe01010101,0x20202fd02020202,0x40404fb04040404,0x80808f708080808,0x101010ef10101010,0x202020df20202020,0x404040bf40404040,0x8080807f80808080,0x101fe0101010101,0x202fd0202020202,0x404fb0404040404,0x808f70808080808,0x1010ef1010101010,0x2020df2020202020,0x4040bf4040404040,0x80807f8080808080,0x1fe010101010101,0x2fd020202020202,0x4fb040404040404,0x8f7080808080808,0x10ef101010101010,0x20df202020202020,0x40bf404040404040,0x807f808080808080,0xfe01010101010101,0xfd02020202020202,0xfb04040404040404,0xf708080808080808,0xef10101010101010,0xdf20202020202020,0xbf40404040404040,0x7f80808080808080,
  // queen
  0x81412111090503fe,0x2824222120a07fd,0x404844424150efb,0x8080888492a1cf7,0x10101011925438ef,0x2020212224a870df,0x404142444850e0bf,0x8182848890a0c07f,0x412111090503fe03,0x824222120a07fd07,0x4844424150efb0e,0x80888492a1cf71c,0x101011925438ef38,0x20212224a870df70,0x4142444850e0bfe0,0x82848890a0c07fc0,0x2111090503fe0305,0x4222120a07fd070a,0x844424150efb0e15,0x888492a1cf71c2a,0x1011925438ef3854,0x212224a870df70a8,0x42444850e0bfe050,0x848890a0c07fc0a0,0x11090503fe030509,0x22120a07fd070a12,0x4424150efb0e1524,0x88492a1cf71c2a49,0x11925438ef385492,0x2224a870df70a824,0x444850e0bfe05048,0x8890a0c07fc0a090,0x90503fe03050911,0x120a07fd070a1222,0x24150efb0e152444,0x492a1cf71c2a4988,0x925438ef38549211,0x24a870df70a82422,0x4850e0bfe0504844,0x90a0c07fc0a09088,0x503fe0305091121,0xa07fd070a122242,0x150efb0e15244484,0x2a1cf71c2a498808,0x5438ef3854921110,0xa870df70a8242221,0x50e0bfe050484442,0xa0c07fc0a0908884,0x3fe030509112141,0x7fd070a12224282,0xefb0e1524448404,0x1cf71c2a49880808,0x38ef385492111010,0x70df70a824222120,0xe0bfe05048444241,0xc07fc0a090888482,0xfe03050911214181,0xfd070a1222428202,0xfb0e152444840404,0xf71c2a4988080808,0xef38549211101010,0xdf70a82422212020,0xbfe0504844424140,0x7fc0a09088848281,
};

Square getkingsq(__private Bitboard *board, bool side)
{
  Bitboard bbTemp = (side)?
                      board[QBBBLACK]
                     :board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]);

  bbTemp &= board[QBBP1]&board[QBBP2]&~board[QBBP3]; // get king

  return first1(bbTemp);
}
// is square attacked by an enemy piece, via superpiece approach
bool squareunderattack(__private Bitboard *board, bool stm, Square sq) 
{
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbMe;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbTemp;

  bbBlockers = board[QBBP1]|board[QBBP2]|board[QBBP3];
  bbMe       = (stm)?board[QBBBLACK]:(board[QBBBLACK]^bbBlockers);

  // rooks and queens via dumb7fill
  bbMoves = BBEMPTY;

  bbPro  = ~bbBlockers;
  bbPro &= BBNOTAFILE;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen << 1) & bbPro;
  bbTemp |= bbGen = (bbGen << 1) & bbPro;
  bbTemp |= bbGen = (bbGen << 1) & bbPro;
  bbTemp |= bbGen = (bbGen << 1) & bbPro;
  bbTemp |= bbGen = (bbGen << 1) & bbPro;
  bbTemp |=         (bbGen << 1) & bbPro;
  bbMoves |=         (bbTemp<< 1) & BBNOTAFILE;

  bbPro  = ~bbBlockers;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen << 8) & bbPro;
  bbTemp |= bbGen = (bbGen << 8) & bbPro;
  bbTemp |= bbGen = (bbGen << 8) & bbPro;
  bbTemp |= bbGen = (bbGen << 8) & bbPro;
  bbTemp |= bbGen = (bbGen << 8) & bbPro;
  bbTemp |=         (bbGen << 8) & bbPro;
  bbMoves |=         (bbTemp<< 8);

  bbPro  = ~bbBlockers;
  bbPro &= BBNOTHFILE;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen >> 1) & bbPro;
  bbTemp |= bbGen = (bbGen >> 1) & bbPro;
  bbTemp |= bbGen = (bbGen >> 1) & bbPro;
  bbTemp |= bbGen = (bbGen >> 1) & bbPro;
  bbTemp |= bbGen = (bbGen >> 1) & bbPro;
  bbTemp |=         (bbGen >> 1) & bbPro;
  bbMoves |=         (bbTemp>> 1) & BBNOTHFILE;

  bbPro  = ~bbBlockers;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen >> 8) & bbPro;
  bbTemp |= bbGen = (bbGen >> 8) & bbPro;
  bbTemp |= bbGen = (bbGen >> 8) & bbPro;
  bbTemp |= bbGen = (bbGen >> 8) & bbPro;
  bbTemp |= bbGen = (bbGen >> 8) & bbPro;
  bbTemp |=         (bbGen >> 8) & bbPro;
  bbMoves |=         (bbTemp>> 8);

  bbWork =    (bbMe&(board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
            | (bbMe&(~board[QBBP1]&board[QBBP2]&board[QBBP3]));
  if (bbMoves&bbWork)
  {
    return true;
  }

  // bishops and queens via dumb7fill
  bbMoves = BBEMPTY;
  bbPro  = ~bbBlockers;
  bbPro &= BBNOTAFILE;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen << 9) & bbPro;
  bbTemp |= bbGen = (bbGen << 9) & bbPro;
  bbTemp |= bbGen = (bbGen << 9) & bbPro;
  bbTemp |= bbGen = (bbGen << 9) & bbPro;
  bbTemp |= bbGen = (bbGen << 9) & bbPro;
  bbTemp |=         (bbGen << 9) & bbPro;
  bbMoves |=         (bbTemp<< 9) & BBNOTAFILE;

  bbPro  = ~bbBlockers;
  bbPro &= BBNOTHFILE;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen << 7) & bbPro;
  bbTemp |= bbGen = (bbGen << 7) & bbPro;
  bbTemp |= bbGen = (bbGen << 7) & bbPro;
  bbTemp |= bbGen = (bbGen << 7) & bbPro;
  bbTemp |= bbGen = (bbGen << 7) & bbPro;
  bbTemp |=         (bbGen << 7) & bbPro;
  bbMoves |=         (bbTemp<< 7) & BBNOTHFILE;

  bbPro  = ~bbBlockers;
  bbPro &= BBNOTHFILE;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen >> 9) & bbPro;
  bbTemp |= bbGen = (bbGen >> 9) & bbPro;
  bbTemp |= bbGen = (bbGen >> 9) & bbPro;
  bbTemp |= bbGen = (bbGen >> 9) & bbPro;
  bbTemp |= bbGen = (bbGen >> 9) & bbPro;
  bbTemp |=         (bbGen >> 9) & bbPro;
  bbMoves |=         (bbTemp>> 9) & BBNOTHFILE;

  bbPro  = ~bbBlockers;
  bbPro &= BBNOTAFILE;
  bbTemp = bbGen = SETMASKBB(sq);

  bbTemp |= bbGen = (bbGen >> 7) & bbPro;
  bbTemp |= bbGen = (bbGen >> 7) & bbPro;
  bbTemp |= bbGen = (bbGen >> 7) & bbPro;
  bbTemp |= bbGen = (bbGen >> 7) & bbPro;
  bbTemp |= bbGen = (bbGen >> 7) & bbPro;
  bbTemp |=         (bbGen >> 7) & bbPro;
  bbMoves |=         (bbTemp>> 7) & BBNOTAFILE;

  bbWork =  (bbMe&(~board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
          | (bbMe&(~board[QBBP1]&board[QBBP2]&board[QBBP3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  // knights
  bbWork = bbMe&(~board[QBBP1]&board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTables[128+sq] ;
  if (bbMoves&bbWork) 
  {
    return true;
  }
  // pawns
  bbWork = bbMe&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTables[!stm*64+sq];
  if (bbMoves&bbWork)
  {
    return true;
  }
  // king
  bbWork = bbMe&(board[QBBP1]&board[QBBP2]&~board[QBBP3]);
  bbMoves = AttackTables[192+sq];
  if (bbMoves&bbWork)
  {
    return true;
  } 

  return false;
}
// alphabeta search on gpu
// 64 threads in parallel on one chess position
// move gen with pawn queen promo only
__kernel void alphabeta_gpu(
                              const __global Bitboard *BOARD,
                                    __global u64 *COUNTERS,
                              const __global u32 *RNUMBERS,
                                    __global Move *PV,
                                    __global Bitboard *globalbbMoves1,
                                    __global Bitboard *globalbbMoves2,
                                    __global Hash *HashHistory,
                              const __global Bitboard *bbInBetween,
                              const __global Bitboard *bbLine,
                                    __global TTE *TT1,
                                    __global ABDADATTE *TT2,
                                    __global Move *Killers,
                                    __global Move *Counters,
                                       const s32 stm_init,
                                       const s32 ply_init,
                                       const s32 search_depth,
                                       const u64 max_nodes,
                                       const u64 ttindex1,
                                       const u64 ttindex2,
                                    __global u32 *finito
)
{
  // Quadbitboard
  __private Bitboard board[4]; 

  // temporary place holders
#if !defined cl_khr_int64_extended_atomics || defined OLDSCHOOL
  __local Bitboard bbTmp64[64];
#endif
#if !defined cl_khr_local_int32_base_atomics || defined OLDSCHOOL
  __local s32 scrTmp64[64];
#endif

  __local TTE tt1;
  __local ABDADATTE tt2;

  // iterative var stack
  __local u8 localNodeStates[MAXPLY];
  __local u8 localSearchMode[MAXPLY];
  __local s32 localDepth[MAXPLY];
  __local Score localAlphaBetaScores[MAXPLY*2];
  __local s32 localTodoIndex[MAXPLY];
  __local s32 localMoveCounter[MAXPLY];
  __local Move localMoveHistory[MAXPLY];
  __local Move localIIDMoves[MAXPLY];
  __local Cr localCrHistory[MAXPLY];
  __local u8 localHMCHistory[MAXPLY];
  __local Hash localHashHistory[MAXPLY];

  __local bool bexit;       // exit the main loop flag
  __local bool brandomize;  // randomize move order flag
  __local bool bresearch;   // late move reduction reseach flag
  __local bool bforward;    // late move reduction reseach flag

  __local u8 ttage;

  __local Square sqchecker;

  __local Score evalscore;
  __local Score movescore;
  __local s32 movecount;

  __local Score bestscore;

  __local Move lmove;
  __local Move bestmove;

  __local Bitboard bbAttacks;
  __local Bitboard bbCheckers;

  const s32 gid = (u32)(get_global_id(0)*get_global_size(1)+get_global_id(1));
  const Square lid = (Square)get_local_id(2);

  bool tmpb;
  bool color;
  bool kic;
  bool rootkic;
  bool qs;
  bool stm;

  Square sqking;
  Square sqto;
  Square sqcpt;   
  Square sqep;

  Piece pfrom;
  Piece pto;
  Piece pcpt;

  s32 sd;
  s32 ply;

  s32 n;

  Score score;
  Score tmpscore;

  // pseudo random numbers, seeded on host
  u32 prn = RNUMBERS[gid*64+(s32)lid];
/*
  // xorshift32 PRNG
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
*/
  Move move;
  Move tmpmove;

  Bitboard bbBlockers;
  Bitboard bbMe;
  Bitboard bbOpp;
  Bitboard bbTemp;
  Bitboard bbWork;
  Bitboard bbMask;
  Bitboard bbMoves;

  Bitboard bbPinned;

  Bitboard bbPro;
  Bitboard bbGen; 

  // get init quadbitboard plus plus
  board[QBBBLACK] = BOARD[QBBBLACK];
  board[QBBP1]    = BOARD[QBBP1];
  board[QBBP2]    = BOARD[QBBP2];
  board[QBBP3]    = BOARD[QBBP3];
  stm             = (bool)stm_init;
  ply             = 0;
  sd              = 1;

  // inits
  bexit           = false;
  bestmove        = MOVENONE;
  bestscore       = DRAWSCORE;
  brandomize      = false;
  bresearch       = false;
  bforward        = false;
  ttage           = (u8)ply_init&0x3F;

  // init ab search var stack
  localAlphaBetaScores[0*2+ALPHA] =-INF;
  localAlphaBetaScores[0*2+BETA]  = INF;
  localMoveCounter[0]             = 0;
  localTodoIndex[0]               = 0;
  localMoveHistory[0]             = MOVENONE;
  localIIDMoves[0]                = MOVENONE;
  localCrHistory[0]               = BOARD[QBBPMVD];
  localHMCHistory[0]              = (u8)BOARD[QBBHMC];
  localHashHistory[0]             = BOARD[QBBHASH];
  localDepth[0]                   = search_depth+1;
  localNodeStates[0]              = STATENONE | ITER1;
  localSearchMode[0]              = SEARCH;
  localAlphaBetaScores[sd*2+ALPHA]=-INF;
  localAlphaBetaScores[sd*2+BETA] = INF;
  localMoveCounter[sd]            = 0;
  localTodoIndex[sd]              = 0;
  localMoveHistory[sd]            = MOVENONE;
  localIIDMoves[sd]               = MOVENONE;
  localCrHistory[sd]              = BOARD[QBBPMVD];
  localHMCHistory[sd]             = (u8)BOARD[QBBHMC];
  localHashHistory[sd]            = BOARD[QBBHASH];
  localDepth[sd]                  = search_depth;
  localNodeStates[sd]             = STATENONE | ITER1;
  localSearchMode[sd]             = SEARCH;

  barrier(CLK_LOCAL_MEM_FENCE);
  barrier(CLK_GLOBAL_MEM_FENCE);
  // ################################
  // ####       main loop        ####
  // ################################
  while(!bexit)
  {
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    // reset vars
    brandomize  = false;
    bresearch   = false;
    bforward    = false;
    movecount   = 0;
    lmove       = MOVENONE;
    evalscore   = DRAWSCORE;
    movescore   = -INFMOVESCORE;
    sqchecker   = 0x0;
    bbAttacks   = BBEMPTY;
    bbCheckers  = BBEMPTY;
    bbPinned    = BBEMPTY;

    // ################################
    // ####     movegenerator x64  ####
    // ################################
    // inits
    n = 0;
    bbBlockers  = board[QBBP1]|board[QBBP2]|board[QBBP3];
    bbMe        =  (stm)?board[QBBBLACK]:(board[QBBBLACK]^bbBlockers);
    bbOpp       = (!stm)?board[QBBBLACK]:(board[QBBBLACK]^bbBlockers);

    // get colored king
    bbTemp = bbMe&board[QBBP1]&board[QBBP2]&~board[QBBP3];
    // get king square
    sqking = first1(bbTemp);

    // calc superking and get pinned pieces
    // get superking, rooks n queens via dumb7fill
    bbWork = BBEMPTY;

    bbPro  = BBFULL;
    bbPro &= BBNOTAFILE;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |=         (bbGen << 1) & bbPro;
    bbWork |=         (bbTemp<< 1) & BBNOTAFILE;

    bbPro  = BBFULL;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |=         (bbGen << 8) & bbPro;
    bbWork |=         (bbTemp<< 8);

    bbPro  = BBFULL;
    bbPro &= BBNOTHFILE;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |=         (bbGen >> 1) & bbPro;
    bbWork |=         (bbTemp>> 1) & BBNOTHFILE;

    bbPro  = BBFULL;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |=         (bbGen >> 8) & bbPro;
    bbWork |=         (bbTemp>> 8);

    bbWork &= ((bbOpp&(board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
                | (bbOpp&(~board[QBBP1]&board[QBBP2]&board[QBBP3])));

    // get pinned pieces
    while (bbWork)
    {
      sqto = popfirst1(&bbWork);
      bbTemp = bbInBetween[sqto*64+sqking]&bbBlockers;
      if (count1s(bbTemp)==1)
        bbPinned |= bbTemp;
    }

    // get superking, bishops n queems via dumb7fill
    bbWork = BBEMPTY;

    bbPro  = BBFULL;
    bbPro &= BBNOTAFILE;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |=         (bbGen << 9) & bbPro;
    bbWork |=         (bbTemp<< 9) & BBNOTAFILE;

    bbPro  = BBFULL;
    bbPro &= BBNOTHFILE;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |=         (bbGen << 7) & bbPro;
    bbWork |=         (bbTemp<< 7) & BBNOTHFILE;

    bbPro  = BBFULL;
    bbPro &= BBNOTHFILE;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |=         (bbGen >> 9) & bbPro;
    bbWork |=         (bbTemp>> 9) & BBNOTHFILE;

    bbPro  = BBFULL;
    bbPro &= BBNOTAFILE;
    bbTemp = bbGen = SETMASKBB(sqking);

    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |=         (bbGen >> 7) & bbPro;
    bbWork |=         (bbTemp>> 7) & BBNOTAFILE;

    bbWork &= ((bbOpp&(~board[QBBP1]&~board[QBBP2]&board[QBBP3])) 
                | (bbOpp&(~board[QBBP1]&board[QBBP2]&board[QBBP3])));

    // get pinned pieces
    while (bbWork)
    {
      sqto = popfirst1(&bbWork);
      bbTemp = bbInBetween[sqto*64+sqking]&bbBlockers;
      if (count1s(bbTemp)==1)
        bbPinned |= bbTemp;
    }

    // generate own moves and opposite attacks
    pfrom  = GETPIECE(board, lid);
    color  = GETCOLOR(pfrom);
    bbWork = BBEMPTY;
    // dumb7fill for 8 directions
    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbPro &= BBNOTAFILE;
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |= bbGen = (bbGen << 9) & bbPro;
    bbTemp |=         (bbGen << 9) & bbPro;
    bbWork |=         (bbTemp<< 9) & BBNOTAFILE;

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbPro &= BBNOTAFILE;
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |= bbGen = (bbGen << 1) & bbPro;
    bbTemp |=         (bbGen << 1) & bbPro;
    bbWork |=         (bbTemp<< 1) & BBNOTAFILE;

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbPro &= BBNOTHFILE;
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |= bbGen = (bbGen << 7) & bbPro;
    bbTemp |=         (bbGen << 7) & bbPro;
    bbWork |=         (bbTemp<< 7) & BBNOTHFILE;

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |= bbGen = (bbGen << 8) & bbPro;
    bbTemp |=         (bbGen << 8) & bbPro;
    bbWork |=         (bbTemp<< 8);

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbPro &= BBNOTHFILE;
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |= bbGen = (bbGen >> 9) & bbPro;
    bbTemp |=         (bbGen >> 9) & bbPro;
    bbWork |=         (bbTemp>> 9) & BBNOTHFILE;

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbPro &= BBNOTHFILE;
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |= bbGen = (bbGen >> 1) & bbPro;
    bbTemp |=         (bbGen >> 1) & bbPro;
    bbWork |=         (bbTemp>> 1) & BBNOTHFILE;

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbPro &= BBNOTAFILE;
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |= bbGen = (bbGen >> 7) & bbPro;
    bbTemp |=         (bbGen >> 7) & bbPro;
    bbWork |=         (bbTemp>> 7) & BBNOTAFILE;

    bbPro  = (color==stm)?~bbBlockers:~(bbBlockers^SETMASKBB(sqking));
    bbTemp = bbGen = bbBlockers&SETMASKBB(lid);

    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |= bbGen = (bbGen >> 8) & bbPro;
    bbTemp |=         (bbGen >> 8) & bbPro;
    bbWork |=         (bbTemp>> 8);

    // consider knights
    bbWork  = (GETPTYPE(pfrom)==KNIGHT)?BBFULL:bbWork;
    // verify captures
    n       = (color==stm)?(s32)stm:(s32)!stm;
    n       = (GETPTYPE(pfrom)==PAWN)?n:GETPTYPE(pfrom);
    bbMask  = AttackTables[n*64+(s32)lid];
    bbMoves = (color==stm)?(bbMask&bbWork&bbOpp):(bbMask&bbWork);

    barrier(CLK_LOCAL_MEM_FENCE);
#if defined cl_khr_int64_extended_atomics && !defined OLDSCHOOL
    // collect opp attacks x64
    atom_or(&bbAttacks, (color!=stm)?bbMoves:BBEMPTY);
    // collect king checkers x64
    atom_or(&bbCheckers, (bbMoves&SETMASKBB(sqking))?SETMASKBB(lid):BBEMPTY);
    // get king checkers x64
    if (bbMoves&SETMASKBB(sqking))
      sqchecker = lid;
#else
    bbTmp64[lid] = (color!=stm)?bbMoves:BBEMPTY;
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect opp attacks x1
    if (lid==0)
      for (int i=0;i<64;i++)
        bbAttacks |= bbTmp64[i];
    barrier(CLK_LOCAL_MEM_FENCE);
    // get king checkers
    bbTmp64[lid] = BBEMPTY;
    if (bbMoves&SETMASKBB(sqking))
    {
      sqchecker = lid;
      bbTmp64[lid] = SETMASKBB(lid);
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect king checkers x1
    if (lid==0)
      for (int i=0;i<64;i++)
        bbCheckers |= bbTmp64[i];
#endif
    barrier(CLK_LOCAL_MEM_FENCE);

    // in check
    rootkic = (bbCheckers)?true:false;

    // LMR, no check giving moves
    if (lid==0&&rootkic&&(localNodeStates[sd-1]&LMR))
    {
      localDepth[sd]        += LMRR;
      localNodeStates[sd-1] ^= LMR;
      localSearchMode[sd]    = localSearchMode[sd-1];
    }

    // depth extension
    if (lid==0
        &&localDepth[sd]>=0
        &&!(localNodeStates[sd-1]&QS)
        &&
        (
          rootkic
          ||
          ((GETPTYPE(GETPFROM(localMoveHistory[sd-1]))==PAWN)
            &&(GETPTYPE(GETPTO(localMoveHistory[sd-1]))==QUEEN)
           )
        )
       )
    {
      localDepth[sd]++;
      localNodeStates[sd] |= EXT;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    // enter quiescence search?
    qs = (localDepth[sd]<=0)?true:false;

    // verify non captures
    bbMask  = (GETPTYPE(pfrom)==PAWN)?
                (AttackTablesPawnPushes[stm*64+lid])
               :bbMask;
    bbMoves|= (color==stm&&!qs)?(bbMask&bbWork&~bbBlockers):BBEMPTY; 

    // extract only own moves
    bbMoves = (color==stm)?bbMoves:BBEMPTY;

    n = count1s(bbCheckers);

    // double check, king moves only
    tmpb = (n>=2&&GETPTYPE(pfrom)!=KING)?true:false;
    bbMoves = (tmpb)?BBEMPTY:bbMoves;

    // consider pinned pieces
    tmpb = (SETMASKBB(lid)&bbPinned)?true:false;
    bbMoves &= (tmpb)?bbLine[lid*64+sqking]:BBFULL;

    // consider king and opp attacks
    tmpb = (GETPTYPE(pfrom)==KING)?true:false;
    bbMoves &= (tmpb)?~bbAttacks:BBFULL;

    // consider single checker
    tmpb = (n==1&&GETPTYPE(pfrom)!=KING)?true:false;
    bbMoves &= (tmpb)?(bbInBetween[sqchecker*64+sqking]|bbCheckers):BBFULL;

    // gen en passant moves, TODO: reimplement as x64?
    // check for double pawn push
    bbTemp  = ~localCrHistory[sd];
    bbTemp &= 0x000000FFFF000000;
    sqep    = (bbTemp)?first1(bbTemp):0x0;
    // check pawns
    bbMask  = bbMe&(board[QBBP1]&~board[QBBP2]&~board[QBBP3]); // get our pawns
    bbMask &= (stm)?0xFF000000UL:0xFF00000000UL;
    bbTemp  = bbMask&(SETMASKBB(sqep+1)|SETMASKBB(sqep-1));
    // check for en passant pawns
    if (bbTemp&SETMASKBB(lid))
    {
      pto     = pfrom;
      sqcpt   = sqep;
      pcpt    = GETPIECE(board, sqcpt);
      sqto    = (stm)? sqep-8:sqep+8;
      // pack move into 32 bits
      move    = MAKEMOVE((Move)lid, (Move)sqto, (Move)sqcpt, (Move)pfrom, (Move)pto, (Move)pcpt);
      // legal moves only
      domovequick(board, move);
      kic = squareunderattack(board, !stm, getkingsq(board, stm));
      undomovequick(board, move);
      if (!kic)
      {
        bbMoves |= SETMASKBB(sqto);
      }
    }
    // gen caslte moves, TODO: speedup, less registers
    bbTemp = localCrHistory[sd]; // get castle rights via piece moved flags
    // gen castle moves queenside
    tmpb = (lid==sqking
            &&!qs
            &&(bbTemp&SMCRALL)
            &&((stm&&(((~bbTemp)&SMCRBLACKQ)==SMCRBLACKQ))
                ||(!stm&&(((~bbTemp)&SMCRWHITEQ)==SMCRWHITEQ))
              )
            )?true:false;
    // rook present
    bbTemp  = (GETPIECE(board, lid-4)==MAKEPIECE(ROOK,GETCOLOR(pfrom)))?
                true
               :false;
    // check for empty squares
    bbMask  = ((bbBlockers&SETMASKBB(lid-1))
                |(bbBlockers&SETMASKBB(lid-2))
                |(bbBlockers&SETMASKBB(lid-3))
              );
    // check for king and empty squares in check
    bbWork =  (bbAttacks&SETMASKBB(lid))
               |(bbAttacks&SETMASKBB(lid-1))
               |(bbAttacks&SETMASKBB(lid-2)
              );
    // store move
    bbMoves |= (tmpb&&bbTemp&&!bbMask&&!bbWork)?SETMASKBB(lid-2):BBEMPTY;

    bbTemp = localCrHistory[sd]; // get castle rights via piece moved flags
    // gen castle moves kingside
    tmpb =  (lid==sqking
             &&!qs
             &&(bbTemp&SMCRALL)
             &&((stm&&(((~bbTemp)&SMCRBLACKK)==SMCRBLACKK))
                ||(!stm&&(((~bbTemp)&SMCRWHITEK)==SMCRWHITEK))
               )
             )?true:false;
    // rook present
    bbTemp  = (GETPIECE(board, lid+3)==MAKEPIECE(ROOK,GETCOLOR(pfrom)))?
                true
               :false;
    // check for empty squares
    bbMask  = ((bbBlockers&SETMASKBB(lid+1))|(bbBlockers&SETMASKBB(lid+2)));
    // check for king and empty squares in check
    bbWork =  (bbAttacks&SETMASKBB(lid))
                |(bbAttacks&SETMASKBB(lid+1))
                |(bbAttacks&SETMASKBB(lid+2)
              );
    // store move
    bbMoves |= (tmpb&&bbTemp&&!bbMask&&!bbWork)?SETMASKBB(lid+2):BBEMPTY;

    // store move bitboards in global memory for movepicker
    globalbbMoves1[gid*MAXPLY*64+sd*64+(s32)lid] = bbMoves;
    globalbbMoves2[gid*MAXPLY*64+sd*64+(s32)lid] = BBEMPTY;

    // collect movecount
#if defined cl_khr_local_int32_base_atomics && !defined OLDSCHOOL
    // collect movecount x64
    atom_add(&movecount, count1s(bbMoves));
#else
    // store movecount in local temp
    scrTmp64[lid] = count1s(bbMoves);
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect movecount
    if (lid==0)
      for (int i=0;i<64;i++)
        movecount += scrTmp64[i];
#endif
    // store node state in local memory
    if (lid==0)
    {
      localNodeStates[sd]  |= (qs)?QS:STATENONE;
      localNodeStates[sd]  |= (rootkic)?KIC:STATENONE;
    }
    // ################################
    // ####     evaluation x64      ###
    // ################################
    pfrom   = GETPIECE(board, lid);
    color   = GETCOLOR(pfrom);
    pfrom   = GETPTYPE(pfrom);
    bbBlockers = board[QBBP1]|board[QBBP2]|board[QBBP3];
    bbMask  = board[QBBP1]&~board[QBBP2]&~board[QBBP3]; // get all pawns
    bbMe    =  (color)?board[QBBBLACK]:(board[QBBBLACK]^bbBlockers);
    bbOpp   =  (color)?(board[QBBBLACK]^bbBlockers):board[QBBBLACK];
    score   = 0;
    Square sqfrom = (color)?lid:FLOP(lid);
    // piece bonus
    score+= (pfrom!=PNONE)?10:0;
    // wood count
    score+= (pfrom!=PNONE)?EvalPieceValues[pfrom]:0;
    // piece square tables
    score+= (pfrom!=PNONE)?EvalTable[pfrom*64+sqfrom]:0;
    // square control table
    score+= (pfrom!=PNONE)?EvalControl[sqfrom]:0;
    // simple pawn structure white
    tmpb = (pfrom==PAWN&&color==WHITE)?true:false;
    // blocked
    score-=(tmpb&&GETRANK(lid)<RANK_8&&(bbOpp&SETMASKBB(lid+8)))?15:0;
      // chain
    score+=(tmpb&&GETFILE(lid)<FILE_H&&(bbMask&bbMe&SETMASKBB(lid-7)))?10:0;
    score+=(tmpb&&GETFILE(lid)>FILE_A&&(bbMask&bbMe&SETMASKBB(lid-9)))?10:0;
    // column, TODO: popcount based
    for(sqto=lid-8;sqto>7&&tmpb;sqto-=8)
      score-=(bbMask&bbMe&SETMASKBB(sqto))?30:0;

    // simple pawn structure black
    tmpb = (pfrom==PAWN&&color==BLACK)?true:false;
    // blocked
    score-=(tmpb&&GETRANK(lid)>RANK_1&&(bbOpp&SETMASKBB(lid-8)))?15:0;
      // chain
    score+=(tmpb&&GETFILE(lid)>FILE_A&&(bbMask&bbMe&SETMASKBB(lid+7)))?10:0;
    score+=(tmpb&&GETFILE(lid)<FILE_H&&(bbMask&bbMe&SETMASKBB(lid+9)))?10:0;
    // column, TODO: popcount based
    for(sqto=lid+8;sqto<56&&tmpb;sqto+=8)
      score-=(bbMask&bbMe&SETMASKBB(sqto))?30:0;
    // negamaxed scores
    score = (color)?-score:score;
    // duble bishop
    if (lid==0)
    {
      score-= (count1s(board[QBBBLACK]
                       &(~board[QBBP1]&~board[QBBP2]&board[QBBP3]))==2)?
              25:0;
      score+= (count1s((board[QBBBLACK]^bbBlockers)
                        &(~board[QBBP1]&~board[QBBP2]&board[QBBP3]))==2)?
              25:0;
    }

#if defined cl_khr_local_int32_base_atomics && !defined OLDSCHOOL
    // collect score x64
    atom_add(&evalscore, score);
#else
    // store scores in local temp
    scrTmp64[lid] = score;
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect score x1
    if (lid==0)
      for (int i=0;i<64;i++)
        evalscore+= scrTmp64[i];
#endif

    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    // #################################
    // ####   negmax and scoring x1  ###
    // #################################
    if (lid==0)
    {
      // negamaxed scores
      score = (stm)?-evalscore:evalscore;
      // checkmate
      score = (!qs&&rootkic&&movecount==0)?-INF+ply:score;
      // stalemate
      score = (!qs&&!rootkic&&movecount==0)?STALEMATESCORE:score;

      // draw by 3 fold repetition, x1
      bbWork = localHashHistory[sd];
      for (n=ply+ply_init-2;
                            lid==0
                            &&movecount>0
                            &&!qs
                            &&sd>1
                            &&n>=0
                            &&n>=ply+ply_init-(s32)localHMCHistory[sd];
                                                                       n-=2)
      {
        if (bbWork==HashHistory[gid*MAXGAMEPLY+n])
        {
          movecount = 0;
          score = DRAWSCORE;
          break;
        }
      }
      // fifty move rule
      if (sd>1&&localHMCHistory[sd]>=100)
      {
        movecount = 0;
        score = DRAWSCORE;
      }
      // Kxk draw
      if (sd>1&&count1s(board[QBBP1]|board[QBBP2]|board[QBBP3])<=2)
      {
        movecount = 0;
        score = DRAWSCORE;
      }
      // #################################
      // ####       tree flow x1       ###
      // #################################
      // check bounds
      if (sd>=MAXPLY)
        movecount = 0;
      // terminal or leaf node
      if (movecount==0)
        localAlphaBetaScores[sd*2+ALPHA]=score;
      // stand pat in qsearch
      // return beta
      if (movecount>0&&qs&&!rootkic&&score>=localAlphaBetaScores[sd*2+BETA])
      {
        localAlphaBetaScores[sd*2+ALPHA] = score; // fail soft
        movecount = 0;
      }
      // stand pat in qsearch
      // set alpha
      if (movecount>0&&qs&&!rootkic&&score>localAlphaBetaScores[sd*2+ALPHA])
        localAlphaBetaScores[sd*2+ALPHA] = score; // fail soft

      // load from hash table, update alpha with ttscore
      if (movecount>0
          &&!qs
          &&sd>1 // not on root
          &&!(localSearchMode[sd]&NULLMOVESEARCH)
          &&(ttindex1>1)
       )
      {
        bbWork = localHashHistory[sd];    
        bbTemp = bbWork&(ttindex1-1);
        score  = -INF;

        tt1 = TT1[bbTemp];
        if ((tt1.hash==(bbWork^(Hash)tt1.bestmove^(Hash)tt1.score^(Hash)tt1.depth))
            &&(s32)tt1.depth>=localDepth[sd]
            &&(tt1.flag&0x3)>FAILLOW
           )
        {
          score = (Score)tt1.score;

          // handle mate scores in TT
          score = (ISMATE(score)&&score>0)?score-ply:score;
          score = (ISMATE(score)&&score<0)?score+ply:score;
        }

        if (!ISINF(score)
            &&score>localAlphaBetaScores[sd*2+ALPHA]
           )
        {
          // set alpha
          localAlphaBetaScores[sd*2+ALPHA] = score;

          // cut
          if(localAlphaBetaScores[sd*2+ALPHA]>=localAlphaBetaScores[sd*2+BETA])
            movecount = 0;

          // tt score hit counter
          COUNTERS[gid*64+4]++;

        }
      } // end load from hash table

      // abdada, check hash table
      if (movecount>1
          &&!qs
          &&sd>1 // not on root
          &&!(localSearchMode[sd]&NULLMOVESEARCH)
          &&!(localSearchMode[sd]&IIDSEARCH)
          &&localTodoIndex[sd-1]>1 // first move first
          &&localMoveCounter[sd-1]>1
          &&(ttindex2>1)
       )
      {
        move    = localMoveHistory[sd-1];
        bbWork  = localHashHistory[sd];    
        bbTemp  = bbWork&(ttindex2-1);
        score   = -INF;

        // check and set ply and sd, reset lock
        if (atom_cmpxchg(&TT2[bbTemp].ply, 0, ply_init+1)!=ply_init+1
            ||atom_cmpxchg(&TT2[bbTemp].sd, 0, search_depth+1)!=search_depth+1
           )
        {
          atom_xchg(&TT2[bbTemp].ply, ply_init+1);
          atom_xchg(&TT2[bbTemp].sd, search_depth+1);
          atom_xchg(&TT2[bbTemp].lock, 0);
        }
        // get lock
        n = atom_cmpxchg(&TT2[bbTemp].lock, 0, gid+1);
        // verify lock
        n = atom_cmpxchg(&TT2[bbTemp].lock, gid+1, gid+1);

        tt2 = TT2[bbTemp];
        score = (Score)tt2.score;

        // handle mate scores in TT
        score = (ISMATE(score)&&score>0)?score-ply:score;
        score = (ISMATE(score)&&score<0)?score+ply:score;

        // locked, backup move for iter 2
        if (n!=gid+1
            &&n>0
            &&(localNodeStates[sd-1]&ITER1)
           )
        {
          globalbbMoves2[gid*MAXPLY*64+(sd-1)*64+(s32)GETSQFROM(move)] |= SETMASKBB(GETSQTO(move));
          localTodoIndex[sd-1]--;
          movecount = 0;
          localAlphaBetaScores[sd*2+ALPHA] = INF; // ignore score
        }
        // locked and loaded, update alpha
        if (n!=gid+1
            &&n>0
            &&(localNodeStates[sd-1]&ITER2)
            &&tt2.hash==(bbWork^(Hash)tt2.score^(Hash)tt2.depth)
            &&tt2.depth>=(s16)localDepth[sd]
           )
        {
          if (
              !ISINF(score)
              &&score>localAlphaBetaScores[sd*2+ALPHA]
             )
          {
            // set alpha
            localAlphaBetaScores[sd*2+ALPHA] = score;
//            movecount = 0; // does not work, transpositions...
          }
        }
      } // end abdada, check hash table
    } // end ab flow x1
    if (lid==0)
    {
      // store move counter in local memory
      localMoveCounter[sd]  = movecount;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    // ################################
    // ####       movedown x64     ####
    // ################################
    // move down in tree
    // undomove x64
    while (
            (
              // all children searched
              localTodoIndex[sd]>=localMoveCounter[sd] 
              ||
              // apply alphabeta pruning downwards the tree
              localAlphaBetaScores[sd*2+ALPHA]>=localAlphaBetaScores[sd*2+BETA]
            )
            &&!bresearch
            &&!bforward
          ) 
    {

      // abdada, set values 
      if (lid==0&&sd>1&&ttindex2>1)
      {
        bbWork = localHashHistory[sd];    
        bbTemp = bbWork&(ttindex2-1);
        score  = localAlphaBetaScores[sd*2+ALPHA];

        // handle mate scores in TT, mate in => distance to mate
        score = (ISMATE(score)&&score>0)?score+ply:score;
        score = (ISMATE(score)&&score<0)?score-ply:score;

        // verify lock
        n = atom_cmpxchg(&TT2[bbTemp].lock, gid+1, gid+1);

        // set values
        if (n==gid+1
            &&!ISINF(score)
            &&!(localNodeStates[sd]&QS)
            &&!(localSearchMode[sd]&NULLMOVESEARCH)
            &&!(localSearchMode[sd]&IIDSEARCH)
           )
        {
          tt2.hash   = bbWork^(Hash)score^(Hash)localDepth[sd];
          tt2.score  = (TTScore)score;
          tt2.depth  = (s16)localDepth[sd];
          tt2.lock   = gid+1;
          tt2.ply    = ply_init+1;
          tt2.sd     = search_depth+1;

          TT2[bbTemp]=tt2;
        }
      }

      sd--;
      ply--;
      stm = !stm; // switch site to move

      undomove(board, localMoveHistory[sd]);

      // early bird
      if (lid==0&&sd<1)
       atom_cmpxchg(finito,0,(u32)gid+1);

      if (sd<1)
        break;

      barrier(CLK_LOCAL_MEM_FENCE);

      // ########################################
      // #### alphabeta negamax scoring++ x1 ####
      // ########################################
      if (lid==0)
      {
        u8 flag = FAILLOW;

        move  = localMoveHistory[sd];

        score = -localAlphaBetaScores[(sd+1)*2+ALPHA];

        // nullmove hack, avoid alpha setting, set score only when score >= beta
        if (move==NULLMOVE&&score<localAlphaBetaScores[sd*2+BETA])
          score = -INF;  // ignore score

        // late move reductions hack, init research
        if ((localNodeStates[sd]&LMR)&&score>localAlphaBetaScores[sd*2+ALPHA])
        {
          score = -INF;  // ignore score
          bresearch = true;
          // reset values
          localNodeStates[sd]^=LMR;
        }

        // set negamaxed alpha score
        if (score>localAlphaBetaScores[sd*2+ALPHA])
        {
          localAlphaBetaScores[sd*2+ALPHA]=score;
          flag = EXACTSCORE;

          // collect bestmove and score
          if (sd==1&&move!=MOVENONE&&move!=NULLMOVE)
          {
            bestscore = score;
            bestmove = move;
          }
        }
        if (score>=localAlphaBetaScores[sd*2+BETA])
          flag = FAILHIGH;

        // iid, collect move
        if ((localNodeStates[sd]&IID)&&flag>FAILLOW)
          localIIDMoves[sd] = move;
        // iid hack, init research
        if ((localNodeStates[sd]&IID)
            &&  
              (
               localAlphaBetaScores[sd*2+ALPHA]>=localAlphaBetaScores[sd*2+BETA]
               ||
               localTodoIndex[sd]>=localMoveCounter[sd] 
              )
           )
        {
          bforward = true;
          // reset values for new iteration
          localMoveHistory[sd]              = MOVENONE;
          localMoveCounter[sd]              = 0;
          localTodoIndex[sd]                = 0;
          localAlphaBetaScores[sd*2+ALPHA]  = -localAlphaBetaScores[(sd-1)*2+BETA];
          localAlphaBetaScores[sd*2+BETA]   = -localAlphaBetaScores[(sd-1)*2+ALPHA];
          localDepth[sd]                    = localDepth[sd-1]-1;
          localSearchMode[sd]               = localSearchMode[sd-1];
          // set iid done flag
          localNodeStates[sd]               = STATENONE | IIDDONE | ITER1;
        }

        // ###################################
        // ####     save to hash table    ####
        // ###################################
        // save to hash table
        if (
            flag>FAILLOW
            &&move!=MOVENONE
            &&move!=NULLMOVE
            &&!(localNodeStates[sd]&QS)
            &&!(localSearchMode[sd]&NULLMOVESEARCH)
            &&!(localSearchMode[sd]&IIDSEARCH)
            &&!(localNodeStates[sd]&LMR) // TODO: ?
            &&localDepth[sd]>=0
            &&!bforward
            &&!bresearch
            &&(ttindex1>1)
           )
        {
          bbWork = localHashHistory[sd];    
          bbTemp = bbWork&(ttindex1-1);

          // handle mate scores in TT, mate in => distance to mate
          score = (ISMATE(score)&&score>0)?score+ply:score;
          score = (ISMATE(score)&&score<0)?score-ply:score;

          // xor trick for avoiding race conditions
          bbMask = bbWork^(Hash)move^(Hash)score^(Hash)localDepth[sd];

          // slot 1, depth, score and ply replace
          tt1 = TT1[bbTemp]; 
          if (
               ((u8)localDepth[sd]>tt1.depth)
               ||
               ((u8)localDepth[sd]>=tt1.depth
                &&tt1.hash==(bbWork^(Hash)tt1.bestmove^(Hash)tt1.score^(Hash)tt1.depth)
                &&score>(Score)tt1.score
               )
               ||
               (ttage!=(tt1.flag>>2)
                &&ttage!=(tt1.flag>>2)+2
               )
             ) 
          {
              tt1.hash      = bbMask;
              tt1.bestmove  = (TTMove)move;
              tt1.score     = (TTScore)score;
              tt1.flag      = flag | (ttage&0x3F)<<2;
              tt1.depth     = (u8)localDepth[sd];
              TT1[bbTemp]   = tt1;
          }
        } // end save to hash table
        // save killer and counter move heuristic for quiet moves
        if (
            flag==FAILHIGH
            &&move!=MOVENONE
            &&move!=NULLMOVE
            &&GETPTYPE(GETPCPT(move))==PNONE // quiet moves only
            &&!(localNodeStates[sd]&QS)
            &&!(localSearchMode[sd]&NULLMOVESEARCH)
            &&!bforward
            &&!bresearch
           )
        {
          // save killer move
          Killers[gid*MAXPLY+sd] = move;
          // save counter move
          tmpmove = localMoveHistory[sd-1];
          Counters[gid*64*64+(s32)GETSQFROM(tmpmove)*64+(s32)GETSQTO(tmpmove)]=move;
        } // end save killer and counter move
        //clear lmr flag
        if (localNodeStates[sd]&LMR)
          localNodeStates[sd]^=LMR;
      } // end scoring x1
      barrier(CLK_LOCAL_MEM_FENCE);
    } // end while movedown loop x64
    if (lid==0)
    {
      // this is the end
      bexit = (sd<1)?true:bexit;
      // nodecount based termination
      bexit = (COUNTERS[1]>max_nodes)?true:bexit;
      // termination flag for helper threads
      bexit = (atom_cmpxchg(finito,0,0)>0)?true:bexit;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    if (bexit)
      break;
    if (bforward)
      continue;
    barrier(CLK_GLOBAL_MEM_FENCE);
    // ################################
    // ####        movepicker      ####
    // ################################
    // nullmove, ttmove, lmr-research
    if (lid==0)
    {
      // nullmove observation
      if (!bresearch
          &&sd>1
          &&localMoveHistory[sd]==MOVENONE
          &&!(localNodeStates[sd]&QS)
          &&!(localNodeStates[sd]&KIC)
          &&!(localNodeStates[sd]&EXT)
          &&!(localNodeStates[sd]&IIDDONE)
          &&!(localSearchMode[sd]&NULLMOVESEARCH)
          &&!(localSearchMode[sd]&IIDSEARCH)
          &&localDepth[sd]>=4
          )
      {
        lmove = NULLMOVE;
      }
      // late move reductions research
      if (bresearch)
        lmove = localMoveHistory[sd];

      // lazy smp, randomize move order
      if (
          lmove==MOVENONE
          &&(
             // single slot, randomize
             ((ttindex1>1&&ttindex2<=1)&&gid>0)
             ||
             // abdada, randomize > n
             ((ttindex2>1)&&(RANDABDADA)&&gid>=RANDWORKERS)
            )
          &&!(localNodeStates[sd]&QS)
          &&!(localNodeStates[sd]&KIC)
          &&!(localNodeStates[sd]&EXT)
          &&!(localSearchMode[sd]&NULLMOVESEARCH)
          &&!(localSearchMode[sd]&LMRSEARCH)
          &&!(localSearchMode[sd]&IIDSEARCH)
          &&localDepth[sd]>0
          // previous searched moves
          &&localTodoIndex[sd]>=RANDBRO
          )
      {
        brandomize = true;
      }
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    // ##################################
    // ####     movepicker x64 ITER1 ####
    // ##################################
    // move picker, extract moves x64 parallel
    // get moves from global stack
    bbMoves = globalbbMoves1[gid*MAXPLY*64+sd*64+(s32)lid];
    move = localMoveHistory[sd-1];
    // get killer move and counter move
    Move killermove = Killers[gid*MAXPLY+sd];
    Move countermove = Counters[gid*64*64+(s32)GETSQFROM(move)*64+(s32)GETSQTO(move)];
    // load move from transposition table
    Move ttmove = MOVENONE;
    bbWork = localHashHistory[sd];    
    bbTemp = bbWork&(ttindex1-1);
    if (ttindex1>1)
    {
      tt1 = TT1[bbTemp];
      if (tt1.hash==(bbWork^(Hash)tt1.bestmove^(Hash)tt1.score^(Hash)tt1.depth))
        ttmove = tt1.bestmove;
    }
    move    = MOVENONE;
    score  = -INFMOVESCORE;
    pfrom   = GETPIECE(board, lid);
    // pick best move from bitboard
    while(bbMoves&&!bresearch&&lmove==MOVENONE)
    {
      // xorshift32 PRNG
      prn ^= prn << 13;
      prn ^= prn >> 17;
      prn ^= prn << 5;

      sqto  = popfirst1(&bbMoves);
      sqcpt = sqto;
      // get piece captured
      pcpt  = GETPIECE(board, sqcpt);
      // check for en passant capture square
      sqcpt = (GETPTYPE(pfrom)==PAWN
               &&stm
               &&lid-sqto!=8
               &&lid-sqto!=16
               &&pcpt==PNONE)?(stm?sqto+8:sqto-8):sqcpt;
      sqcpt = (GETPTYPE(pfrom)==PAWN
               &&!stm
               &&sqto-lid!=8
               &&sqto-lid!=16
               &&pcpt==PNONE)?(stm?sqto+8:sqto-8):sqcpt;
      pcpt  = GETPIECE(board, sqcpt);
      pto   = pfrom;
      // set pawn promotion, queen
      pto   = (GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)==RANK_8)?
                MAKEPIECE(QUEEN,GETCOLOR(pfrom))
               :pfrom;
      // make move
      tmpmove  = MAKEMOVE((Move)lid, (Move)sqto, (Move)sqcpt, (Move)pfrom, (Move)pto, (Move)pcpt);
      // eval move
      // wood count and piece square tables, pto-pfrom   
      tmpscore = EvalPieceValues[GETPTYPE(pto)]
                 +EvalTable[GETPTYPE(pto)*64+((stm)?sqto:FLOP(sqto))]
                 +EvalControl[((stm)?sqto:FLOP(sqto))];

      tmpscore-= EvalPieceValues[GETPTYPE(pfrom)]
                 +EvalTable[GETPTYPE(pfrom)*64+((stm)?lid:FLOP(lid))]
                 +EvalControl[((stm)?lid:FLOP(lid))];
      // MVV-LVA
      tmpscore = (GETPTYPE(pcpt)!=PNONE)?
                  EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]
                 :tmpscore;
      // check counter move heuristic
      if (countermove==tmpmove)
      {
        // score as second highest quiet move
        tmpscore = EvalPieceValues[QUEEN]+EvalPieceValues[PAWN];
      }
      // check killer move heuristic
      if (killermove==tmpmove)
      {
        // score as highest quiet move
        tmpscore = EvalPieceValues[QUEEN]+EvalPieceValues[PAWN]*2; 
      }
      // lazy smp, randomize move order
      if (brandomize)
      {
        tmpscore = (prn%INF)+INF;
        // captures first
        tmpscore+= (GETPTYPE(pcpt)==PNONE)?0:INF;
      }
      // check iid move
      if (localIIDMoves[sd]==tmpmove)
      {
        // score as 2nd highest move
        tmpscore = INFMOVESCORE-200;
        // iid move hit counter
        COUNTERS[gid*64+5]++;
      }
      // check tt move
      if (ttmove==tmpmove)
      {
        // score as highest move
        tmpscore = INFMOVESCORE-100;
        // TThits counter
        COUNTERS[gid*64+3]++;
      }
      // get move with highest score
      move = (tmpscore>=score)?tmpmove:move;
      score = (tmpscore>=score)?tmpscore:score;
    }

#if defined cl_khr_local_int32_extended_atomics && !defined OLDSCHOOL
    // collect best movescore and bestmove x64
    atom_max(&movescore, score);
    barrier(CLK_LOCAL_MEM_FENCE);
    if (atom_cmpxchg(&movescore,score,score)==score&&!bresearch&&move!=MOVENONE)
      lmove = move;
#else
    // store score and move in local temp
    scrTmp64[lid] = score;
    bbTmp64[lid] = (u64)move;
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect best movescore and bestmove x1
    if (lid==0&&lmove==MOVENONE)
    {
      movescore = -INFMOVESCORE;
      for (int i=0;i<64;i++)
      {
        tmpscore = (Score)scrTmp64[i];
        if (tmpscore>movescore)
        {
          movescore = tmpscore;
          lmove = (Move)bbTmp64[i];
        }
      }
    }
#endif

    barrier(CLK_LOCAL_MEM_FENCE);
    movescore = -INFMOVESCORE;
    barrier(CLK_LOCAL_MEM_FENCE);

    // set ABDADA iter mode 2
    if (lid==0&&lmove==MOVENONE&&(localNodeStates[sd]&ITER1))
    {
      localNodeStates[sd]^=ITER1;
      localNodeStates[sd]|=ITER2;
    }

    // ##################################
    // ####     movepicker x64 ITER2 ####
    // ##################################
    // move picker, extract moves x64 parallel
    // get moves from global stack
    bbMoves = globalbbMoves2[gid*MAXPLY*64+sd*64+(s32)lid];
    move    = MOVENONE;
    score   = -INFMOVESCORE;
    pfrom   = GETPIECE(board, lid);
    // pick best move from bitboard
    while(bbMoves&&!bresearch&&lmove==MOVENONE)
    {
      // xorshift32 PRNG
      prn ^= prn << 13;
      prn ^= prn >> 17;
      prn ^= prn << 5;

      sqto  = popfirst1(&bbMoves);
      sqcpt = sqto;
      // get piece captured
      pcpt  = GETPIECE(board, sqcpt);
      // check for en passant capture square
      sqcpt = (GETPTYPE(pfrom)==PAWN
               &&stm
               &&lid-sqto!=8
               &&lid-sqto!=16
               &&pcpt==PNONE)?(stm?sqto+8:sqto-8):sqcpt;
      sqcpt = (GETPTYPE(pfrom)==PAWN
               &&!stm
               &&sqto-lid!=8
               &&sqto-lid!=16
               &&pcpt==PNONE)?(stm?sqto+8:sqto-8):sqcpt;
      pcpt  = GETPIECE(board, sqcpt);
      pto   = pfrom;
      // set pawn promotion, queen
      pto   = (GETPTYPE(pfrom)==PAWN&&GETRRANK(sqto,stm)==RANK_8)?
                MAKEPIECE(QUEEN,GETCOLOR(pfrom))
               :pfrom;
      // make move
      tmpmove  = MAKEMOVE((Move)lid, (Move)sqto, (Move)sqcpt, (Move)pfrom, (Move)pto, (Move)pcpt);
      // eval move
      // wood count and piece square tables, pto-pfrom   
      tmpscore = EvalPieceValues[GETPTYPE(pto)]
                 +EvalTable[GETPTYPE(pto)*64+((stm)?sqto:FLOP(sqto))]
                 +EvalControl[((stm)?sqto:FLOP(sqto))];

      tmpscore-= EvalPieceValues[GETPTYPE(pfrom)]
                 +EvalTable[GETPTYPE(pfrom)*64+((stm)?lid:FLOP(lid))]
                 +EvalControl[((stm)?lid:FLOP(lid))];
      // MVV-LVA
      tmpscore = (GETPTYPE(pcpt)!=PNONE)?
                  EvalPieceValues[GETPTYPE(pcpt)]*16-EvalPieceValues[GETPTYPE(pto)]
                 :tmpscore;
      // check counter move heuristic
      if (countermove==tmpmove)
      {
        // score as second highest quiet move
        tmpscore = EvalPieceValues[QUEEN]+EvalPieceValues[PAWN];
      }
      // check killer move heuristic
      if (killermove==tmpmove)
      {
        // score as highest quiet move
        tmpscore = EvalPieceValues[QUEEN]+EvalPieceValues[PAWN]*2; 
      }
      // lazy smp, randomize move order
      if (brandomize)
      {
        tmpscore = (prn%INF)+INF;
      }
      // check iid move
      if (localIIDMoves[sd]==tmpmove)
      {
        // score as 2nd highest move
        tmpscore = INFMOVESCORE-200;
        // iid move hit counter
        COUNTERS[gid*64+5]++;
      }
      // check tt move
      if (ttmove==tmpmove)
      {
        // score as highest move
        tmpscore = INFMOVESCORE-100;
        // TThits counter
        COUNTERS[gid*64+3]++;
      }
      // get move with highest score
      move = (tmpscore>=score)?tmpmove:move;
      score = (tmpscore>=score)?tmpscore:score;
    }

#if defined cl_khr_local_int32_extended_atomics && !defined OLDSCHOOL
    // collect best movescore and bestmove x64
    atom_max(&movescore, score);
    barrier(CLK_LOCAL_MEM_FENCE);
    if (atom_cmpxchg(&movescore,score,score)==score&&!bresearch&&move!=MOVENONE)
      lmove = move;
#else
    // store score and move in local temp
    scrTmp64[lid] = score;
    bbTmp64[lid] = (u64)move;
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect best movescore and bestmove x1
    if (lid==0&&lmove==MOVENONE)
    {
      movescore = -INFMOVESCORE;
      for (int i=0;i<64;i++)
      {
        tmpscore = (Score)scrTmp64[i];
        if (tmpscore>movescore)
        {
          movescore = tmpscore;
          lmove = (Move)bbTmp64[i];
        }
      }
    }
#endif

    bbAttacks = HASHNONE;
    barrier(CLK_LOCAL_MEM_FENCE);

    // ################################
    // ####         moveup         ####
    // ################################
    // move up in tree
    move = lmove;
    if (lid==0&&!bresearch)
    {
      // clear move from bb moves
      if (move!=NULLMOVE&&move!=MOVENONE)
      {
        if (localNodeStates[sd]&ITER1)
          globalbbMoves1[gid*MAXPLY*64+sd*64+(s32)GETSQFROM(move)]&=CLRMASKBB(GETSQTO(move));
        else
          globalbbMoves2[gid*MAXPLY*64+sd*64+(s32)GETSQFROM(move)]&=CLRMASKBB(GETSQTO(move));
        // nodecounter
        COUNTERS[gid*64+1]++;
      }
      // set history
      localTodoIndex[sd]++; // inc anyway, safety first
      localMoveHistory[sd]=move;
    }

    // domove x64
    domove(board, move);
    stm = !stm; // switch site to move
    sd++;       // increase depth counter
    ply++;      // increase ply counter

    // compute hash x64
    bbWork = HASHNONE; // set empty hash
    pfrom = GETPIECE(board,lid);
    bbTemp = (GETPTYPE(pfrom))?Zobrist[GETCOLOR(pfrom)*6+GETPTYPE(pfrom)-1]:HASHNONE;
    bbTemp = ((bbTemp<<lid)|(bbTemp>>(64-lid))); // rotate left 64

#if defined cl_khr_int64_extended_atomics && !defined OLDSCHOOL
    // collect hashes x64
    atom_xor(&bbAttacks, bbTemp);
    barrier(CLK_LOCAL_MEM_FENCE);
    if (lid==0)
        bbWork = bbAttacks;
#else
    // store hash in local temp
    bbTmp64[lid] =  bbTemp;
    barrier(CLK_LOCAL_MEM_FENCE);
    // collect hashes x1
    if (lid==0)
      for (int i=0;i<64;i++)
        bbWork ^= bbTmp64[i];
#endif

    // moveup x1
    if (lid==0)
    {
      // en passant flag
      sqto = ( GETPTYPE(GETPFROM(move))==PAWN
              &&GETRRANK(GETSQTO(move),GETCOLOR(GETPFROM(move)))-GETRRANK(GETSQFROM(move),GETCOLOR(GETPFROM(move)))==2
              )?GETSQTO(move):0x0;
      if (sqto) 
        bbWork ^= ((Zobrist[16]<<GETFILE(sqto))|(Zobrist[16]>>(64-GETFILE(sqto))));
      // site to move
      if (stm)
        bbWork ^= 0x1UL;

      // set piece moved flags for castle right
      bbTemp   = localCrHistory[sd-1];
      bbTemp  |= SETMASKBB(GETSQFROM(move));
      bbTemp  |= SETMASKBB(GETSQTO(move));
      bbTemp  |= SETMASKBB(GETSQCPT(move));
      // set en passant target square in piece moved flags
      sqep   = ( GETPTYPE(GETPFROM(move))==PAWN
                 &&GETRRANK(GETSQTO(move),GETCOLOR(GETPFROM(move)))-GETRRANK(GETSQFROM(move),GETCOLOR(GETPFROM(move)))==2
               )?GETSQTO(move):0x0;
      bbTemp |= 0x000000FFFF000000;
      if (sqep)
        bbTemp &= CLRMASKBB(sqep);
      // store in local
      localCrHistory[sd] = bbTemp;
      // compute hash castle rights
      if ((~bbTemp)&SMCRWHITEQ)
        bbWork^= Zobrist[12];
      if ((~bbTemp)&SMCRWHITEK)
        bbWork^= Zobrist[13];
      if ((~bbTemp)&SMCRBLACKQ)
        bbWork^= Zobrist[14];
      if ((~bbTemp)&SMCRBLACKK)
        bbWork^= Zobrist[15];
      // compute hash nullmove
      if (move==NULLMOVE)
        bbWork ^= 0x1UL;
      // set new zobrist hash
      localHashHistory[sd]=bbWork;
      HashHistory[gid*MAXGAMEPLY+ply+ply_init]=bbWork;
      // halfmove clock
      localHMCHistory[sd]=localHMCHistory[sd-1]+1; // increase
      // reset hmc
      if ((GETPTYPE(GETPFROM(move))==PAWN))  // pawn move
        localHMCHistory[sd] = 0;
      if ((GETPTYPE(GETPCPT(move))!=PNONE)) // capture
        localHMCHistory[sd] = 0;
      // castle queenside
      if ((GETPTYPE(GETPFROM(move))==KING)&&(GETSQFROM(move)-GETSQTO(move)==2))
        localHMCHistory[sd] = 0;
      // castle kingside
      if ((GETPTYPE(GETPFROM(move))==KING)&&(GETSQTO(move)-GETSQFROM(move)==2))
        localHMCHistory[sd] = 0;

      // set values for next depth
      localMoveHistory[sd]              = MOVENONE;
      localIIDMoves[sd]                 = MOVENONE;
      localMoveCounter[sd]              = 0;
      localTodoIndex[sd]                = 0;
      localAlphaBetaScores[sd*2+ALPHA]  = -localAlphaBetaScores[(sd-1)*2+BETA];
      localAlphaBetaScores[sd*2+BETA]   = -localAlphaBetaScores[(sd-1)*2+ALPHA];
      localDepth[sd]                    = localDepth[sd-1]-1; // decrease depth
      localSearchMode[sd]               = localSearchMode[sd-1];
      localNodeStates[sd]               = STATENONE | ITER1;

      // set values and alpha beta window for IID search
      if (!bresearch
         &&sd>2 // not on root
         &&!brandomize
         &&move!=NULLMOVE
         &&move!=MOVENONE
         &&ttmove==MOVENONE
         &&localTodoIndex[sd-1]==1 // iid only on first move
         &&localMoveCounter[sd-1]>1
         &&localDepth[sd-1]>5
         &&!(localNodeStates[sd-1]&IID) 
         &&!(localNodeStates[sd-1]&IIDDONE) 
         &&!(localNodeStates[sd-1]&QS)
       )
      {
        localNodeStates[sd-1]                |= IID;
        localAlphaBetaScores[(sd-1)*2+ALPHA]  = -INF;
        localAlphaBetaScores[(sd-1)*2+BETA]   = +INF;
        localAlphaBetaScores[sd*2+ALPHA]      = -INF;
        localAlphaBetaScores[sd*2+BETA]       = +INF;
      }
      if (localNodeStates[sd-1]&IID)
      {
        localSearchMode[sd]                  |= IIDSEARCH;
        localDepth[sd]                        = localDepth[sd-1]/5; // new depth
      }

      // set values and alpha beta window for nullmove search
      if (move==NULLMOVE)
      {
        localTodoIndex[sd-1]--;
        localSearchMode[sd]              |= NULLMOVESEARCH;
        localDepth[sd]                   -= NULLR; // depth reduction
        localAlphaBetaScores[sd*2+ALPHA]  = -localAlphaBetaScores[(sd-1)*2+BETA];
        localAlphaBetaScores[sd*2+BETA]   = -localAlphaBetaScores[(sd-1)*2+BETA]+1;
      }

      // set values for late move reduction search
      if (!bresearch
         &&sd>2  // not on root
         &&move!=NULLMOVE
         &&move!=MOVENONE
         &&GETPTYPE(GETPCPT(move))==PNONE     // quiet moves only
         &&!(localNodeStates[sd-1]&IID)
         &&!(localNodeStates[sd-1]&QS)
         &&!(localNodeStates[sd-1]&KIC)
         &&!(localNodeStates[sd-1]&EXT)
         &&localTodoIndex[sd-1]>2 // previous moves searched
         &&localDepth[sd-1]>=2
         &&count1s(board[QBBBLACK])>=2
         &&count1s(board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]))>=2
        )
      {
        localDepth[sd]        -= LMRR; // depth reduction
        localNodeStates[sd-1] |= LMR;
        localSearchMode[sd]   |= LMRSEARCH;
      }
      // get a bestmove anyway
      if (sd==2&&localTodoIndex[sd-1]==1)
        bestmove = move;
    } // end moveup x1
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
  } // end while main loop
  // ################################
  // ####      collect pv        ####
  // ################################
  barrier(CLK_LOCAL_MEM_FENCE);
  barrier(CLK_GLOBAL_MEM_FENCE);
  // collect pv for gui output
  if (lid==0&&atom_cmpxchg(finito,(u32)gid+1,(u32)gid+1)==(u32)gid+1)
  {
    // get init quadbitboard plus plus
    board[QBBBLACK] = BOARD[QBBBLACK];
    board[QBBP1]    = BOARD[QBBP1];
    board[QBBP2]    = BOARD[QBBP2];
    board[QBBP3]    = BOARD[QBBP3];
    bbWork          = BOARD[QBBHASH]; // hash
    bbMask          = BOARD[QBBPMVD]; // bb castle rights
    bbTemp          = bbWork&(ttindex1-1);
    stm             = (bool)stm_init;
    n               = 1;
    PV[0]           = (Score)bestscore;
    while(n<MAXPLY)
    {

      PV[n] = bestmove;

      domove(board, bestmove);
      stm = !stm;
      // set piece moved flags for castle right
      bbMask  |= SETMASKBB(GETSQFROM(bestmove));
      bbMask  |= SETMASKBB(GETSQTO(bestmove));
      bbMask  |= SETMASKBB(GETSQCPT(bestmove));
      // set en passant target square in piece moved flags
      sqep   = ( GETPTYPE(GETPFROM(bestmove))==PAWN
                 &&GETRRANK(GETSQTO(bestmove),GETCOLOR(GETPFROM(bestmove)))-GETRRANK(GETSQFROM(bestmove),GETCOLOR(GETPFROM(bestmove)))==2
               )?GETSQTO(bestmove):0x0;
      bbMask |= 0x000000FFFF000000;
      if (sqep)
        bbMask &= CLRMASKBB(sqep);
      // compute hash x1
      bbWork = computehash(board, stm, bbMask);
      bbTemp = bbWork&(ttindex1-1);

      bestmove = MOVENONE;
      bestscore = -INF;

      // load ttmove from hash table
      if (ttindex1>1)
      {
        tt1 = TT1[bbTemp];
        if (tt1.hash==
              (bbWork^(Hash)tt1.bestmove^(Hash)tt1.score^(Hash)tt1.depth))
        {
          bestscore = (Score)tt1.score;
          bestmove = (Move)tt1.bestmove;
        }
      }

      if (bestmove==MOVENONE||ISINF(bestscore))
        break;

      n++;
    }

  } // end collect pv
} // end kernel alphabeta_gpu

