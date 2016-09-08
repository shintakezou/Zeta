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

#define WHITE 0
#define BLACK 1

#define true          1
#define false         0

#define MAXLEGALMOVES 220

#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define FLIPFLOP(square)    (((square)^7)^56)
#define FLOP(square)         ((square)^56)
#define FLIP(square)         ((square)^7)

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

#define node_size   8

#define ILL     64


#define INF 32000
#define MATESCORE 29000

#endif
