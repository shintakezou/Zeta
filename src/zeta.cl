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

// mandatory
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics       : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics   : enable
// otpional
#pragma OPENCL EXTENSION cl_khr_byte_addressable_store          : enable

typedef ulong   u64;
typedef uint    u32;
typedef int     s32;
typedef uchar    u8;

typedef s32     Score;
typedef u8      Square;
typedef u8      Piece;

typedef u64     Cr;
typedef u64     Move;
typedef u64     Bitboard;
typedef u64     Hash;

// node tree entry
typedef struct
{
  Move move;
  Score score;
  s32 lock;
  s32 visits;
  s32 child;
  s32 children;
  s32 parent;
} NodeBlock;

// bf modes
#define INIT            0
#define SELECT          1
#define EXPAND          2
#define EVALLEAF        3
#define MOVEUP          4
#define MOVEDOWN        5
#define UPDATESCORE     6
#define BACKUPSCORE     7
// defaults
#define VERSION "098e"
/* quad bitboard array index definition */
#define QBBBLACK  0     /* pieces white */
#define QBBP1     1     /* piece type first bit */
#define QBBP2     2     /* piece type second bit */
#define QBBP3     3     /* piece type third bit */
#define QBBPMVD   4     /* piece moved flags, for castle rights */
#define QBBHASH   5     /* 64 bit board Zobrist hash */
#define QBBLAST   6     /* lastmove + ep target + halfmove clock + move score */
/* move encoding 
   0  -  5  square from
   6  - 11  square to
  12  - 17  square capture
  18  - 21  piece from
  22  - 25  piece to
  26  - 29  piece capture
  30  - 35  square en passant target
  36        move is castle kingside
  37        move is castle queenside
  38  - 39  2 bit free
  40  - 47  halfmove clock for fity move rule, last capture/castle/pawn move
  48  - 63  move score, signed 16 bit
*/
// engine defaults
#define MAXGAMEPLY  1024    // max ply a game can reach
#define MAXMOVES    256     // max amount of legal moves per position
#define TIMESPARE   100     // 100 milliseconds spare
#define MINDEVICEMB 64
#define MAXDEVICEMB 1024
/* colors */
#define BLACK               1
#define WHITE               0
// score indexing
#define ALPHA   0
#define BETA    1
/* scores */
#define INF             1000000
#define MATESCORE        999000
#define DRAWSCORE       0
#define STALEMATESCORE  0
/* piece type enumeration */
#define PNONE               0
#define PAWN                1
#define KNIGHT              2
#define KING                3
#define BISHOP              4
#define ROOK                5
#define QUEEN               6
/* move is castle flag */
#define MOVEISCR            0x0000003000000000
#define MOVEISCRK           0x0000001000000000
#define MOVEISCRQ           0x0000002000000000
/* bitboard masks, computation prefered over lookup */
#define SETMASKBB(sq)       ((ulong)1<<(sq))
#define CLRMASKBB(sq)       (~((ulong)1<<(sq)))
/* u64 defaults */
#define BBEMPTY             0x0000000000000000
#define BBFULL              0xFFFFFFFFFFFFFFFF
#define MOVENONE            0x0000000000000000
#define HASHNONE            0x0000000000000000
#define CRNONE              0x0000000000000000
#define SCORENONE           0x0000000000000000
/* set masks */
#define SMMOVE              0x0000003FFFFFFFFF
#define SMSQEP              0x0000000FC0000000
#define SMHMC               0x0000FF0000000000
#define SMCRALL             0x8900000000000091
#define SMSCORE             0xFFFF000000000000
#define SMTTMOVE            0x000000003FFFFFFF
/* clear masks */
#define CMMOVE              0xFFFFFFC000000000
#define CMSQEP              0xFFFFFFF03FFFFFFF
#define CMHMC               0xFFFF00FFFFFFFFFF
#define CMCRALL             0x76FFFFFFFFFFFF6E
#define CMSCORE             0x0000FFFFFFFFFFFF
#define CMTTMOVE            0xFFFFFFFFC0000000
/* castle right masks */
#define SMCRWHITE           0x0000000000000091
#define SMCRWHITEQ          0x0000000000000011
#define SMCRWHITEK          0x0000000000000090
#define SMCRBLACK           0x9100000000000000
#define SMCRBLACKQ          0x1100000000000000
#define SMCRBLACKK          0x9000000000000000
/* move helpers */
#define MAKEPIECE(p,c)     ((((Piece)p)<<1)|(Piece)c)
#define JUSTMOVE(move)     (move&SMMOVE)
#define GETCOLOR(p)        ((p)&0x1)
#define GETPTYPE(p)        (((p)>>1)&0x7)      /* 3 bit piece type encoding */
#define GETSQFROM(mv)      ((mv)&0x3F)         /* 6 bit square */
#define GETSQTO(mv)        (((mv)>>6)&0x3F)    /* 6 bit square */
#define GETSQCPT(mv)       (((mv)>>12)&0x3F)   /* 6 bit square */
#define GETPFROM(mv)       (((mv)>>18)&0xF)    /* 4 bit piece encoding */
#define GETPTO(mv)         (((mv)>>22)&0xF)    /* 4 bit piece encoding */
#define GETPCPT(mv)        (((mv)>>26)&0xF)    /* 4 bit piece encodinge */
#define GETSQEP(mv)        (((mv)>>30)&0x3F)   /* 6 bit square */
#define SETSQEP(mv,sq)     (((mv)&CMSQEP)|(((sq)&0x3F)<<30))
#define GETHMC(mv)         (((mv)>>40)&0xFF)   /* 8 bit halfmove clock */
#define SETHMC(mv,hmc)     (((mv)&CMHMC)|(((hmc)&0xFF)<<40))
#define GETSCORE(mv)       (((mv)>>48)&0xFFFF) /* signed 16 bit move score */
#define SETSCORE(mv,score) (((mv)&CMSCORE)|(((score)&0xFFFF)<<48)) 
/* pack move into 64 bits */
#define MAKEMOVE(sqfrom, sqto, sqcpt, pfrom, pto, pcpt, sqep, hmc, score) \
( \
     sqfrom      | (sqto<<6)  | (sqcpt<<12) \
  | (pfrom<<18)  | (pto<<22)  | (pcpt<<26) \
  | (sqep<<30)   | (hmc<<40)  | (score<<48) \
)
/* square helpers */
#define MAKESQ(file,rank)   ((rank)<<3|(file))
#define GETRANK(sq)         ((sq)>>3)
#define GETFILE(sq)         ((sq)&7)
#define GETRRANK(sq,color)  ((color)?(((sq)>>3)^7):((sq)>>3))
#define FLIP(sq)            ((sq)^7)
#define FLOP(sq)            ((sq)^56)
#define FLIPFLOP(sq)        (((sq)^56)^7)
/* piece helpers */
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
/* file enumeration */
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
/* rank enumeration */
enum Ranks
{
  RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NONE
};
#define BBRANK7             0x00FF000000000000
#define BBRANK5             0x000000FF00000000
#define BBRANK4             0x00000000FF000000
#define BBRANK2             0x000000000000FF00
/* square enumeration */
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
/* is score a mate in n */
#define ISMATE(val)           ((((val)>MATESCORE&&(val)<INF)||((val)<-MATESCORE&&(val)>-INF))?true:false)
/* is score default inf */
#define ISINF(val)            (((val)==INF||(val)==-INF)?true:false)
// tuneable search parameter
#define MAXEVASIONS          4               // max check evasions from qsearch
#define SMOOTHUCT            1.00           // factor for uct params in select formula
#define SKIPMATE             1             // 0 or 1
#define SKIPDRAW             1            // 0 or 1
#define INCHECKEXT           1           // 0 or 1
#define SINGLEEXT            1          // 0 or 1
#define PROMOEXT             1         // 0 or 1
#define ROOTSEARCH           1        // 0 or 1, distribute root nodes equaly in select phase
#define SCOREWEIGHT          0.33    // factor for board score in select formula
#define BROADWELL            1      // 0 or 1, will apply bf select formula
#define DEPTHWELL            32    // 0 to totalThreads, every nth thread will search depth wise
#define MAXBFPLY             128  // max ply of bf search tree
// functions
Score EvalMove(Move move);
// rotate left based zobrist hashing
__constant Hash Zobrist[17]=
{
  0x9D39247E33776D41, 0x2AF7398005AAA5C7, 0x44DB015024623547, 0x9C15F73E62A76AE2,
  0x75834465489C0C89, 0x3290AC3A203001BF, 0x0FBBAD1F61042279, 0xE83A908FF2FB60CA,
  0x0D7E765D58755C10, 0x1A083822CEAFE02D, 0x9605D5F0E25EC3B0, 0xD021FF5CD13A2ED5,
  0x40BDF15D4A672E32, 0x011355146FD56395, 0x5DB4832046F3D9E5, 0x239F8B2D7FF719CC,
  0x05D1A1AE85B49AA1
};
/* 
  piece square tables based on proposal by Tomasz Michniewski
  https://chessprogramming.wikispaces.com/Simplified+evaluation+function
*/

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
// wrapper for casting
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
/* bit twiddling hacks
  bb_work=bb_temp&-bb_temp;  // get lsb 
  bb_temp&=bb_temp-1;       // clear lsb
*/
/* apply move on board, quick during move generation */
void domovequick(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Bitboard pto    = GETPTO(move);
  Bitboard bbTemp = BBEMPTY;

  /* check for edges */
  if (move==MOVENONE)
    return;

  /* unset square from, square capture and square to */
  bbTemp = CLRMASKBB(sqfrom)&CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* set piece to */
  board[QBBBLACK] |= (pto&0x1)<<sqto;
  board[QBBP1]    |= ((pto>>1)&0x1)<<sqto;
  board[QBBP2]    |= ((pto>>2)&0x1)<<sqto;
  board[QBBP3]    |= ((pto>>3)&0x1)<<sqto;
}
/* restore board again, quick during move generation */
void undomovequick(Bitboard *board, Move move)
{
  Square sqfrom   = GETSQFROM(move);
  Square sqto     = GETSQTO(move);
  Square sqcpt    = GETSQCPT(move);
  Bitboard pfrom  = GETPFROM(move);
  Bitboard pcpt   = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;

  /* check for edges */
  if (move==MOVENONE)
    return;

  /* unset square capture, square to */
  bbTemp = CLRMASKBB(sqcpt)&CLRMASKBB(sqto);
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;

  /* restore piece capture */
  board[QBBBLACK] |= (pcpt&0x1)<<sqcpt;
  board[QBBP1]    |= ((pcpt>>1)&0x1)<<sqcpt;
  board[QBBP2]    |= ((pcpt>>2)&0x1)<<sqcpt;
  board[QBBP3]    |= ((pcpt>>3)&0x1)<<sqcpt;

  /* restore piece from */
  board[QBBBLACK] |= (pfrom&0x1)<<sqfrom;
  board[QBBP1]    |= ((pfrom>>1)&0x1)<<sqfrom;
  board[QBBP2]    |= ((pfrom>>2)&0x1)<<sqfrom;
  board[QBBP3]    |= ((pfrom>>3)&0x1)<<sqfrom;
}
Hash computehash(__private Bitboard *board, bool stm)
{
  Piece piece;
  Bitboard bbWork;
  Square sq;
  Hash hash = HASHNONE;
  Hash zobrist;
  u8 side;

  /* for each color */
  for (side=WHITE;side<=BLACK;side++)
  {
    bbWork = (side==BLACK)?board[QBBBLACK]:(board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]));
    /* for each piece */
    while(bbWork)
    {
      sq    = popfirst1(&bbWork);
      piece = GETPIECE(board,sq);
      zobrist = Zobrist[GETCOLOR(piece)*6+GETPTYPE(piece)-1];
      hash ^= ((zobrist<<sq)|(zobrist>>(64-sq)));; // rotate left 64
    }
  }
  /* castle rights
  if (((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
      hash ^= Zobrist[12];
  if (((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
      hash ^= Zobrist[13];
  if (((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
      hash ^= Zobrist[14];
  if (((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
      hash ^= Zobrist[15];
 */
  /* file en passant
  if (GETSQEP(board[QBBLAST]))
  {
    sq = GETFILE(GETSQEP(board[QBBLAST]));
    zobrist = Zobrist[16];
    hash ^= ((zobrist<<sq)|(zobrist>>(64-sq)));; // rotate left 64
  }
 */
  /* site to move */
  if (!stm)
    hash ^= 0x1;

  return hash;
}
// update castle rights, TODO: make nice
Move updateCR(Move move, Cr cr)
{

  Square from   =  (Square)(move&0x3F);
  Piece piece   =  (Piece)(((move>>18)&0xF)>>1);
  bool som      =  (bool)((move>>18)&0xF)&1;

  // clear white queenside
  cr&= ((piece==ROOK&&som==WHITE&&from==0)||(piece==KING&&som==WHITE&&from==4))?0xE:0xF;
  // clear white kingside
  cr&= ((piece==ROOK&&som==WHITE&&from==7)||(piece==KING&&som==WHITE&&from==4))?0xD:0xF;
  // clear black queenside
  cr&= ((piece==ROOK&&som==BLACK&&from==56)||(piece==KING&&som==BLACK&&from==60))?0xB:0xF;
  // clear black kingside
  cr&= ((piece==ROOK&&som==BLACK&&from==63)||(piece==KING&&som==BLACK&&from==60))?0x7:0xF;

  move &= 0xFFFFFF0FFFFFFFFF; // clear old cr
  move |= ((Move)cr<<36)&0x000000F000000000; // set new cr

  return move;
}
// move generator costants
__constant ulong4 shift4 = (ulong4)( 9, 1, 7, 8);

// wraps for kogge stone shifts
__constant ulong4 wraps4[2] =
{
  // wraps shift left
  (ulong4)(
            0xfefefefefefefe00, // <<9
            0xfefefefefefefefe, // <<1
            0x7f7f7f7f7f7f7f00, // <<7
            0xffffffffffffff00  // <<8
          ),

  // wraps shift right
  (ulong4)(
            0x007f7f7f7f7f7f7f, // >>9
            0x7f7f7f7f7f7f7f7f, // >>1
            0x00fefefefefefefe, // >>7
            0x00ffffffffffffff  // >>8
          )
};
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
// generate rook moves via koggestone shifts
Bitboard ks_attacks_ls1(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions left shifting <<1 ROOK
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  // do kogge stone
  bbGen  |= bbPro &   (bbGen << 1);
  bbPro  &=           (bbPro << 1);
  bbGen  |= bbPro &   (bbGen << 2*1);
  bbPro  &=           (bbPro << 2*1);
  bbGen  |= bbPro &   (bbGen << 4*1);
  // shift one further
  bbGen   = bbWrap &  (bbGen << 1);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_ls8(Bitboard bbBlockers, Square sq)
{
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions left shifting <<8 ROOK
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  // do kogge stone
  bbGen  |= bbPro &   (bbGen << 8);
  bbPro  &=           (bbPro << 8);
  bbGen  |= bbPro &   (bbGen << 2*8);
  bbPro  &=           (bbPro << 2*8);
  bbGen  |= bbPro &   (bbGen << 4*8);
  // shift one further
  bbGen   =           (bbGen << 8);
  bbMoves|= bbGen;
  
  return bbMoves;
}
Bitboard ks_attacks_rs1(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions right shifting >>1 ROOK
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  // do kogge stone
  bbGen  |= bbPro &   (bbGen >> 1);
  bbPro  &=           (bbPro >> 1);
  bbGen  |= bbPro &   (bbGen >> 2*1);
  bbPro  &=           (bbPro >> 2*1);
  bbGen  |= bbPro &   (bbGen >> 4*1);
  // shift one further
  bbGen   = bbWrap &  (bbGen >> 1);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_rs8(Bitboard bbBlockers, Square sq)
{
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions right shifting >>8 ROOK
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  // do kogge stone
  bbGen  |= bbPro &   (bbGen >> 8);
  bbPro  &=           (bbPro >> 8);
  bbGen  |= bbPro &   (bbGen >> 2*8);
  bbPro  &=           (bbPro >> 2*8);
  bbGen  |= bbPro &   (bbGen >> 4*8);
  // shift one further
  bbGen   =           (bbGen >> 8);
  bbMoves|= bbGen;  

  return bbMoves;
}
Bitboard rook_attacks(Bitboard bbBlockers, Square sq)
{
  return ks_attacks_ls1(bbBlockers, sq) |
         ks_attacks_ls8(bbBlockers, sq) |
         ks_attacks_rs1(bbBlockers, sq) |
         ks_attacks_rs8(bbBlockers, sq);
}
// generate bishop moves via koggestone shifts
Bitboard ks_attacks_ls9(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions left shifting <<9 BISHOP
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  // do kogge stone
  bbGen  |= bbPro &   (bbGen << 9);
  bbPro  &=           (bbPro << 9);
  bbGen  |= bbPro &   (bbGen << 2*9);
  bbPro  &=           (bbPro << 2*9);
  bbGen  |= bbPro &   (bbGen << 4*9);
  // shift one further
  bbGen   = bbWrap &  (bbGen << 9);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_ls7(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions left shifting <<7 BISHOP
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  // do kogge stone
  bbGen  |= bbPro &   (bbGen << 7);
  bbPro  &=           (bbPro << 7);
  bbGen  |= bbPro &   (bbGen << 2*7);
  bbPro  &=           (bbPro << 2*7);
  bbGen  |= bbPro &   (bbGen << 4*7);
  // shift one further
  bbGen   = bbWrap &  (bbGen << 7);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_rs9(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions right shifting >>9 ROOK
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  // do kogge stone
  bbGen  |= bbPro &   (bbGen >> 9);
  bbPro  &=           (bbPro >> 9);
  bbGen  |= bbPro &   (bbGen >> 2*9);
  bbPro  &=           (bbPro >> 2*9);
  bbGen  |= bbPro &   (bbGen >> 4*9);
  // shift one further
  bbGen   = bbWrap &  (bbGen >> 9);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_rs7(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  // directions right shifting <<7 ROOK
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  // do kogge stone
  bbGen  |= bbPro &   (bbGen >> 7);
  bbPro  &=           (bbPro >> 7);
  bbGen  |= bbPro &   (bbGen >> 2*7);
  bbPro  &=           (bbPro >> 2*7);
  bbGen  |= bbPro &   (bbGen >> 4*7);
  // shift one further
  bbGen   = bbWrap &  (bbGen >> 7);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard bishop_attacks(Bitboard bbBlockers, Square sq)
{
  return ks_attacks_ls7(bbBlockers, sq) |
         ks_attacks_ls9(bbBlockers, sq) |
         ks_attacks_rs7(bbBlockers, sq) |
         ks_attacks_rs9(bbBlockers, sq);
}
Square getkingpos(__private Bitboard *board, bool side)
{
  Bitboard bbTemp = (side)?board[0]:board[0]^(board[1]|board[2]|board[3]);;
  bbTemp &= board[1]&board[2]&~board[3]; // get king
  return first1(bbTemp);
}
// is square attacked by an enemy piece, via superpiece approach
bool squareunderattack(__private Bitboard *board, bool stm, Square sq) 
{
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbMe;

  bbBlockers = board[1]|board[2]|board[3];
  bbMe       = (stm)?board[0]:(board[0]^bbBlockers);

  // rooks and queens
  bbMoves = rook_attacks(bbBlockers, sq);
  bbWork =    (bbMe&(board[1]&~board[2]&board[3])) 
            | (bbMe&(~board[1]&board[2]&board[3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  bbMoves = bishop_attacks(bbBlockers, sq);
  // bishops and queens
  bbWork =  (bbMe&(~board[1]&~board[2]&board[3])) 
          | (bbMe&(~board[1]&board[2]&board[3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  // knights
  bbWork = bbMe&(~board[1]&board[2]&~board[3]);
  bbMoves = AttackTables[128+sq] ;
  if (bbMoves&bbWork) 
  {
    return true;
  }
  // pawns
  bbWork = bbMe&(board[1]&~board[2]&~board[3]);
  bbMoves = AttackTables[!stm*64+sq];
  if (bbMoves&bbWork)
  {
    return true;
  }
  // king
  bbWork = bbMe&(board[1]&board[2]&~board[3]);
  bbMoves = AttackTables[192+sq];
  if (bbMoves&bbWork)
  {
    return true;
  } 

  return false;
}
// kogge stone vector based move generator
void gen_moves(
                __private Bitboard *board, 
                          s32 *n, 
                          bool som, 
                          bool qs, 
                          s32 sd, 
                          const s32 pid, 
                          const s32 max_depth,
                __global  Move *global_pid_moves, 
                          bool rootkic
)
{
  bool kic = false;
  Score score;
  s32 i;
  s32 j;
  Square kingpos;
  Square pos;
  Square to;
  Square cpt;   
  Square ep;
  Piece piece;
  Piece pieceto;
  Piece piececpt;
  Move move;
  Move lastmove = board[QBBLAST];
//  Move tmpmove = 0;
  Bitboard bbBlockers     = board[1]|board[2]|board[3];
  Bitboard bbMe           = (som)?board[0]:(board[0]^bbBlockers);
  Bitboard bbOpp          = (!som)?board[0]:(board[0]^bbBlockers);
  Bitboard bbTemp;
  Bitboard bbTempO;
  Bitboard bbWork;
  Bitboard bbMoves;

  ulong4 bbWrap4;
  ulong4 bbPro4;
  ulong4 bbGen4; 

  bbWork = bbMe;

  while(bbWork)
  {
    pos     = popfirst1(&bbWork);
    piece   = GETPIECE(board,pos);

    bbTempO = SETMASKBB(pos);
    // get koggestone wraps
    bbWrap4 = wraps4[0];
    // generator and propagator (piece and empty squares)
    bbGen4  = (ulong4)bbTempO;
    bbPro4  = (ulong4)(~bbBlockers);
    // kogge stone shift left via ulong4 vector
    bbPro4 &= bbWrap4;
    bbGen4 |= bbPro4  & (bbGen4 << shift4);
    bbPro4 &=           (bbPro4 << shift4);
    bbGen4 |= bbPro4  & (bbGen4 << 2*shift4);
    bbPro4 &=           (bbPro4 << 2*shift4);
    bbGen4 |= bbPro4  & (bbGen4 << 4*shift4);
    bbGen4  = bbWrap4 & (bbGen4 << shift4);
    bbTemp  = bbGen4.s0|bbGen4.s1|bbGen4.s2|bbGen4.s3;
    // get koggestone wraps
    bbWrap4 = wraps4[1];
    // set generator and propagator (piece and empty squares)
    bbGen4  = (ulong4)bbTempO;
    bbPro4  = (ulong4)(~bbBlockers);
    // kogge stone shift right via ulong4 vector
    bbPro4 &= bbWrap4;
    bbGen4 |= bbPro4  & (bbGen4 >> shift4);
    bbPro4 &=           (bbPro4 >> shift4);
    bbGen4 |= bbPro4  & (bbGen4 >> 2*shift4);
    bbPro4 &=           (bbPro4 >> 2*shift4);
    bbGen4 |= bbPro4  & (bbGen4 >> 4*shift4);
    bbGen4  = bbWrap4 & (bbGen4 >> shift4);
    bbTemp |= bbGen4.s0|bbGen4.s1|bbGen4.s2|bbGen4.s3;
    // consider knights
    bbTemp  = ((piece>>1)==KNIGHT)?BBFULL:bbTemp;
    // verify captures
    i       = ((piece>>1)==PAWN)?(s32)som:(piece>>1);
    bbTempO  = AttackTables[i*64+pos];
    bbMoves = bbTempO&bbTemp&bbOpp;
    // verify non captures
    bbTempO  = ((piece>>1)==PAWN)?(AttackTablesPawnPushes[som*64+pos]):bbTempO;
    bbMoves |= (!qs)?(bbTempO&bbTemp&~bbBlockers):BBEMPTY; 

    while(bbMoves)
    {
      to = popfirst1(&bbMoves);

      // en passant to
      i = pos-to;
      ep = ( (piece>>1) == PAWN && abs(i) == 16 ) ? to : 0;

      cpt = to;
      pieceto = ((piece>>1)==PAWN&&GETRRANK(cpt,piece&0x1)==7)?(QUEEN<<1|(Piece)som):piece; // pawn promotion

      piececpt = GETPIECE(board,cpt);

      // make move
      move = MAKEMOVE((Move)pos, (Move)to, (Move)cpt, (Move)piece, (Move)pieceto, (Move)piececpt, (Move)ep, (u64)GETHMC(lastmove), (u64)score);
      // get move score
      score = EvalMove(move);
      score/=10; // in centi pawn please
      move = SETSCORE(move,(Move)score);
 
      // TODO: pseudo legal move gen: 2x speedup?
      // domove
      domovequick(board, move);

      kic = squareunderattack(board, !som, getkingpos(board, som));

      if (!kic)
      {
        // update castle rights        
//        move = updateCR(move, CR);
        // copy move to global
        global_pid_moves[pid*max_depth*MAXMOVES+sd*MAXMOVES+n[0]] = move;
        // movecounters
        n[0]++;
/*
        // sort moves, obsolete by move picker
        i = pid*max_depth*MAXMOVES+sd*MAXMOVES+0;
        for(j=n[0]-1;j>0;j--)
        {
          if (EvalMove(global_pid_moves[j+i])>EvalMove(global_pid_moves[j-1+i]))
          {
            tmpmove = global_pid_moves[j+i];
            global_pid_moves[j+i] = global_pid_moves[j-1+i];
            global_pid_moves[j-1+i] = tmpmove;
           }
           else
            break;
        }
*/
      }
      // undomove
      undomovequick(board, move);
    }
  }
}
Score evalpiece(Piece piece, Square sq)
{
  Score score = 0;

  // wood count
  score+= EvalPieceValues[GETPTYPE(piece)];
  // piece square tables
  sq = (piece&0x1)?sq:FLOP(sq);
  score+= EvalTable[GETPTYPE(piece)*64+sq];
  // sqaure control
  score+= EvalControl[sq];

  return score;
}
Score EvalMove(Move move)
{
  // pto (wood + pst) - pfrom (wood + pst)
  if ( (((move>>26)&0xF)>>1) == PNONE) 
    return evalpiece((Piece)((move>>22)&0xF), (Square)((move>>6)&0x3F))-evalpiece((Piece)((move>>18)&0xF), (Square)(move&0x3F));
  // MVV-LVA
  else
    return EvalPieceValues[(Piece)(((move>>26)&0xF)>>1)] * 16 - EvalPieceValues[(Piece)(((move>>22)&0xF)>>1)];
}
// evaluation taken from ZetaDva engine, est 2000 Elo.
Score eval(__private Bitboard *board)
{
  s32 j;
  Square pos;
  Piece piece;
  Bitboard bbBlockers = board[1]|board[2]|board[3];        // all pieces
  Bitboard bbTemp     = board[1]&~board[2]&~board[3];      // all pawns
  Bitboard bbWork;
  Bitboard bbMe;
  Bitboard bbOpp;
  Score score = 0;

  // white
  bbMe  = board[0]^bbBlockers;
  bbOpp = board[0];
  bbWork = bbMe;

  while (bbWork) 
  {
    pos = popfirst1(&bbWork);
    piece = GETPIECETYPE(board,pos);
    // piece bonus
    score+= 10;
    // wodd count
    score+= EvalPieceValues[piece];
    // piece posuare tables
    score+= EvalTable[piece*64+FLOP(pos)];
    // posuare control table
    score+= EvalControl[FLOP(pos)];
    // simple pawn structure white
    // blocked
    score-=(piece==PAWN&&GETRANK(pos)<RANK_8&&(bbOpp&SETMASKBB(pos+8)))?15:0;
    // chain
    score+=(piece==PAWN&&GETFILE(pos)<FILE_H&&(bbTemp&bbMe&SETMASKBB(pos-7)))?10:0;
    score+=(piece==PAWN&&GETFILE(pos)>FILE_A&&(bbTemp&bbMe&SETMASKBB(pos-9)))?10:0;
    // column
    for(j=pos-8;j>7&&piece==PAWN;j-=8)
      score-=(bbTemp&bbMe&SETMASKBB(j))?30:0;
  }
  // duble bishop
  score+= (count1s(bbMe&(~board[1]&~board[2]&board[3]))>=2)?25:0;

  // black
  bbMe  = board[0];
  bbOpp = board[0]^bbBlockers;
  bbWork = bbMe;

  while (bbWork) 
  {
    pos = popfirst1(&bbWork);
    piece = GETPIECETYPE(board,pos);
    // piece bonus
    score-= 10;
    // wodd count
    score-= EvalPieceValues[piece];
    // piece posuare tables
    score-= EvalTable[piece*64+pos];
    // posuare control table
    score-= EvalControl[pos];
    // simple pawn structure black
    // blocked
    score+=(piece==PAWN&&GETRANK(pos)>RANK_1&&(bbOpp&SETMASKBB(pos-8)))?15:0;
    // chain
    score-=(piece==PAWN&&GETFILE(pos)>FILE_A&&(bbTemp&bbMe&SETMASKBB(pos+7)))?10:0;
    score-=(piece==PAWN&&GETFILE(pos)<FILE_H&&(bbTemp&bbMe&SETMASKBB(pos+9)))?10:0;
    // column
    for(j=pos+8;j<56&&piece==PAWN;j+=8)
      score+=(bbTemp&bbMe&SETMASKBB(j))?30:0;
  }
  // duble bishop
  score-= (count1s(bbMe&(~board[1]&~board[2]&board[3]))>=2)?25:0;

  return score;
}
// bestfirst minimax search on gpu
/*
  steps:
  1. SELECT and lock node from stored tree in memory
  2. UPDATESCORE in node tree, excluding selected one
  3. EXPAND node in tree with generated child moves
  4. MOVEUP/MOVEDOWN, perform shallow alphabeta search for node evaluation
  5. BACKUPSCORE from alphabeta search to root in stored node tree
*/
__kernel void bestfirst_gpu(  
                            __global Bitboard *init_board,
                            __global NodeBlock *board_stack_1,
                            __global NodeBlock *board_stack_2,
                            __global NodeBlock *board_stack_3,
                            __global u64 *COUNTERS,
                            __global s32 *board_stack_top,
                            __global s32 *total_nodes_visited,
                            __global s32 *global_finished,
                            __global s32 *global_pid_movecounter,
                            __global s32 *global_pid_todoindex,
                            __global s32 *global_pid_ab_score,
                            __global s32 *global_pid_depths,
                            __global Move *global_pid_moves,
                            __global Move *global_pid_movehistory,
                            __global Hash *global_hashhistory,
                               const s32 som_init,
                               const s32 ply_init,
                               const s32 search_depth,
                               const s32 max_nodes_to_expand,
                               const s32 max_nodes_per_slot,
                               const u64 max_nodes,
                               const s32 max_depth
)
{
  __global NodeBlock *board_stack;
  __global NodeBlock *board_stack_tmp;

  __private Bitboard board[7]; // Quadbitboard + hash + lastmove

  const s32 pid = get_global_id(0) * get_global_size(1) * get_global_size(2) + get_global_id(1) * get_global_size(2) + get_global_id(2);

  bool som = (bool)som_init;
  bool rootkic = false;
  bool qs = false;
  Score score = 0;
  s32 mode = INIT;
  s32 index = 0;
  s32 current = 0;
  s32 child   = 0;
  s32 depth = search_depth;
  s32 sd = 0;
  s32 ply = 0;
  s32 n = 0;
  Move move = 0;
  Move lastmove;

  // assign root node to pid 0 for expand mode
  if (pid==0&&*board_stack_top==1)
  {
    // root node
    index = 0;
      // get init quadbitboard plus plus
    board[QBBBLACK] = init_board[QBBBLACK];
    board[QBBP1]    = init_board[QBBP1];
    board[QBBP2]    = init_board[QBBP2];
    board[QBBP3]    = init_board[QBBP3];
    board[QBBPMVD]  = init_board[QBBPMVD]; // piece moved flags
    board[QBBHASH]  = init_board[QBBHASH]; // hash
    board[QBBLAST]  = init_board[QBBLAST]; // lastmove
    som      = (bool)som_init;
    ply      = 0;
    sd       = 0;
    lastmove = board_stack_1[0].move;
    mode     = EXPAND;
  }

  // ################################
  // ####       main loop        ####
  // ################################
  while(    *board_stack_top<max_nodes_to_expand
         && *global_finished<max_nodes*8
         && *total_nodes_visited<max_nodes
      )
  {
    // iterations counter
    COUNTERS[pid*10+0]++;
    // single reply and mate hack
    if (board_stack_1[0].children==1||board_stack_1[0].children==0||ISMATE(board_stack_1[0].score))
      break;
    // ################################
    // ####          init          ####
    // ################################
    if (mode==INIT)
    {
      index = 0;
      board_stack_1[0].visits++;
      // get init quadbitboard plus plus
      board[QBBBLACK] = init_board[QBBBLACK];
      board[QBBP1]    = init_board[QBBP1];
      board[QBBP2]    = init_board[QBBP2];
      board[QBBP3]    = init_board[QBBP3];
      board[QBBPMVD]  = init_board[QBBPMVD]; // piece moved flags
      board[QBBHASH]  = init_board[QBBHASH]; // hash
      board[QBBLAST]  = init_board[QBBLAST]; // lastmove
      depth    = search_depth;
      som      = (bool)som_init;
      ply      = 0;
      sd       = 0;
      lastmove = board_stack_1[0].move;
      mode     = SELECT;
    }
    // ################################
    // ####         select         ####
    // ################################
    if (mode==SELECT)
    {
      float tmpscorea = 0;
      float tmpscoreb = 0;
      s32 k = -1;

      board_stack = (index >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
      n = board_stack[(index%max_nodes_per_slot)].children;
      // check bounds
      if (ply>=MAXBFPLY||ply+ply_init>=MAXGAMEPLY)
        n=0;
      tmpscorea = -1000000000;
      current = 0;
      // selecta best move from stored node tree
      for (s32 i=0;i<n;i++)
      {
        child = board_stack[(index%max_nodes_per_slot)].child + i;
        board_stack_tmp = (child>=max_nodes_per_slot*2)?board_stack_3:(child>=max_nodes_per_slot)?board_stack_2:board_stack_1;

        // if child node is locked or dead
        if (board_stack_tmp[(child%max_nodes_per_slot)].lock>-1||board_stack_tmp[(child%max_nodes_per_slot)].children==0)
          continue;
        // get node block score
        tmpscoreb = (float)-board_stack_tmp[(child%max_nodes_per_slot)].score;
        tmpscoreb/=10; // back to centipawn based scores
        // skip draw score
        if (SKIPDRAW&&tmpscoreb==DRAWSCORE)
            continue;
        // skip check mate
        if (SKIPMATE&&index>0&&ISMATE(tmpscoreb))
            continue;
        // on root node deliver work via visit counter
        if (ROOTSEARCH&&index==0)
        {
          tmpscoreb = (float)-board_stack_tmp[(child%max_nodes_per_slot)].visits;
          tmpscoreb+= (((float)board_stack[(index%max_nodes_per_slot)].visits) / (SMOOTHUCT*(float)board_stack_tmp[(child%max_nodes_per_slot)].visits+1));
        }
        // most threads go to breadth, some go to depth
        else if (BROADWELL&&(pid%DEPTHWELL>0))
        {
          // parallel best first select formula
          tmpscoreb*= SCOREWEIGHT;
          tmpscoreb+= (((float)board_stack[(index%max_nodes_per_slot)].visits) / (SMOOTHUCT*(float)board_stack_tmp[(child%max_nodes_per_slot)].visits+1));
        }
        if (tmpscoreb>tmpscorea)
        {
          tmpscorea = tmpscoreb;
          current = child;
        }
      }
      board_stack_tmp = (current>=max_nodes_per_slot*2)?board_stack_3:(current>=max_nodes_per_slot)?board_stack_2:board_stack_1;
      // check if selected node is locked or dead
      if (current>0&&(board_stack_tmp[(current%max_nodes_per_slot)].lock>-1||board_stack_tmp[(current%max_nodes_per_slot)].children==0))
        current = 0;
      // get lock
      if (current>0&&board_stack_tmp[(current%max_nodes_per_slot)].children==-1)
      {
        atom_cmpxchg(&board_stack_tmp[(current%max_nodes_per_slot)].lock, -1, pid);
        k = atom_cmpxchg(&board_stack_tmp[(current%max_nodes_per_slot)].lock, pid, pid);
      }
      // if otherwise locked, repeat init run
      if (k != -1 && k != pid)
          current = 0;
      // got lock, do move and exit select phase to expand phase
      if (k==pid)
      {
        mode = UPDATESCORE; // update score tree first, then expand node
      }
      // nothing goes
      if (current==0)
        mode = INIT;
      // got a child, do move
      if (current > 0)
      {
        board_stack_tmp = (current>=max_nodes_per_slot*2)?board_stack_3:(current>=max_nodes_per_slot)?board_stack_2:board_stack_1;
        board_stack_tmp[(current%max_nodes_per_slot)].visits++;
        move = board_stack_tmp[(current%max_nodes_per_slot)].move;
        lastmove = move;
        ply++;
        // remember bf depth for xboard output
        if (ply>COUNTERS[5])
          COUNTERS[5] = ply;
        domovequick(board, move);
        global_hashhistory[pid*MAXGAMEPLY+ply+ply_init] = board[QBBHASH];
        som = !som;
        board[QBBHASH] = computehash(board, som);
//        updateHash(board, move);
//        board[QBBHASH] = computeHash(board, som);
        index = current;
      }
    }
    if (mode==INIT||mode==SELECT)
    {
      atom_inc(global_finished);
      continue;
    }
    // termination counter 
    atom_add(global_finished, 8);
    // ################################
    // ####       updatescore      ####
    // ################################
    // update score tree without selected node
    if (mode==UPDATESCORE)
    {
      Score tmpscore = 0;
      s32 parent  = 0;
      s32 j;

      mode = EXPAND;
      board_stack = (current>=max_nodes_per_slot*2)?board_stack_3:(current>=max_nodes_per_slot)?board_stack_2:board_stack_1;
      parent = board_stack[(current%max_nodes_per_slot)].parent;
      // all nodead back to root
      while(parent>=0)
      {
        score = -INF;
        board_stack = (parent>=max_nodes_per_slot*2)?board_stack_3:(parent>=max_nodes_per_slot)?board_stack_2:board_stack_1;
        j = board_stack[(parent%max_nodes_per_slot)].children;
        // each child from node
        for(s32 i=0;i<j;i++)
        {
          child = board_stack[(parent%max_nodes_per_slot)].child + i;
          // skip selected node from scoring
          if (child==current)
            continue;
          board_stack_tmp = (child>=max_nodes_per_slot*2)?board_stack_3:(child>=max_nodes_per_slot)?board_stack_2:board_stack_1;
          tmpscore = -board_stack_tmp[(child%max_nodes_per_slot)].score;
          // skip unexplored 
          if (ISINF(tmpscore))
          {
            score = -INF;
            break;
          }
          if (tmpscore > score)
            score = tmpscore;
        }
        if (!ISINF(score))
          tmpscore = atom_xchg(&board_stack[(parent%max_nodes_per_slot)].score, score);
        else
          break;
        parent = board_stack[(parent%max_nodes_per_slot)].parent;
      }
    }
    // ################################
    // ####     nove generator     ####
    // ################################
    n = 0;
    // king in check?
    rootkic = squareunderattack(board, !som, getkingpos(board, som));
    // enter quiescence search?
    qs = (sd<=depth)?false:true;
    qs = (mode==EXPAND||mode==EVALLEAF)?false:qs;
    qs = (rootkic&&sd<=search_depth+MAXEVASIONS)?false:qs;
//    qs = (rootkic)?false:qs;
    // generate moves
    gen_moves(board, &n, som, qs, sd, pid, max_depth, global_pid_moves, rootkic);
    // ################################
    // ####        evaluation       ###
    // ################################
    score = eval(board);
    // centi pawn *10
    score*=10;
    // hack for drawscore == 0, site to move bonus
    score+=(!som)?1:-1;
    // negamaxed scores
    score = (som)?-score:score;
    // checkmate
    score = (!qs&&rootkic&&n==0)?-INF+ply+ply_init:score;
    // stalemate
    score = (!qs&&!rootkic&&n==0)?STALEMATESCORE:score;
    // draw by 3 fold repetition
    for (s32 i=ply+ply_init-2;i>0&&mode==EXPAND&&index>0;i-=2)
    {
      if (board[QBBHASH]==global_hashhistory[pid*MAXGAMEPLY+i])
      {
        n       = 0;
        score   = DRAWSCORE;
        break;
      }
    }
    // #################################
    // ####     alphabeta stuff      ###
    // #################################
    // out of range
    if (sd>=max_depth)
      n = 0;
    global_pid_movecounter[pid*max_depth+sd] = n;
    // terminal node
    if (mode==MOVEUP&&n==0)
    {
      global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] = score;
      mode = MOVEDOWN;
    }
    // stand pat in qsearch
    // return beta
    if (mode==MOVEUP&&qs&&!rootkic&&score>=global_pid_ab_score[pid*max_depth*2+(sd)*2+BETA])
    {
//      global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] = global_pid_ab_score[pid*max_depth*2+(sd)*2+BETA]; // fail hard
      global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] = score; // fail soft
			mode = MOVEDOWN;
    }
    // stand pat in qsearch
    // set alpha
    if (mode==MOVEUP&&qs&&!rootkic)
    {
      atom_max(&global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA], score);
		}
    // ################################
    // ####         expand         ####
    // ################################
    if (mode==EXPAND)
    {
      s32 parent  = 0;

      // expand node counter
      COUNTERS[pid*10+1]++;
      // create child nodes
      current = atom_add(board_stack_top,n);
      // check bounds
      if (n>0&&(current+n>=max_nodes_to_expand||ply>=MAXBFPLY))
        n = -1;
      board_stack = (index>=max_nodes_per_slot*2)?board_stack_3:(index>=max_nodes_per_slot)?board_stack_2:board_stack_1;
      // each child from node
      for(s32 i=0;i<n;i++)
      {
        move = global_pid_moves[pid*max_depth*MAXMOVES+sd*MAXMOVES+i];
        parent = (current+i);      
        board_stack_tmp = (parent>=max_nodes_per_slot*2)?board_stack_3:(parent>=max_nodes_per_slot)?board_stack_2:board_stack_1;
        board_stack_tmp[(parent%max_nodes_per_slot)].move       = move;
        board_stack_tmp[(parent%max_nodes_per_slot)].score      = -INF;
        board_stack_tmp[(parent%max_nodes_per_slot)].visits     =  0;
        board_stack_tmp[(parent%max_nodes_per_slot)].children   = -1;
        board_stack_tmp[(parent%max_nodes_per_slot)].parent     = index;
        board_stack_tmp[(parent%max_nodes_per_slot)].child      = -1;
        board_stack_tmp[(parent%max_nodes_per_slot)].lock       = -1;
      }
      board_stack[(index%max_nodes_per_slot)].child = current;
      board_stack[(index%max_nodes_per_slot)].children = n;
      mode = EVALLEAF;
      // terminal expand node, set score and backup
      if (n==0)
      {
        board_stack[(index%max_nodes_per_slot)].score = score;
        board_stack[(index%max_nodes_per_slot)].children = n;
        current = index;
        mode = BACKUPSCORE;
      }
      // something went wrong, release lock
      if (n<0)
      {
        // release lock
        board_stack[(index%max_nodes_per_slot)].score = -INF;
        board_stack[(index%max_nodes_per_slot)].lock = -1;
        mode = INIT;
      }
    }
    // ################################
    // ####        evalleaf        ####
    // ################################
    // init values for alphabeta search
    if (mode==EVALLEAF)
    {
      sd = 0;
      // search extensions
      depth = search_depth;
      depth = (INCHECKEXT&&rootkic)?search_depth+1:search_depth;
      depth = (SINGLEEXT&&n==1)?search_depth+1:depth;
      depth = (PROMOEXT&&(((lastmove>>18)&0xF)>>1)==PAWN&&(GETRRANK(((lastmove>>6)&0x3F),(((lastmove>>18)&0xF)&0x1))>=RANK_7))?search_depth+1:depth;
      // set move todo index
      global_pid_todoindex[pid*max_depth+sd] = 0;
      // set init Alpha Beta values
      global_pid_ab_score[pid*max_depth*2+0*2+ALPHA] = -INF;
      global_pid_ab_score[pid*max_depth*2+0*2+BETA]  =  INF;
      // set current search depth
      global_pid_depths[pid*max_depth+sd] = depth;
      mode = MOVEUP;
    }
    // ################################
    // ####        movedown        ####
    // ################################
    // move down in alphabeta search
    if (mode==MOVEDOWN)
    {
      // incl. alphabeta pruning
      while (    global_pid_todoindex[pid*max_depth+sd]>=global_pid_movecounter[pid*max_depth+sd]
             || (global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA]>=global_pid_ab_score[pid*max_depth*2+(sd)*2+BETA])
            ) 
      {
        // do alphabeta negamax scoring
        score = global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA];
        if (abs(score)!=INF&&sd>0)
          atom_max(&global_pid_ab_score[pid*max_depth*2+(sd-1)*2+ALPHA],-score);
        sd--;
        ply--;
        // this is the end
        if (sd < 0)
            break;
        depth = global_pid_depths[pid*max_depth+sd];
        move = global_pid_movehistory[pid*max_depth+sd];
        undomovequick(board, move);
        // switch site to move
        som = !som;
//        updateHash(board, move);
//        board[QBBHASH] = computeHash(board, som);
        // store score to child from expanded node
        if (sd==0)
        {
          board_stack_tmp = (child>=max_nodes_per_slot*2)?board_stack_3:(child>=max_nodes_per_slot)?board_stack_2:board_stack_1;
          board_stack_tmp[(child%max_nodes_per_slot)].score =  global_pid_ab_score[pid*max_depth*2+1*2+ALPHA];
        }
      }
      mode = MOVEUP;
      // on root, set score of current node block and backup score
      if (sd < 0)
      {
        board_stack = (index>=max_nodes_per_slot*2)?board_stack_3:(index>=max_nodes_per_slot)?board_stack_2:board_stack_1;
//        board_stack[(index%max_nodes_per_slot)].score =  global_pid_ab_score[pid*max_depth*2+0*2+ALPHA];
        current = board_stack[(index%max_nodes_per_slot)].child;
        mode = BACKUPSCORE;
			}
    }
    // ################################
    // ####         moveup         ####
    // ################################
    // move up in alphabeta search
    if (mode==MOVEUP)
    {
      Move tmpmove = 0;
      Score tmpscore = 0;
      s32 i = 0;

      // alphabeta search node counter
      COUNTERS[pid*10+2]++;
      atom_inc(total_nodes_visited);
      /* movepicker */
      move = MOVENONE;
      n = global_pid_movecounter[pid*max_depth+sd];
      current = pid*max_depth*MAXMOVES+sd*MAXMOVES+0;
      score = -INF;
      tmpscore = 0;
      // iterate and eval moves
      for(s32 j=0;j<n;j++)
      {
        tmpmove = global_pid_moves[j+current];
        if (tmpmove==MOVENONE)
          continue;
        tmpscore = EvalMove(tmpmove);
        if (tmpscore>score)
        {
          score = tmpscore;
          move = tmpmove;
          i = j;
        }
      }
      // remember child node index
      if (sd==0)
      {
          board_stack = (index>=max_nodes_per_slot*2)?board_stack_3:(index>=max_nodes_per_slot)?board_stack_2:board_stack_1;
          child = board_stack[(index%max_nodes_per_slot)].child + i;
      }
      global_pid_moves[i+current] = MOVENONE; // reset move
//      move = global_pid_moves[pid*max_depth*MAXMOVES+sd*MAXMOVES+global_pid_todoindex[pid*max_depth+sd]];
      domovequick(board, move);
      lastmove = move;
      global_pid_movehistory[pid*max_depth+sd]=lastmove;
      // switch site to move
      som = !som;
//      updateHash(board, move);
//      board[QBBHASH] = computeHash(board, som);
      global_pid_todoindex[pid*max_depth+sd]++;
      sd++;
      ply++;
      global_hashhistory[pid*MAXGAMEPLY+ply+ply_init] = board[QBBHASH];
      // set values for next depth
      global_pid_movecounter[pid*max_depth+sd] = 0;
      global_pid_todoindex[pid*max_depth+sd] = 0;
      global_pid_depths[pid*max_depth+sd] = depth;
      // set alpha beta values for next depth
      global_pid_ab_score[pid*max_depth*2+sd*2+ALPHA] = (sd==0)?-INF:-global_pid_ab_score[pid*max_depth*2+(sd-1)*2+BETA];
      global_pid_ab_score[pid*max_depth*2+sd*2+BETA]  = (sd==0)?+INF:-global_pid_ab_score[pid*max_depth*2+(sd-1)*2+ALPHA];
      continue;
    }
    // ################################
    // ####      backupscore       ####
    // ################################
    // backup score from alphabeta search in bf node tree
    if (mode==BACKUPSCORE)
    {
      Score tmpscore = 0;
      s32 j;
      s32 parent;

      mode = INIT;
      board_stack = (current>=max_nodes_per_slot*2)?board_stack_3:(current>=max_nodes_per_slot)?board_stack_2:board_stack_1;
      parent = board_stack[(current%max_nodes_per_slot)].parent;

      // backup score
      while(parent>=0)
      {
        score = -INF;
        board_stack = (parent>=max_nodes_per_slot*2)?board_stack_3:(parent>=max_nodes_per_slot)?board_stack_2:board_stack_1;
        j = board_stack[(parent%max_nodes_per_slot)].children;

        for(s32 i=0;i<j;i++)
        {
          child = board_stack[(parent%max_nodes_per_slot)].child + i;
          board_stack_tmp = (child>=max_nodes_per_slot*2)?board_stack_3:(child>=max_nodes_per_slot)?board_stack_2:board_stack_1;
          tmpscore = -board_stack_tmp[(child%max_nodes_per_slot)].score;

          // skip not fully explored nodes
          if (ISINF(tmpscore))
          {
            score = -INF;
            break;
          }
          if (tmpscore > score)
            score = tmpscore;
        }
        if (!ISINF(score))
        {
          tmpscore = atom_xchg(&board_stack[(parent%max_nodes_per_slot)].score, score);
          // store ply of last score for xboard output
          if (parent==0&&score>tmpscore)
              COUNTERS[7] = (u64)ply+1;
        }
        else
          break;
        parent = board_stack[(parent%max_nodes_per_slot)].parent;
      }
      // release lock
      board_stack = (index>=max_nodes_per_slot*2)?board_stack_3:(index>=max_nodes_per_slot)?board_stack_2:board_stack_1;
      board_stack[(index%max_nodes_per_slot)].lock = -1;
    }
  } // end main loop
  COUNTERS[6] = (*board_stack_top>=max_nodes_to_expand)?1:0; // memory full flag
}

