/* 
    Zeta Dva, yet another amateur level chess engine
    Author: Srdja Matovic <srdja@matovic.de>
    Created at: 15-Jan-2011
    Updated at: 31-Jan-2011
    Description: Amateur chesss programm

    Copyright (C) 2011 Srdja Matovic
    This program is distributed under the GNU General Public License.
    See file COPYING or http://www.gnu.org/licenses/

*/

#if !defined(TYPES_H_INCLUDED)
#define TYPES_H_INCLUDED

/* C99 headers */
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "CL/cl.h"


typedef cl_ulong U64;
typedef cl_uint U32;

typedef cl_int S32;

typedef U64 Move;
typedef U64 Bitboard;
typedef U32 Cr;
typedef U64 Hash;

typedef S32 Score;
typedef S32 MoveScore;
typedef U32 Square;
typedef U32 Piece;


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
#define MAXPLY      128     // max internal search ply
#define MAXGAMEPLY  1024    // max ply a game can reach
#define MAXMOVES    256     // max amount of legal moves per position
#define TIMESPARE   100     // 100 milliseconds spare
#define MAXLEGALMOVES 220

#define WHITE 0
#define BLACK 1

#define true          1
#define false         0


#define MAX(a,b)            ((a)>(b)?(a):(b))
#define MIN(a,b)            ((a)<(b)?(a):(b))

#define SwitchSide(som)     ((som == WHITE)? BLACK : WHITE)

#define PEMPTY  0
#define PAWN    1
#define KNIGHT  2
#define KING    3
#define BISHOP  4
#define ROOK    5
#define QUEEN   6

// Node structure
#define MOVE        0
#define INFO        1
#define SCORE       2
#define VISITS      3
#define CHILDREN    4
#define PARENT      5
#define CHILD       6
#define LOCK        7
//#define LOCKS       8      
//#define EXPANDABLE  9
//#define node_size   10

#define ILL     64

#define INF             1048576
#define MATESCORE       1040000
#define DRAWSCORE       0
#define STALEMATESCORE  0

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
                           |  (((board[1]>>(sq))&0x1)) \
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

#endif
