/*
    Zeta CL, OpenCL Chess Engine
    Author: Srdja Matovic <srdja.matovic@googlemail.com>
    Created at: 20-Jan-2011
    Updated at:
    Description: A Chess Engine written in OpenCL, a language suited for GPUs.

    Copyright (C) 2011 Srdja Matovic
    This program is distributed under the GNU General Public License.
    See file COPYING or http://www.gnu.org/licenses/
*/

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics       : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics   : enable
//#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics        : enable
//#pragma OPENCL EXTENSION cl_khr_byte_addressable_store          : enable

typedef unsigned long   u64;
typedef unsigned int    u32;
typedef signed int      s32;

typedef s32             Score;
typedef u32             Square;
typedef u32             Piece;

typedef u64 Cr;
typedef u64 Move;
typedef u64 Bitboard;
typedef u64 Hash;

typedef struct {
    Move move;
    Score score;
    s32 lock;
    s32 visits;
    s32 child;
    s32 children;
    s32 parent;
}  NodeBlock;


#define WHITE       0
#define BLACK       1

#define ALPHA       0
#define BETA        1

// modes
#define INIT        0
#define SELECTION   1
#define EXPAND      2
#define EVALLEAF    3
#define MOVEUP      4
#define MOVEDOWN    5
#define BACKUPSCORE 6

#define INF             1048576
#define MATESCORE       1040000
#define DRAWSCORE       0
#define STALEMATESCORE  0

#define MAXLEGALMOVES 220

#define PEMPTY  0
#define PAWN    1
#define KNIGHT  2
#define KING    3
#define BISHOP  4
#define ROOK    5
#define QUEEN   6

#define ILL     64

/* u64 defaults */
#define BBEMPTY             0x0000000000000000
#define BBFULL              0xFFFFFFFFFFFFFFFF
#define MOVENONE            0x0000000000000000
#define HASHNONE            0x0000000000000000
#define CRNONE              0x0000000000000000
#define SCORENONE           0x0000000000000000


// bitboard masks, computation prefered over lookup
#define SETMASKBB(sq)       (((u64)1)<<(sq))
#define CLRMASKBB(sq)       (~(((u64)1)<<(sq)))
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
#define BBRANK7             0x00FF000000000000
#define BBRANK5             0x000000FF00000000
#define BBRANK4             0x00000000FF000000
#define BBRANK2             0x000000000000FF00
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
/* is score a mate in n */
#define ISMATE(val)           ((((val)>MATESCORE&&(val)<INF)||((val)<-MATESCORE&&(val)>-INF))?true:false)
/* is score default inf */
#define ISINF(val)            (((val)==INF||(val)==-INF)?true:false)

// tuneable search parameter
#define MAXEVASIONS          3             // in check evasions
#define TERMINATESOFT        1            // 0 or 1, will finish all searches before exit
#define SMOOTHUCT            0.28        // factor for uct params in select formula
#define SKIPMATE             1          // 0 or 1
#define SKIPDRAW             1         // 0 or 1
#define ROOTSEARCH           1        // 0 or 1, distribute root nodes equaly in select phase
#define SCOREWEIGHT          0.11    // factor for board score in select formula
#define ZETAPRUNING          1      // 0 or 1

/* 
  piece square tables based on proposal by Tomasz Mich
ni
ewski
  https://chessprogramming.wikispaces.com/Simplified+evaluation+function
*/

/* piece values */
/* pnone, pawn, knight, king, bishop, rook, queen */
/* const Score EvalPieceValues[7] = {0, 100, 300, 0, 300, 500, 900}; */
__constant Score EvalPieceValues[7] = {0, 100, 400, 0, 400, 600, 1200};
/* square control bonus, black view */
/* flop square for white-index: sq^56*/
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
/* piece square tables, black view */
/* flop square for white-index: sq^56*/
__constant Score EvalTable[7*64] =
{
    /* piece none  */
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,

    /* pawn */
    0,  0,  0,  0,  0,  0,  0,  0,
   50, 50, 50, 50, 50, 50, 50, 50,
   30, 30, 30, 30, 30, 30, 30, 30,
    5,  5,  5, 10, 10,  5,  5,  5,
    3,  3,  3,  8,  8,  3,  3,  3,
    2,  2,  2,  2,  2,  2,  2,  2,
    0,  0,  0, -5, -5,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,

     /* knight */
  -50,-40,-30,-30,-30,-30,-40,-50,
  -40,-20,  0,  0,  0,  0,-20,-40,
  -30,  0, 10, 15, 15, 10,  0,-30,
  -30,  5, 15, 20, 20, 15,  5,-30,
  -30,  0, 15, 20, 20, 15,  0,-30,
  -30,  5, 10, 15, 15, 10,  5,-30,
  -40,-20, 0,   5,  5,  0,-20,-40,
  -50,-40,-30,-30,-30,-30,-40,-50,

    /* king */
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,

    /* bishop */
  -20,-10,-10,-10,-10,-10,-10,-20,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5, 10, 10,  5,  0,-10,
  -10,  5,  5, 10, 10,  5,  5,-10,
  -10,  0, 10, 10, 10, 10,  0,-10,
  -10, 10, 10, 10, 10, 10, 10,-10,
  -10,  5,  0,  0,  0,  0,  5,-10,
  -20,-10,-10,-10,-10,-10,-10,-20,

    /* rook */
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
   -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0,

    /* queen */
  -20,-10,-10, -5, -5,-10,-10,-20,
  -10,  0,  0,  0,  0,  0,  0,-10,
  -10,  0,  5,  5,  5,  5,  0,-10,
   -5,  0,  5,  5,  5,  5,  0, -5,
   -5,  0,  5,  5,  5 , 5 , 0, -5,
  -10,  0,  5,  5,  5,  5,  0,-10,
  -10,  0,  0,  0,  0 , 0 , 0,-10,
  -20,-10,-10, -5, -5,-10,-10,-20
};

Score EvalMove(Move move)
{
/*
    // castle moves
    if ( (((move>>54)&0xF)>>1) == ROOK)
        return INF; 
*/
  // MVA
  if ( (((move>>26)&0xF)>>1) == PEMPTY) 
    return EvalPieceValues[(((move>>22)&0xF)>>1)];
  // MVV-LVA
  else
    return EvalPieceValues[(((move>>26)&0xF)>>1)] * 16 - EvalPieceValues[(((move>>22)&0xF)>>1)];
}

// OpenCL 1.2 has popcount function
#if __OPENCL_VERSION__ < 120
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
#endif
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
/* bit twiddling
  bb_work=bb_temp&-bb_temp;  // get lsb 
  bb_temp&=bb_temp-1;       // clear lsb
*/


void domove(__private Bitboard *board, Move move) {

    Square from = (Square)(move & 0x3F);
    Square to   = (Square)((move>>6) & 0x3F);
    Square cpt  = (Square)((move>>12) & 0x3F);

    Bitboard pto   = ((move>>22) & 0xF);

    Bitboard bbTemp = BBEMPTY;

    // castle info
    u32 castlefrom   = (u32)((move>>40) & 0x7F); // is set to illegal square 64 when empty
    u32 castleto     = (u32)((move>>47) & 0x7F); // is set to illegal square 64 when empty
    Bitboard castlepciece = ((move>>54) & 0xF);  // is set to 0 when PEMPTY

    // unset from capture to and castle from
    bbTemp = CLRMASKBB(from) & CLRMASKBB(cpt) & CLRMASKBB(to);
    bbTemp &= (castlefrom==ILL)?BBFULL:CLRMASKBB(castlefrom);

    board[0] &= bbTemp;
    board[1] &= bbTemp;
    board[2] &= bbTemp;
    board[3] &= bbTemp;

    // set to
    board[0] |= (pto&1)<<to;
    board[1] |= ((pto>>1)&1)<<to;
    board[2] |= ((pto>>2)&1)<<to;
    board[3] |= ((pto>>3)&1)<<to;

    // set to castle rook
    if (castleto < 64 && (castlepciece>>1) == ROOK) {
        board[0] |= (castlepciece&1)<<castleto;
        board[1] |= ((castlepciece>>1)&1)<<castleto;
        board[2] |= ((castlepciece>>2)&1)<<castleto;
        board[3] |= ((castlepciece>>3)&1)<<castleto;
    }

}

void undomove(__private Bitboard *board, Move move) {

    Square from = (Square)(move & 0x3F);
    Square to   = (Square)((move>>6) & 0x3F);
    Square cpt  = (Square)((move>>12) & 0x3F);

    Bitboard pfrom = ((move>>18) & 0xF);
    Bitboard pcpt  = ((move>>26) & 0xF);

    Bitboard bbTemp = BBEMPTY;

    // castle info
    u32 castlefrom   = (u32)((move>>40) & 0x7F); // is set to illegal square 64 when empty
    u32 castleto     = (u32)((move>>47) & 0x7F); // is set to illegal square 64 when empty
    Bitboard castlepciece = ((move>>54) & 0xF);  // is set to 0 when PEMPTY

    // unset capture, to and castle to
    bbTemp = CLRMASKBB(cpt) & CLRMASKBB(to);
    bbTemp &= (castleto==ILL)?BBFULL:CLRMASKBB(castleto);

    board[0] &= bbTemp;
    board[1] &= bbTemp;
    board[2] &= bbTemp;
    board[3] &= bbTemp;

    // restore cpt
    board[0] |= (pcpt&1)<<cpt;
    board[1] |= ((pcpt>>1)&1)<<cpt;
    board[2] |= ((pcpt>>2)&1)<<cpt;
    board[3] |= ((pcpt>>3)&1)<<cpt;

    // restore from
    board[0] |= (pfrom&1)<<from;
    board[1] |= ((pfrom>>1)&1)<<from;
    board[2] |= ((pfrom>>2)&1)<<from;
    board[3] |= ((pfrom>>3)&1)<<from;

    // restore from castle rook
    if (castlefrom < 64 && (castlepciece>>1) == ROOK) {
        board[0] |= (castlepciece&1)<<castlefrom;
        board[1] |= ((castlepciece>>1)&1)<<castlefrom;
        board[2] |= ((castlepciece>>2)&1)<<castlefrom;
        board[3] |= ((castlepciece>>3)&1)<<castlefrom;
    }
}


/* ############################# */
/* ###         Hash          ### */
/* ############################# */

Hash computeHash(__private Bitboard *board, bool stm, __global u64 *Zobrist) {

    Piece piece;
    int side;
    Square pos;
    Bitboard bbBoth[2];
    Bitboard bbWork = 0;
    Hash hash = 0;

    bbBoth[0]   = ( board[0] ^ (board[1] | board[2] | board[3]));
    bbBoth[1]   =   board[0];

    // for each side
    for(side=0; side<2;side++) {
        bbWork = bbBoth[side];

        // each piece
        while (bbWork) {
            // pop 1st bit
            pos     = popfirst1(&bbWork);
            piece = GETPIECETYPE(board, pos);
            hash ^= Zobrist[side*7*64+piece*64+pos];

        }
    }
    if (!stm)
      hash^=Zobrist[844];

    return hash;    
}

void updateHash(__private Bitboard *board, Move move, __global u64 *Zobrist) {

    // from
    board[4] ^= Zobrist[(((move>>18) & 0xF)&1)*7*64+(((move>>18) & 0xF)>>1)*64+(move & 0x3F)];

    // to
    board[4] ^= Zobrist[(((move>>22) & 0xF)&1)*7*64+(((move>>22) & 0xF)>>1)*64+((move>>6) & 0x3F)];

    // capture
    if ( (((move>>26) & 0xF)>>1) != PEMPTY)
        board[4] ^= Zobrist[(((move>>26) & 0xF)&1)*7*64+(((move>>26) & 0xF)>>1)*64+((move>>12) & 0x3F)];

    // castle from
    if (((move>>40) & 0x7F) < ILL && (((move>>54) & 0xF)>>1) == ROOK )
        board[4] ^= Zobrist[(((move>>54) & 0xF)&1)*7*64+(((move>>54) & 0xF)>>1)*64+((move>>40) & 0x3F)];

    // castle to
    if (((move>>47) & 0x7F) < ILL && (((move>>54) & 0xF)>>1) == ROOK )
        board[4] ^= Zobrist[(((move>>54) & 0xF)&1)*7*64+(((move>>54) & 0xF)>>1)*64+((move>>47) & 0x3F)];
}

Move updateCR(Move move, Cr cr) {

    Square from   =  (Square)(move&0x3F);
    Piece piece   =  (Piece)(((move>>18)&0xF)>>1);
    bool som      =  (bool)((move>>18)&0xF)&1;

    // update castle rights, TODO: make nice
    // clear white queenside
    cr&= ( (piece == ROOK && som == WHITE && from == 0) || ( piece == KING && som == WHITE && from == 4)) ? 0xE : 0xF;
    // clear white kingside
    cr&= ( (piece == ROOK && som == WHITE && from == 7) || ( piece == KING && som == WHITE && from == 4)) ? 0xD : 0xF;
    // clear black queenside
    cr&= ( (piece == ROOK && som == BLACK && from == 56) || ( piece == KING && som == BLACK && from == 60)) ? 0xB : 0xF;
    // clear black kingside
    cr&= ( (piece == ROOK && som == BLACK && from == 63) || ( piece == KING && som == BLACK && from == 60)) ? 0x7 : 0xF;


    move &= 0xFFFFFF0FFFFFFFFF; // clear old cr
    move |= ((Move)cr<<36)&0x000000F000000000; // set new cr

    return move;
}
// move generator costants
// wraps for kogge stone shifts
__constant ulong4 shift4 = (ulong4)( 9, 1, 7, 8);

__constant ulong4 wraps4[2] =
{
  // wraps shift left
  (ulong4)(
            0xfefefefefefefe00, // <<9
            0xfefefefefefefefe, // <<1 */
            0x7f7f7f7f7f7f7f00, // <<7 */
            0xffffffffffffff00  // <<8 */
          ),

  // wraps shift right
  (ulong4)(
            0x007f7f7f7f7f7f7f, // >>9
            0x7f7f7f7f7f7f7f7f, // >>1
            0x00fefefefefefefe, // >>7
            0x00ffffffffffffff  // >>8
          )
};
__constant Bitboard AttackTablesPawnPushes[2*64] = 
{
  /* white pawn pushes */
  0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x1010000,0x2020000,0x4040000,0x8080000,0x10100000,0x20200000,0x40400000,0x80800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10000000000,0x20000000000,0x40000000000,0x80000000000,0x100000000000,0x200000000000,0x400000000000,0x800000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000,0x100000000000000,0x200000000000000,0x400000000000000,0x800000000000000,0x1000000000000000,0x2000000000000000,0x4000000000000000,0x8000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  /* black pawn pushes */
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10100000000,0x20200000000,0x40400000000,0x80800000000,0x101000000000,0x202000000000,0x404000000000,0x808000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000
};
/* piece attack tables */
__constant Bitboard AttackTables[7*64] = 
{
  /* white pawn */
  0x200,0x500,0xa00,0x1400,0x2800,0x5000,0xa000,0x4000,0x20000,0x50000,0xa0000,0x140000,0x280000,0x500000,0xa00000,0x400000,0x2000000,0x5000000,0xa000000,0x14000000,0x28000000,0x50000000,0xa0000000,0x40000000,0x200000000,0x500000000,0xa00000000,0x1400000000,0x2800000000,0x5000000000,0xa000000000,0x4000000000,0x20000000000,0x50000000000,0xa0000000000,0x140000000000,0x280000000000,0x500000000000,0xa00000000000,0x400000000000,0x2000000000000,0x5000000000000,0xa000000000000,0x14000000000000,0x28000000000000,0x50000000000000,0xa0000000000000,0x40000000000000,0x200000000000000,0x500000000000000,0xa00000000000000,0x1400000000000000,0x2800000000000000,0x5000000000000000,0xa000000000000000,0x4000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  /* black pawn */
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x5,0xa,0x14,0x28,0x50,0xa0,0x40,0x200,0x500,0xa00,0x1400,0x2800,0x5000,0xa000,0x4000,0x20000,0x50000,0xa0000,0x140000,0x280000,0x500000,0xa00000,0x400000,0x2000000,0x5000000,0xa000000,0x14000000,0x28000000,0x50000000,0xa0000000,0x40000000,0x200000000,0x500000000,0xa00000000,0x1400000000,0x2800000000,0x5000000000,0xa000000000,0x4000000000,0x20000000000,0x50000000000,0xa0000000000,0x140000000000,0x280000000000,0x500000000000,0xa00000000000,0x400000000000,0x2000000000000,0x5000000000000,0xa000000000000,0x14000000000000,0x28000000000000,0x50000000000000,0xa0000000000000,0x40000000000000,
  /* knight */
  0x20400,0x50800,0xa1100,0x142200,0x284400,0x508800,0xa01000,0x402000,0x2040004,0x5080008,0xa110011,0x14220022,0x28440044,0x50880088,0xa0100010,0x40200020,0x204000402,0x508000805,0xa1100110a,0x1422002214,0x2844004428,0x5088008850,0xa0100010a0,0x4020002040,0x20400040200,0x50800080500,0xa1100110a00,0x142200221400,0x284400442800,0x508800885000,0xa0100010a000,0x402000204000,0x2040004020000,0x5080008050000,0xa1100110a0000,0x14220022140000,0x28440044280000,0x50880088500000,0xa0100010a00000,0x40200020400000,0x204000402000000,0x508000805000000,0xa1100110a000000,0x1422002214000000,0x2844004428000000,0x5088008850000000,0xa0100010a0000000,0x4020002040000000,0x400040200000000,0x800080500000000,0x1100110a00000000,0x2200221400000000,0x4400442800000000,0x8800885000000000,0x100010a000000000,0x2000204000000000,0x4020000000000,0x8050000000000,0x110a0000000000,0x22140000000000,0x44280000000000,0x88500000000000,0x10a00000000000,0x20400000000000,
  /* king */
  0x302,0x705,0xe0a,0x1c14,0x3828,0x7050,0xe0a0,0xc040,0x30203,0x70507,0xe0a0e,0x1c141c,0x382838,0x705070,0xe0a0e0,0xc040c0,0x3020300,0x7050700,0xe0a0e00,0x1c141c00,0x38283800,0x70507000,0xe0a0e000,0xc040c000,0x302030000,0x705070000,0xe0a0e0000,0x1c141c0000,0x3828380000,0x7050700000,0xe0a0e00000,0xc040c00000,0x30203000000,0x70507000000,0xe0a0e000000,0x1c141c000000,0x382838000000,0x705070000000,0xe0a0e0000000,0xc040c0000000,0x3020300000000,0x7050700000000,0xe0a0e00000000,0x1c141c00000000,0x38283800000000,0x70507000000000,0xe0a0e000000000,0xc040c000000000,0x302030000000000,0x705070000000000,0xe0a0e0000000000,0x1c141c0000000000,0x3828380000000000,0x7050700000000000,0xe0a0e00000000000,0xc040c00000000000,0x203000000000000,0x507000000000000,0xa0e000000000000,0x141c000000000000,0x2838000000000000,0x5070000000000000,0xa0e0000000000000,0x40c0000000000000,
  /* bishop */
  0x8040201008040200,0x80402010080500,0x804020110a00,0x8041221400,0x182442800,0x10204885000,0x102040810a000,0x102040810204000,0x4020100804020002,0x8040201008050005,0x804020110a000a,0x804122140014,0x18244280028,0x1020488500050,0x102040810a000a0,0x204081020400040,0x2010080402000204,0x4020100805000508,0x804020110a000a11,0x80412214001422,0x1824428002844,0x102048850005088,0x2040810a000a010,0x408102040004020,0x1008040200020408,0x2010080500050810,0x4020110a000a1120,0x8041221400142241,0x182442800284482,0x204885000508804,0x40810a000a01008,0x810204000402010,0x804020002040810,0x1008050005081020,0x20110a000a112040,0x4122140014224180,0x8244280028448201,0x488500050880402,0x810a000a0100804,0x1020400040201008,0x402000204081020,0x805000508102040,0x110a000a11204080,0x2214001422418000,0x4428002844820100,0x8850005088040201,0x10a000a010080402,0x2040004020100804,0x200020408102040,0x500050810204080,0xa000a1120408000,0x1400142241800000,0x2800284482010000,0x5000508804020100,0xa000a01008040201,0x4000402010080402,0x2040810204080,0x5081020408000,0xa112040800000,0x14224180000000,0x28448201000000,0x50880402010000,0xa0100804020100,0x40201008040201,
  /* rook */
  0x1010101010101fe,0x2020202020202fd,0x4040404040404fb,0x8080808080808f7,0x10101010101010ef,0x20202020202020df,0x40404040404040bf,0x808080808080807f,0x10101010101fe01,0x20202020202fd02,0x40404040404fb04,0x80808080808f708,0x101010101010ef10,0x202020202020df20,0x404040404040bf40,0x8080808080807f80,0x101010101fe0101,0x202020202fd0202,0x404040404fb0404,0x808080808f70808,0x1010101010ef1010,0x2020202020df2020,0x4040404040bf4040,0x80808080807f8080,0x1010101fe010101,0x2020202fd020202,0x4040404fb040404,0x8080808f7080808,0x10101010ef101010,0x20202020df202020,0x40404040bf404040,0x808080807f808080,0x10101fe01010101,0x20202fd02020202,0x40404fb04040404,0x80808f708080808,0x101010ef10101010,0x202020df20202020,0x404040bf40404040,0x8080807f80808080,0x101fe0101010101,0x202fd0202020202,0x404fb0404040404,0x808f70808080808,0x1010ef1010101010,0x2020df2020202020,0x4040bf4040404040,0x80807f8080808080,0x1fe010101010101,0x2fd020202020202,0x4fb040404040404,0x8f7080808080808,0x10ef101010101010,0x20df202020202020,0x40bf404040404040,0x807f808080808080,0xfe01010101010101,0xfd02020202020202,0xfb04040404040404,0xf708080808080808,0xef10101010101010,0xdf20202020202020,0xbf40404040404040,0x7f80808080808080,
  /* queen */
  0x81412111090503fe,0x2824222120a07fd,0x404844424150efb,0x8080888492a1cf7,0x10101011925438ef,0x2020212224a870df,0x404142444850e0bf,0x8182848890a0c07f,0x412111090503fe03,0x824222120a07fd07,0x4844424150efb0e,0x80888492a1cf71c,0x101011925438ef38,0x20212224a870df70,0x4142444850e0bfe0,0x82848890a0c07fc0,0x2111090503fe0305,0x4222120a07fd070a,0x844424150efb0e15,0x888492a1cf71c2a,0x1011925438ef3854,0x212224a870df70a8,0x42444850e0bfe050,0x848890a0c07fc0a0,0x11090503fe030509,0x22120a07fd070a12,0x4424150efb0e1524,0x88492a1cf71c2a49,0x11925438ef385492,0x2224a870df70a824,0x444850e0bfe05048,0x8890a0c07fc0a090,0x90503fe03050911,0x120a07fd070a1222,0x24150efb0e152444,0x492a1cf71c2a4988,0x925438ef38549211,0x24a870df70a82422,0x4850e0bfe0504844,0x90a0c07fc0a09088,0x503fe0305091121,0xa07fd070a122242,0x150efb0e15244484,0x2a1cf71c2a498808,0x5438ef3854921110,0xa870df70a8242221,0x50e0bfe050484442,0xa0c07fc0a0908884,0x3fe030509112141,0x7fd070a12224282,0xefb0e1524448404,0x1cf71c2a49880808,0x38ef385492111010,0x70df70a824222120,0xe0bfe05048444241,0xc07fc0a090888482,0xfe03050911214181,0xfd070a1222428202,0xfb0e152444840404,0xf71c2a4988080808,0xef38549211101010,0xdf70a82422212020,0xbfe0504844424140,0x7fc0a09088848281,
};
/* generate rook moves via koggestone shifts */
Bitboard ks_attacks_ls1(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  /* directions left shifting <<1 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 1);
  bbPro  &=           (bbPro << 1);
  bbGen  |= bbPro &   (bbGen << 2*1);
  bbPro  &=           (bbPro << 2*1);
  bbGen  |= bbPro &   (bbGen << 4*1);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen << 1);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_ls8(Bitboard bbBlockers, Square sq)
{
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  /* directions left shifting <<8 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 8);
  bbPro  &=           (bbPro << 8);
  bbGen  |= bbPro &   (bbGen << 2*8);
  bbPro  &=           (bbPro << 2*8);
  bbGen  |= bbPro &   (bbGen << 4*8);
  /* shift one further */
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

  /* directions right shifting >>1 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 1);
  bbPro  &=           (bbPro >> 1);
  bbGen  |= bbPro &   (bbGen >> 2*1);
  bbPro  &=           (bbPro >> 2*1);
  bbGen  |= bbPro &   (bbGen >> 4*1);
  /* shift one further */
  bbGen   = bbWrap &  (bbGen >> 1);
  bbMoves|= bbGen;

  return bbMoves;
}
Bitboard ks_attacks_rs8(Bitboard bbBlockers, Square sq)
{
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  /* directions right shifting >>8 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 8);
  bbPro  &=           (bbPro >> 8);
  bbGen  |= bbPro &   (bbGen >> 2*8);
  bbPro  &=           (bbPro >> 2*8);
  bbGen  |= bbPro &   (bbGen >> 4*8);
  /* shift one further */
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
/* generate bishop moves via koggestone shifts */
Bitboard ks_attacks_ls9(Bitboard bbBlockers, Square sq)
{
  Bitboard bbWrap;
  Bitboard bbPro;
  Bitboard bbGen;
  Bitboard bbMoves = BBEMPTY;

  /* directions left shifting <<9 BISHOP */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 9);
  bbPro  &=           (bbPro << 9);
  bbGen  |= bbPro &   (bbGen << 2*9);
  bbPro  &=           (bbPro << 2*9);
  bbGen  |= bbPro &   (bbGen << 4*9);
  /* shift one further */
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

  /* directions left shifting <<7 BISHOP */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen << 7);
  bbPro  &=           (bbPro << 7);
  bbGen  |= bbPro &   (bbGen << 2*7);
  bbPro  &=           (bbPro << 2*7);
  bbGen  |= bbPro &   (bbGen << 4*7);
  /* shift one further */
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

  /* directions right shifting >>9 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTHFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 9);
  bbPro  &=           (bbPro >> 9);
  bbGen  |= bbPro &   (bbGen >> 2*9);
  bbPro  &=           (bbPro >> 2*9);
  bbGen  |= bbPro &   (bbGen >> 4*9);
  /* shift one further */
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

  /* directions right shifting <<7 ROOK */
  bbPro   = ~bbBlockers;
  bbGen   = SETMASKBB(sq);
  bbWrap  = BBNOTAFILE;
  bbPro  &= bbWrap;
  /* do kogge stone */
  bbGen  |= bbPro &   (bbGen >> 7);
  bbPro  &=           (bbPro >> 7);
  bbGen  |= bbPro &   (bbGen >> 2*7);
  bbPro  &=           (bbPro >> 2*7);
  bbGen  |= bbPro &   (bbGen >> 4*7);
  /* shift one further */
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


/* is square attacked by an enemy piece, via superpiece approach */
bool squareunderattack(__private Bitboard *board, bool stm, Square sq) 
{
  Bitboard bbWork;
  Bitboard bbMoves;
  Bitboard bbBlockers;
  Bitboard bbMe;

  bbBlockers = board[1]|board[2]|board[3];
  bbMe       = (stm)?board[0]:(board[0]^bbBlockers);

  /* rooks and queens */
  bbMoves = rook_attacks(bbBlockers, sq);
  bbWork =    (bbMe&(board[1]&~board[2]&board[3])) 
            | (bbMe&(~board[1]&board[2]&board[3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  bbMoves = bishop_attacks(bbBlockers, sq);
  /* bishops and queens */
  bbWork =  (bbMe&(~board[1]&~board[2]&board[3])) 
          | (bbMe&(~board[1]&board[2]&board[3]));
  if (bbMoves&bbWork)
  {
    return true;
  }
  /* knights */
  bbWork = bbMe&(~board[1]&board[2]&~board[3]);
  bbMoves = AttackTables[128+sq] ;
  if (bbMoves&bbWork) 
  {
    return true;
  }
  /* pawns */
  bbWork = bbMe&(board[1]&~board[2]&~board[3]);
  bbMoves = AttackTables[!stm*64+sq];
  if (bbMoves&bbWork)
  {
    return true;
  }
  /* king */
  bbWork = bbMe&(board[1]&board[2]&~board[3]);
  bbMoves = AttackTables[192+sq];
  if (bbMoves&bbWork)
  {
    return true;
  } 

  return false;
}
// bestfirst minimax search on gpu
__kernel void bestfirst_gpu(  
                            __global Bitboard *init_board,
                            __global NodeBlock *board_stack_1,
                            __global NodeBlock *board_stack_2,
                            __global s32 *global_return,
                            __global u64 *COUNTERS,
                            __global int *board_stack_top,
                            __global int *total_nodes_visited,
                            __global int *global_pid_movecounter,
                            __global int *global_pid_todoindex,
                            __global int *global_pid_ab_score,
                            __global int *global_pid_depths,
                            __global Move *global_pid_moves,
                            __global int *global_finished,
                            __global int *global_movecount,
                            __global int *global_plyreached,
                            __global Hash *global_HashHistory,
                               const int som_init,
                               const int ply_init,
                               const int search_depth,
                               const int max_nodes_to_expand,
                               const int max_nodes_per_slot,
                               const u64 max_nodes,
                               const s32 max_depth,
                            __global u64 *Zobrist
)

{

    __private Bitboard board[5];
    __global NodeBlock *board_stack;
    __global NodeBlock *board_stack_tmp;

    const s32 lid = get_local_id(2);
    const s32 pid = get_global_id(0) * get_global_size(1) * get_global_size(2) + get_global_id(1) * get_global_size(2) + get_global_id(2);
    const s32 localThreads = get_global_size(2);

    bool som = (bool)som_init;
    bool kic = false;
    bool rootkic = false;
    bool qs = false;
    bool silent = false;

    s32 index = 0;
    s32 current = 0;
    s32 parent  = 0;
    s32 child   = 0;
    s32 tmp   = 0;

    s32 depth = search_depth;
    s32 sd = 0;
    s32 ply = 0;

    Cr CR  = 0;
        
    Move move = 0;
    Move tmpmove = 0;
    Move lastmove = 0;

    Score score = 0;
    Score tmpscore = 0;
    float zeta = 0;
    float tmpscorea = 0;
    float tmpscoreb = 0;

    s32 mode = INIT;

    s32 i = 0;
    s32 j = 0;
    s32 n = 0;
    s32 k = 0;

    Square kingpos = 0;
    Square pos;
    Square to;
    Square cpt;   
    Square ep;
    Piece piece;
    Piece pieceto;
    Piece piececpt;

    Bitboard bbTemp         = 0;
    Bitboard bbTempO        = 0;
    Bitboard bbWork         = 0;
    Bitboard bbMe           = 0;
    Bitboard bbOpp          = 0;
    Bitboard bbBlockers     = 0;
    Bitboard bbMoves        = 0;

    ulong4 bbWrap4;
    ulong4 bbPro4;
    ulong4 bbGen4; 


    // init vars

    // assign root node to pid 0
    if (pid == 0 && *board_stack_top == 1) {

        // root node
        index = 0;
        // get init board
        board[0] = init_board[0];
        board[1] = init_board[1];
        board[2] = init_board[2];
        board[3] = init_board[3];
        board[4] = init_board[4]; // Hash

        som     = (bool)som_init;
        ply     = 0;
        sd      = 0;

        lastmove = board_stack_1[0].move;
        mode    = EXPAND;
    }


    // main loop
    while( (*board_stack_top < max_nodes_to_expand && *global_finished < max_nodes*8 && *total_nodes_visited < max_nodes && *global_movecount < max_nodes*12) || (TERMINATESOFT == 1 && (mode!=INIT && mode != SELECTION)) )
    {
        // Iterations Counter
        COUNTERS[pid*10+0]++;

        // single reply and mate hack
        if ( board_stack_1[0].children == 1 || board_stack_1[0].children == 0 || ISMATE(board_stack_1[0].score)) {
            *global_return = board_stack_1[board_stack_1[0].child].move;
            break;
        }

        if (mode == INIT) {

            index = 0;
            board_stack_1[0].visits++;

            // get init board
            board[0] = init_board[0];
            board[1] = init_board[1];
            board[2] = init_board[2];
            board[3] = init_board[3];
            board[4] = init_board[4]; // Hash

            depth   = search_depth;
            som     = (bool)som_init;
            ply     = 0;
            sd      = 0;

            lastmove = board_stack_1[0].move;


            mode = SELECTION;

            zeta = (float)board_stack_1[0].score;

        }

        if ( mode == SELECTION) {


            board_stack = (index >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
            n = board_stack[(index%max_nodes_per_slot)].children;


            // check movecount
            if (n<=0)
                mode = INIT;
            // check bounds TODO: repetition detection
            if (sd>=max_depth)
                mode = INIT;

            tmpscorea = -1000000000;
            current = 0;
            k = -1;

    		    // selecta best move
            for (i=0; i < n; i++ ) {
        
                child = board_stack[(index%max_nodes_per_slot)].child + i;

                board_stack_tmp = (child >= max_nodes_per_slot)? board_stack_2 : board_stack_1;

                // if child node is locked or dead
                if (board_stack_tmp[(child%max_nodes_per_slot)].lock > -1 || board_stack_tmp[(child%max_nodes_per_slot)].children == 0)
                    continue;

                tmpscoreb = (float)-board_stack_tmp[(child%max_nodes_per_slot)].score;

                // skip draw score
                if ( SKIPDRAW == 1 && tmpscoreb == 0)
                    continue;

                // skip check mate
                if ( SKIPMATE == 1 && index > 0 && ISMATE(tmpscoreb))
                    continue;

                // prune bad score
//                if (ZETAPRUNING == 1 && index > 0 && abs(board_stack_tmp[(child%max_nodes_per_slot)].score) != INF && abs(zeta) != INF && abs(zeta) < MATESCORE && tmpscore < zeta )
//                if (ZETAPRUNING == 1 && index > 0 && abs(board_stack_tmp[(child%max_nodes_per_slot)].score) != INF && abs(zeta) != INF && abs(zeta) < MATESCORE && tmpscore+ (EvalPieceValues[QUEEN]/(pid+1)) < zeta )
//                if (ZETAPRUNING == 1 && index > 0 && abs(board_stack_tmp[(child%max_nodes_per_slot)].score) != INF && abs(zeta) != INF && abs(zeta) < MATESCORE && tmpscore+pid < zeta )
                if (ZETAPRUNING == 1 && index > 0 && lid < localThreads/4 && !ISINF(tmpscoreb) && !ISINF(zeta) && !ISMATE(zeta) && tmpscoreb < zeta )
                  continue;
                else if (ROOTSEARCH == 1 && index==0)
                    tmpscoreb = (float)-board_stack_tmp[(child%max_nodes_per_slot)].visits;
                else
                {
                  tmpscoreb*= SCOREWEIGHT;
                  tmpscoreb+= (s32) (((float)board_stack[(index%max_nodes_per_slot)].visits) / (SMOOTHUCT*(float)board_stack_tmp[(child%max_nodes_per_slot)].visits+1));
                }
                if ( tmpscoreb > tmpscorea ) {
                    tmpscorea = tmpscoreb;
                    current = child;
                }
            }

            board_stack_tmp = (current >= max_nodes_per_slot)? board_stack_2 : board_stack_1;


            if (current > 0 && (board_stack_tmp[(current%max_nodes_per_slot)].lock > -1 || board_stack_tmp[(current%max_nodes_per_slot)].children == 0) )
                current = 0;

            // get lock
            if (current > 0 && board_stack_tmp[(current%max_nodes_per_slot)].children == -1) {
                atom_cmpxchg(&board_stack_tmp[(current%max_nodes_per_slot)].lock, -1, pid);
                k = atom_cmpxchg(&board_stack_tmp[(current%max_nodes_per_slot)].lock, pid, pid);
            }

            // if otherwise locked, repeat
            if (k != -1 && k != pid)
                current = 0;

            // got lock, do move and exit selection phase to expand phase
            if (k == pid ) {
                if (board_stack_tmp[(current%max_nodes_per_slot)].score == -INF)
                    mode = EXPAND;
            }                    
            // nothing goes
            if ( current == 0 )
                mode = INIT;

            // got a child, do move
            if (current > 0) {

                board_stack_tmp = (current >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
                board_stack_tmp[(current%max_nodes_per_slot)].visits++;

                move = board_stack_tmp[(current%max_nodes_per_slot)].move;

                lastmove = move;

                ply++;

                atom_max(global_plyreached, ply);

                domove(board, move);

                global_HashHistory[pid*1024+ply+ply_init] = board[4];

                som = !som;    

//                updateHash(board, move, Zobrist);\n
                board[4] = computeHash(board, som, Zobrist);

                index = current;

                zeta = (float)board_stack_tmp[(current%max_nodes_per_slot)].score;
            }
        }

        if ( mode == INIT || mode == SELECTION ) {
            atom_inc(global_finished);
            continue;
        }

        atom_add(global_finished, 8);

        // #########################################
        // ####         Move Generator          ####
        // #########################################
        n = 0;
        k = 0;
        silent = true;
        bbBlockers  =  board[1]|board[2]|board[3];
        bbMe        = (som)?board[0]:(board[0]^bbBlockers);
        bbOpp       = (!som)?board[0]:(board[0]^bbBlockers);
        bbWork      = bbMe;

        //get king position
        bbTemp  = bbMe & board[1] & board[2] & ~board[3]; // get king
        kingpos = first1(bbTemp);

        rootkic = false;
        // king in check?
        rootkic = squareunderattack(board, !som, kingpos);

        CR = (Cr)((lastmove>>36)&0xF);

        // in check search extension
        depth = (rootkic&&sd-search_depth<MAXEVASIONS)?sd:depth;

        // enter quiescence search?
        qs = (sd<=depth)?false:true;
        qs = (mode==EXPAND||mode==EVALLEAF)?false:qs;

        while( bbWork )  {

            // pop 1st bit
            pos  = popfirst1(&bbWork);
            piece = GETPIECE(board,pos);

    bbTempO  = SETMASKBB(pos);
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
    bbMoves |= (!qs||rootkic)?(bbTempO&bbTemp&~bbBlockers):BBEMPTY; 

            while( bbMoves )  {

                // pop 1st bit
                to = popfirst1(&bbMoves);

                // en passant to
                child = pos-to;
                ep = ( (piece>>1) == PAWN && abs(child) == 16 ) ? to : 0;

                cpt = to;
                pieceto = ((piece>>1)==PAWN&&GETRRANK(cpt,piece&0x1)==7)?(QUEEN<<1|(Piece)som):piece; // pawn promotion

                piececpt = GETPIECE(board,cpt);

                // make move
                move = ( (((Move)pos)&0x000000000000003F) | (((Move)to<<6)&0x0000000000000FC0) | (((Move)cpt<<12)&0x000000000003F000) | (((Move)piece<<18)&0x00000000003C0000) | (((Move)pieceto<<22)&0x0000000003C00000) | (((Move)piececpt<<26)&0x000000003C000000) | (((Move)ep<<30)&0x0000000FC0000000) | (((Move)ILL<<40)&0x00007F0000000000) | (((Move)ILL<<47)&0x003F800000000000) | (((Move)PEMPTY<<54)&0x03C0000000000000) );

                // TODO: pseudo legal move gen: 2x speedup?
                // domove
                domove(board, move);

                //get king position
                bbTemp  = board[0] ^ (board[1] | board[2] | board[3]);
                bbTemp  = (som&BLACK)? board[0]   : bbTemp ;
                bbTemp  = bbTemp & board[1] & board[2] & ~board[3]; // get king
                kingpos = first1(bbTemp);

                kic = false;
                // king in check?
                kic = squareunderattack(board, !som, kingpos);

                if (!kic) {

                    silent = ((piececpt>>1)!=PEMPTY)?false:silent;

                    k++; // check mate move counter

                    if (!qs||(qs&&(piececpt>>1)!=PEMPTY)) {
                        // update castle rights        
                        move = updateCR(move, CR);
                        // copy move to global
                        global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+n] = move;

                        // Movecounters
                        n++;
                        COUNTERS[pid*10+3]++;
                        atom_inc(global_movecount);

                        // sort move
                        child = pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+0;
                        for(j=n-1; j > 0; j--) {
                            if ( EvalMove(global_pid_moves[j+child]) > EvalMove(global_pid_moves[j-1+child])  ) {
                                tmpmove = global_pid_moves[j+child];
                                global_pid_moves[j+child] = global_pid_moves[j-1+child];
                                global_pid_moves[j-1+child] = tmpmove;
                           }
                           else
                            break;
                        }
                    }
                }

                // undomove
                undomove(board, move);
            }
        }
        // ################################
        // ####       Castle moves      ###
        // ################################
        // get Rooks
        bbOpp  = (bbMe & board[1] & ~board[2] & board[3] );
        //get king position
        bbTemp  = bbMe & board[1] & board[2] & ~board[3]; // get king
        kingpos = first1(bbTemp);

        // Queenside
        bbMoves  = (!som)? 0xE : 0x0E00000000000000;   
        bbMoves &= (board[1] | board[2] | board[3]); 

        if ( !qs && ( (!som && (CR&0x1)) || ( som && (CR&0x4))) && ( (!som && kingpos == 4) || (!som && kingpos == 60) ) && !rootkic && (!bbMoves) && (bbOpp & SETMASKBB(kingpos-4)) ) {

            kic      = false;            
            kic     |= squareunderattack(board, !som, kingpos-2);
            kic     |= squareunderattack(board, !som, kingpos-1);

            if (!kic) {


                // make move
                to = kingpos-4;
                cpt = kingpos-1;
                move = ( (((Move)kingpos)&0x000000000000003F) | (((Move)(kingpos-2)<<6)&0x0000000000000FC0) | (((Move)(kingpos-2)<<12)&0x000000000003F000) | (((Move)( (KING<<1) | (som&1))<<18)&0x00000000003C0000) | (((Move)( (KING<<1) | (som&1))<<22)&0x0000000003C00000) | (((Move)PEMPTY<<26)&0x000000003C000000) | (((Move)PEMPTY<<30)&0x0000000FC0000000) | (((Move)to<<40)&0x00007F0000000000) | (((Move)cpt<<47)&0x003F800000000000) | (((Move)( (ROOK<<1) | (som&1))<<54)&0x03C0000000000000) );

                // update castle rights        
                move = updateCR(move, CR);

                // copy move to global
                global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+n] = move;
                // Movecounters
                n++;
                k++;

                COUNTERS[pid*10+3]++;
                atom_inc(global_movecount);
            }
        }

        // kingside
        bbMoves  = (!som)? 0x60 : 0x6000000000000000;   
        bbMoves &= (board[1] | board[2] | board[3]); 

        if ( !qs && ( (!som && (CR&0x2)) || (som && (CR&0x8))) && ( (!som && kingpos == 4) || (!som && kingpos == 60) ) && !rootkic  && (!bbMoves)  && (bbOpp & SETMASKBB(kingpos+3))) {


            kic      = false;
            kic     |= squareunderattack(board, !som, kingpos+1);
            kic     |= squareunderattack(board, !som, kingpos+2);
        
            if (!kic) {


                // make move
                to = kingpos+3;
                cpt = kingpos+1;
                move = ( (((Move)kingpos)&0x000000000000003F) | (((Move)(kingpos+2)<<6)&0x0000000000000FC0) | (((Move)(kingpos+2)<<12)&0x000000000003F000) | (((Move)( (KING<<1) | (som&1))<<18)&0x00000000003C0000) | (((Move)( (KING<<1) | (som&1))<<22)&0x0000000003C00000) | (((Move)PEMPTY<<26)&0x000000003C000000) | (((Move)PEMPTY<<30)&0x0000000FC0000000) | (((Move)to<<40)&0x00007F0000000000) | (((Move)cpt<<47)&0x003F800000000000) | (((Move)( (ROOK<<1) | (som&1))<<54)&0x03C0000000000000) );

                // update castle rights        
                move = updateCR(move, CR);

                // copy move to global
                global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+n] = move;
                // Movecounters
                n++;
                k++;

                COUNTERS[pid*10+3]++;
                atom_inc(global_movecount);

            }
        }


        // ################################
        // ####     En passant moves    ###
        // ################################
        cpt = (lastmove>>30)&0x3F;

        if (cpt) {

            piece = ((PAWN<<1) | (Piece)som);
            pieceto = piece;
            piececpt = ( (PAWN<<1) | (Piece)(!som) );
            ep = 0;

            bbMoves  =  bbMe & board[1] & ~board[2]  & ~board[3];

            // white
            if (!som) {
                bbMoves &= (0xFF00000000 & (SETMASKBB(cpt+1)|SETMASKBB(cpt-1)));
                
            }
            // black     
            else {
                bbMoves &= (0xFF000000 & (SETMASKBB(cpt+1)|SETMASKBB(cpt-1)));
            }
            while(bbMoves)
            {

                // pop 1st bit
                pos = popfirst1(&bbMoves);

                to  = (som)? cpt-8 : cpt+8;
                
                // make move
                move = ( (((Move)pos)&0x000000000000003F) | (((Move)to<<6)&0x0000000000000FC0) | (((Move)cpt<<12)&0x000000000003F000) | (((Move)piece<<18)&0x00000000003C0000) | (((Move)pieceto<<22)&0x0000000003C00000) | (((Move)piececpt<<26)&0x000000003C000000) | (((Move)ep<<30)&0x0000000FC0000000) | (((Move)ILL<<40)&0x00007F0000000000) | (((Move)ILL<<47)&0x003F800000000000) | (((Move)PEMPTY<<54)&0x03C0000000000000) );

                // domove
                domove(board, move);

                //get king position
                bbTemp  = bbMe&board[1]&board[2]&~board[3];
                kingpos = first1(bbTemp);

                // king in check?
                kic = squareunderattack(board, !som, kingpos);

                if (!kic) {

                    // update castle rights        
                    move = updateCR(move, CR);
                    // copy move to global
                    global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+n] = move;
                    // Movecounters
                    n++;
                    k++;

                    COUNTERS[pid*10+3]++;
                    atom_inc(global_movecount);
                }

                // undomove
                undomove(board, move);
            }
        }


        // ################################
        // ####        Evaluation       ###
        // ################################

        // new eval, mostly ported from Stockfish
        bbBlockers  = board[1]|board[2]|board[3];        // all pieces
        bbTemp      = board[1]&~board[2]&~board[3];      // all pawns
        score = 0;
    
        // white
        bbMe  = board[0]^bbBlockers;
        bbOpp = board[0];
        bbWork = bbMe;

        while (bbWork) 
        {
          pos = popfirst1(&bbWork);
          piece = GETPIECETYPE(board,pos);

          /* piece bonus */
          score+= 10;
          /* wodd count */
          score+= EvalPieceValues[piece];
          /* piece posuare tables */
          score+= EvalTable[piece*64+FLIPFLOP(pos)];
          /* posuare control table */
          score+= EvalControl[FLIPFLOP(pos)];

          /* simple pawn structure white */
          /* blocked */
          score-=(piece==PAWN&&GETRANK(pos)<RANK_8&&(bbOpp&SETMASKBB(pos+8)))?15:0;
            /* chain */
          score+=(piece==PAWN&&GETFILE(pos)<FILE_H&&(bbTemp&bbMe&SETMASKBB(pos-7)))?10:0;
          score+=(piece==PAWN&&GETFILE(pos)>FILE_A&&(bbTemp&bbMe&SETMASKBB(pos-9)))?10:0;
          /* column */
          for(j=pos-8;j>7&&piece==PAWN;j-=8)
            score-=(bbTemp&bbMe&SETMASKBB(j))?30:0;
        }
        /* duble bishop */
        score+= (popcount(bbMe&(~board[1]&~board[2]&board[3]))>=2)?25:0;

        // black
        bbMe  = board[0];
        bbOpp = board[0]^bbBlockers;
        bbWork = bbMe;

        while (bbWork) 
        {
          pos = popfirst1(&bbWork);
          piece = GETPIECETYPE(board,pos);

          /* piece bonus */
          score-= 10;
          /* wodd count */
          score-= EvalPieceValues[piece];
          /* piece posuare tables */
          score-= EvalTable[piece*64+pos];
          /* posuare control table */
          score-= EvalControl[pos];

          /* simple pawn structure black */
          /* blocked */
          score+=(piece==PAWN&&GETRANK(pos)>RANK_1&&(bbOpp&SETMASKBB(pos-8)))?15:0;
            /* chain */
          score-=(piece==PAWN&&GETFILE(pos)>FILE_A&&(bbTemp&bbMe&SETMASKBB(pos+7)))?10:0;
          score-=(piece==PAWN&&GETFILE(pos)<FILE_H&&(bbTemp&bbMe&SETMASKBB(pos+9)))?10:0;
          /* column */
          for(j=pos+8;j<56&&piece==PAWN;j+=8)
            score+=(bbTemp&bbMe&SETMASKBB(j))?30:0;

        }
        /* duble bishop */
        score-= (popcount(bbMe&(~board[1]&~board[2]&board[3]))>=2)?25:0;

        // centi pawn *10
        score*=10;
        // hack for drawscore == 0, site to move bonus
        score+=(!som)?1:-1;

        // negamaxed scores
        score = (som)?-score:score;
        // checkmate
        score = (rootkic&&k==0)?-INF+ply+ply_init:score;
        // stalemate
        score = (!rootkic&&k==0&&!qs)?0:score;



        // draw by 3 fold repetition
        if (mode==EXPAND&&index>0)
        {
          for (i=ply+ply_init-2;i>0;i-=2)
          {
            if (board[4] == global_HashHistory[pid*1024+i])
            {
              n       = 0;
              score   = 0;
              break;
            }
          }   
        }   

        // out of range
        if (sd>=max_depth)
          n = 0;

        global_pid_movecounter[pid*max_depth+sd] = n;

        // eval leaf node
        if (mode == MOVEUP && n == 0 ) {
            global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] = score;
            mode = MOVEDOWN;
        }

        // stand pat
        // return Beta
        if (mode == MOVEUP && qs && !rootkic && score >= global_pid_ab_score[pid*max_depth*2+(sd)*2+BETA] ) {
//            global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] = global_pid_ab_score[pid*max_depth*2+(sd)*2+BETA]; // fail hard
            global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] = score; // fail soft
      			mode = MOVEDOWN;
        }

        // stand pat
        // set Alpha
        if (mode == MOVEUP && qs && !rootkic ) {
            atom_max(&global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA], score);
    		}


        // Expand Node
        if (mode == EXPAND ) {

           // Expand Node Counter
            COUNTERS[pid*10+1]++;

            // create child nodes
            current = atom_add(board_stack_top,n);

            // expand only if we got enough memory
            if ( n > 0 && current+n >= max_nodes_to_expand )
                n = -1;

            board_stack = (index >= max_nodes_per_slot)? board_stack_2 : board_stack_1;

            if (n > 0 ) {

                for(i=0;i<n;i++) {

                    move = global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+i];
    
                    parent = (current+i);      

                    board_stack_tmp = (parent >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
                
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
            }
            else if (n == 0) {
                board_stack[(index%max_nodes_per_slot)].score = score;
                board_stack[(index%max_nodes_per_slot)].children = n;
                mode = BACKUPSCORE;
            }
            else if (n < 0) {
                // release lock
                board_stack[(index%max_nodes_per_slot)].score = -INF;
                board_stack[(index%max_nodes_per_slot)].lock = -1;
                mode = INIT;
            }
        }

        if (mode == EVALLEAF) {

            sd = 0;
            // extensions
            depth = search_depth;
            depth = (rootkic)?search_depth+1:search_depth;
//            depth = (silent)?search_depth+1:depth;
            depth = (n==1)?search_depth+1:depth;

            global_pid_todoindex[pid*max_depth+sd] = 0;

            // set init Alpha Beta values
            global_pid_ab_score[pid*max_depth*2+0*2+ALPHA] = -INF;
            global_pid_ab_score[pid*max_depth*2+0*2+BETA]  =  INF;

            global_pid_depths[pid*max_depth+sd] = depth;

            mode = MOVEUP;
        }

        if (mode == MOVEDOWN ) {

            // incl AlphaBeta Pruning
            while (global_pid_todoindex[pid*max_depth+sd] >= global_pid_movecounter[pid*max_depth+sd] || (  global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA] >= global_pid_ab_score[pid*max_depth*2+(sd)*2+BETA] ) ) {

                // do negamax-scoring
                score = global_pid_ab_score[pid*max_depth*2+(sd)*2+ALPHA];
                if ( abs(score) != INF && sd > 0)
                    atom_max(&global_pid_ab_score[pid*max_depth*2+(sd-1)*2+ALPHA], -score);

                sd--;
                ply--;

                if (sd < 0)
                    break;

                depth = global_pid_depths[pid*max_depth+sd];

                move = global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+global_pid_todoindex[pid*max_depth+sd]-1];

                undomove(board, move);

                // switch site to move
                som = !som;

//                updateHash(board, move, Zobrist);
//                board[4] = computeHash(board, som, Zobrist);
            }
 
            mode = MOVEUP;
          
            if (sd < 0) {
                board_stack = (index >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
                board_stack[(index%max_nodes_per_slot)].score =  global_pid_ab_score[pid*max_depth*2+0*2+ALPHA];
                mode = BACKUPSCORE;
      			}
        }

        if (mode == MOVEUP ) {

            // AB Search Node Counter
            COUNTERS[pid*10+2]++;
            atom_inc(total_nodes_visited);

            move = global_pid_moves[pid*max_depth*MAXLEGALMOVES+sd*MAXLEGALMOVES+global_pid_todoindex[pid*max_depth+sd]];

            domove(board, move);

            lastmove = move;

            // switch site to move
            som = !som;

//            updateHash(board, move, Zobrist);
            board[4] = computeHash(board, som, Zobrist);

            global_pid_todoindex[pid*max_depth+sd]++;

            sd++;
            ply++;

            global_HashHistory[pid*1024+ply+ply_init] = board[4];

            global_pid_movecounter[pid*max_depth+sd] = 0;
            global_pid_todoindex[pid*max_depth+sd] = 0;
            global_pid_depths[pid*max_depth+sd] = depth;


            // Get Alpha and Beta from prev depth
            global_pid_ab_score[pid*max_depth*2+sd*2+ALPHA] = -global_pid_ab_score[pid*max_depth*2+(sd-1)*2+BETA];
            global_pid_ab_score[pid*max_depth*2+sd*2+BETA]  = -global_pid_ab_score[pid*max_depth*2+(sd-1)*2+ALPHA];

            continue;
        }
  
        if (mode == BACKUPSCORE) {

            mode = INIT;
            board_stack = (index >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
            parent = board_stack[(index%max_nodes_per_slot)].parent;


            // backup score
            while( parent >= 0 ) {

                score = -INF;
                board_stack = (parent >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
                j = board_stack[(parent%max_nodes_per_slot)].children;

                for(i=0;i<j;i++) {


                    child = board_stack[(parent%max_nodes_per_slot)].child + i;
                    board_stack_tmp = (child >= max_nodes_per_slot)? board_stack_2 : board_stack_1;

                    tmpscore = -board_stack_tmp[(child%max_nodes_per_slot)].score;

                    if (ISINF(tmpscore)) { // skip unexplored 
                        score = -INF;
                        break;
                    }
                    if (tmpscore > score) {
                        score = tmpscore;
                    }
                }


                if (!ISINF(score))
                {
                    tmpscore = atom_xchg(&board_stack[(parent%max_nodes_per_slot)].score, score);
                    if (parent == 0 && score > tmpscore)
                        COUNTERS[7] = (u64)ply+1;
                }
                else
                    break;

                parent = board_stack[(parent%max_nodes_per_slot)].parent;
            }
            // release lock
            board_stack = (index >= max_nodes_per_slot)? board_stack_2 : board_stack_1;
            board_stack[(index%max_nodes_per_slot)].lock = -1;
        }
    } 
        
    // return to host
    COUNTERS[5] = *global_plyreached;
    COUNTERS[6] = (*board_stack_top >= max_nodes_to_expand)? 1 : 0;
}

