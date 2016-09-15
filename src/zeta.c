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

#include <stdio.h>      // for file IO

#include "bitboard.h"   // bit functions
#include "timer.h"
#include "types.h"
#include "zobrist.h"

const char filename[]  = "zeta.cl";
char *source;
size_t sourceSize;

u64 MOVECOUNT = 0;
u64 NODECOUNT = 0;
u64 TNODECOUNT = 0;
u64 ABNODECOUNT = 0;
s32 NODECOPIES = 0;
u64 MEMORYFULL = 0;

s32 PLY = 0;
s32 PLYPLAYED = 0;
bool STM = WHITE;

// config
s32 threadsX            =  0;
s32 threadsY            =  0;
s32 threadsZ            =  0;
s32 totalThreads        =  0;
s32 nodes_per_second    =  0;
u64 max_nodes           =  0;
s32 nps_current         =  0;
s32 max_nodes_to_expand =  0;
s32 memory_slots        =  1;
s32 max_leaf_depth      =  0;
s32 max_depth           = 99;
s32 opencl_device_id    =  0;
s32 opencl_platform_id  =  0;

s32 search_depth    = 0;
u64 max_mem_mb      = 512;
s32 max_cores       = 1;
s32 force_mode      = false;
s32 random_mode     = false;
s32 xboard_mode     = false;
s32 xboard_debug    = false;
s32 post_mode       = false;
s32 time_management = false;

Move *MoveHistory;
Hash *HashHistory;

// time management
static double max_time_per_move = 0;
static double time_per_move     = 0;
static s32 max_moves            = 0;
static s32 time_seconds         = 0;
static s32 time_minutes         = 0;
static double time_left_opponent = 0;  
static double time_left_computer = 0;  
static char time_string[128];
static s32 max_nps_per_move     = 0;

double Elapsed;
Score bestscore = 0;
s32 plyreached = 0;
s32 bestmoveply = 0;

// our quad bitboard
Bitboard BOARD[5];

// for exchange with OpenCL Device
Bitboard *GLOBAL_INIT_BOARD;
NodeBlock *NODES = NULL;
u64 *COUNTERS;
Hash *GLOBAL_HASHHISTORY;
s32 BOARD_STACK_TOP;

Move Bestmove = 0;
Move Lastmove = 0;
Cr  CR = 0;

// functions
Move move_parser(char *usermove, Bitboard *board, bool stm);
void setboard(char *fen);
void move2alg(Move move, char * movec);
void print_movealg(Move move);
void print_bitboard(Bitboard board);
void print_board(Bitboard *board);
void print_stats();
void read_config();
s32 load_file_to_string(const char *filename, char **result);
// cl functions
extern bool cl_init_device();
extern bool cl_init_objects();
extern bool cl_run_kernel(bool stm, s32 depth);
extern bool cl_get_and_release_memory();
extern bool cl_release_device();
extern bool cl_guess_config(bool extreme);
// precomputed attack tables for move generation and square in check
const Bitboard AttackTablesPawnPushes[2*64] = 
{
  /* white pawn pushes */
  0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x1010000,0x2020000,0x4040000,0x8080000,0x10100000,0x20200000,0x40400000,0x80800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10000000000,0x20000000000,0x40000000000,0x80000000000,0x100000000000,0x200000000000,0x400000000000,0x800000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000,0x100000000000000,0x200000000000000,0x400000000000000,0x800000000000000,0x1000000000000000,0x2000000000000000,0x4000000000000000,0x8000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  /* black pawn pushes */
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10100000000,0x20200000000,0x40400000000,0x80800000000,0x101000000000,0x202000000000,0x404000000000,0x808000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000
};
/* piece attack tables */
const Bitboard AttackTables[7*64] = 
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
bool squareunderattack(Bitboard *board, bool stm, Square sq) 
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

/* ############################# */
/* ###        inits          ### */
/* ############################# */

void free_resources()
{
  if(GLOBAL_INIT_BOARD)
    free(GLOBAL_INIT_BOARD);
  if(COUNTERS)
    free(COUNTERS);
  if(NODES)
    free(NODES);
  if(GLOBAL_HASHHISTORY)
    free(GLOBAL_HASHHISTORY);

  NODES = NULL;

  cl_release_device();
}

void inits()
{
  if (MoveHistory)
    free(MoveHistory);
  MoveHistory = (Move*)calloc(MAXGAMEPLY, sizeof(Move));
  if (HashHistory)
    free(HashHistory);
  HashHistory = (Hash*)calloc(MAXGAMEPLY, sizeof(Hash));
}


/* ############################# */
/* ###     move tools        ### */
/* ############################# */

static inline Move makemove(Square from, Square to, Square cpt, Piece pfrom, Piece pto, Piece pcpt, Square pdsq ) {
    return (from | (Move)to<<6 |  (Move)cpt<<12 |  (Move)pfrom<<18 | (Move)pto<<22 | (Move)pcpt<<26  | (Move)pdsq<<30);  
}
static inline Move getmove(Move move) {
    return (move & 0x3FFFFFFF);  
}
static inline Square getfrom(Move move) {
    return (move & 0x3F);
}
static inline Square getto(Move move) {
    return ((move>>6) & 0x3F);
}
static inline Square getcpt(Move move) {
    return ((move>>12) & 0x3F);
}
static inline Piece getpfrom(Move move) {
    return ((move>>18) & 0xF);
}
static inline Piece getpto(Move move) {
    return ((move>>22) & 0xF);
}
static inline Piece getpcpt(Move move) {
    return ((move>>26) & 0xF);
}

Piece getPiece (Bitboard *board, Square sq) {

   return ((board[0] >> sq) & 1)
      + 2*((board[1] >> sq) & 1)
      + 4*((board[2] >> sq) & 1)
      + 8*((board[3] >> sq) & 1);
}


/* ############################# */
/* ###         Hash          ### */
/* ############################# */

Hash computeHash(Bitboard *board, bool stm) {

    Piece piece;
    s32 side;
    Square pos;
    Bitboard bbBoth[2];
    Bitboard bbWork = 0;
    Hash hash = 0;
    Hash zobrist;

    bbBoth[0]   = ( board[0] ^ (board[1] | board[2] | board[3]));
    bbBoth[1]   =   board[0];

    // for each side
    for(side=0; side<2;side++) {
        bbWork = bbBoth[side];

        // each piece
        while (bbWork) {
            // pop 1st bit
            pos = popfirst1(&bbWork);
            piece = GETPIECETYPE(board, pos);
            zobrist = Zobrist[piece-1];
            hash ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64

        }
    }
    if (!stm)
      hash^=0x1;

    return hash;    
}

void updateHash(Bitboard *board, Move move)
{
  Square castlefrom   = (Square)((move>>40) & 0x7F); // is set to illegal square 64 when empty
  Square castleto     = (Square)((move>>47) & 0x7F); // is set to illegal square 64 when empty
  Bitboard castlepciece = ((move>>54) & 0xF)>>1;  // is set to 0 when PNONE
  Square pos;
  Hash zobrist;

  // from
  zobrist = Zobrist[(((move>>18)&0xF)>>1)-1];
  pos = (Square)(move&0x3F);
  board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64

  // to
  zobrist = Zobrist[(((move>>22)&0xF)>>1)-1];
  pos = (Square)((move>>6)&0x3F);
  board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64

  // capture
  if ( ((move>>26)&0xF)!=PNONE)
  {
    zobrist = Zobrist[(((move>>26)&0xF)>>1)-1];
    pos = (Square)((move>>12)&0x3F);
    board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  }

  // castle from
  if (castlefrom<ILL&&castlepciece!=PNONE )
  {
    zobrist = Zobrist[ROOK-1];
    pos = castlefrom;
    board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  }

  // castle to
  if (castleto < ILL && castlepciece != PNONE )
  {
    zobrist = Zobrist[ROOK-1];
    pos = castleto;
    board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  }
  // site to move
  board[4]^=0x1;
}


/* ############################# */
/* ###     domove undomove   ### */
/* ############################# */
Move updateCR(Move move, Cr cr) {

    Square from   =  (move & 0x3F);
    Piece piece   =  (((move>>18) & 0xF)>>1);
    bool stm = ((move>>18) & 0xF)&1;

    // castle rights update
    if (cr > 0) {

        // update castle rights, TODO: make nice
        // clear white queenside
        if ( (piece == ROOK && (stm == WHITE) && from == 0) || ( piece == KING && (stm == WHITE) && from == 4))
            cr&= 0xE;
        // clear white kingside
        if ( (piece == ROOK && (stm == WHITE) && from == 7) || ( piece == KING && (stm == WHITE) && from == 4))
            cr&= 0xD;
        // clear black queenside
        if ( (piece == ROOK && (stm == BLACK) && from == 56) || ( piece == KING && (stm == BLACK) && from == 60))
            cr&= 0xB;
        // clear black kingside
        if ( (piece == ROOK && (stm == BLACK) && from == 63) || ( piece == KING && (stm == BLACK) && from == 60))
            cr&= 0x7;

    }    
    move &= 0xFFFFFF0FFFFFFFFF; // clear old cr
    move |= ((Move)cr<<36)&0x000000F000000000; // set new cr

    return move;
}

void domove(Bitboard *board, Move move) {

    Square from = getfrom(move);
    Square to   = getto(move);
    Square cpt  = getcpt(move);

    Bitboard pfrom   = getpfrom(move);
    Bitboard pto   = getpto(move);

    // Castle move kingside move rook
    if ( ( (pfrom>>1) == KING) && (to-from == 2) ) {

        // unset from rook
        board[0] &= CLRMASKBB(from+3);
        board[1] &= CLRMASKBB(from+3);
        board[2] &= CLRMASKBB(from+3);
        board[3] &= CLRMASKBB(from+3);

        // set to rook
        board[0] |= (Bitboard)(pfrom&1)<<(to-1); // set color
        board[1] |= (Bitboard)((ROOK)&1)<<(to-1);
        board[2] |= (Bitboard)((ROOK>>1)&1)<<(to-1);
        board[3] |= (Bitboard)((ROOK>>2)&1)<<(to-1);
    }
    // Castle move queenside move rook
    if ( ( (pfrom>>1) == KING) && (from-to == 2) ) {

        // unset from rook
        board[0] &= CLRMASKBB(from-4);
        board[1] &= CLRMASKBB(from-4);
        board[2] &= CLRMASKBB(from-4);
        board[3] &= CLRMASKBB(from-4);

        // set to rook
        board[0] |= (Bitboard)(pfrom&1)<<(to+1); // set color
        board[1] |= (Bitboard)((ROOK)&1)<<(to+1);
        board[2] |= (Bitboard)((ROOK>>1)&1)<<(to+1);
        board[3] |= (Bitboard)((ROOK>>2)&1)<<(to+1);

    }


    // unset from
    board[0] &= CLRMASKBB(from);
    board[1] &= CLRMASKBB(from);
    board[2] &= CLRMASKBB(from);
    board[3] &= CLRMASKBB(from);

    // unset cpt
    board[0] &= CLRMASKBB(cpt);
    board[1] &= CLRMASKBB(cpt);
    board[2] &= CLRMASKBB(cpt);
    board[3] &= CLRMASKBB(cpt);

    // unset to
    board[0] &= CLRMASKBB(to);
    board[1] &= CLRMASKBB(to);
    board[2] &= CLRMASKBB(to);
    board[3] &= CLRMASKBB(to);

    // set to
    board[0] |= (pto&1)<<to;
    board[1] |= ((pto>>1)&1)<<to;
    board[2] |= ((pto>>2)&1)<<to;
    board[3] |= ((pto>>3)&1)<<to;

    // hash
    board[4] = computeHash(board, (pfrom&1));

//    updateHash(board, move);

}
void undomove(Bitboard *board, Move move) {


    Square from = getfrom(move);
    Square to   = getto(move);
    Square cpt  = getcpt(move);

    Bitboard pfrom = getpfrom(move);
    Bitboard pcpt  = getpcpt(move);

    // Castle move kingside move rook
    if ( ( (pfrom>>1) == KING) && (to-from == 2) ) {

        // unset to rook
        board[0] &= CLRMASKBB(to-1);
        board[1] &= CLRMASKBB(to-1);
        board[2] &= CLRMASKBB(to-1);
        board[3] &= CLRMASKBB(to-1);

        // set from rook
        board[0] |= (Bitboard)(pfrom&1)<<(from+3); // set color
        board[1] |= (Bitboard)((ROOK)&1)<<(from+3);
        board[2] |= (Bitboard)((ROOK>>1)&1)<<(from+3);
        board[3] |= (Bitboard)((ROOK>>2)&1)<<(from+3);
    }
    // Castle move queenside move rook
    if ( ( (pfrom>>1) == KING) && (from-to == 2) ) {

        // unset to rook
        board[0] &= CLRMASKBB(to+1);
        board[1] &= CLRMASKBB(to+1);
        board[2] &= CLRMASKBB(to+1);
        board[3] &= CLRMASKBB(to+1);

        // set from rook
        board[0] |= (Bitboard)(pfrom&1)<<(from-4); // set color
        board[1] |= (Bitboard)((ROOK)&1)<<(from-4);
        board[2] |= (Bitboard)((ROOK>>1)&1)<<(from-4);
        board[3] |= (Bitboard)((ROOK>>2)&1)<<(from-4);

    }

    // unset to
    board[0] &= CLRMASKBB(to);
    board[1] &= CLRMASKBB(to);
    board[2] &= CLRMASKBB(to);
    board[3] &= CLRMASKBB(to);

    // unset cpt
    board[0] &= CLRMASKBB(cpt);
    board[1] &= CLRMASKBB(cpt);
    board[2] &= CLRMASKBB(cpt);
    board[3] &= CLRMASKBB(cpt);

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

    // hash
    board[4] = computeHash(board, (pfrom&1));

}

/* ############################# */
/* ###      root search      ### */
/* ############################# */
Move rootsearch(Bitboard *board, bool stm, s32 depth, Move lastmove) {

    bool state;
    s32 i,j= 0;
    Score score;
    Score tmpscore;
    s32 visits = 0;
    s32 tmpvisits = 0;

    Move bestmove = 0;
    double start, end;

    PLYPLAYED++;
    
    NODECOUNT   = 0;
    TNODECOUNT  = 0;
    ABNODECOUNT = 0;
    MOVECOUNT   = 0;
    NODECOPIES  = 0;
    MEMORYFULL  = 0;

    start = get_time(); 

    // prepare root node
    BOARD_STACK_TOP = 1;
    NODES[0].move                =  lastmove;
    NODES[0].score               = -INF;
    NODES[0].visits              =  0;
    NODES[0].children            = -1;
    NODES[0].parent              = -1;
    NODES[0].child               = -1;
    NODES[0].lock                =  0; // assign root node to process 0   

    // init board
    memcpy(GLOBAL_INIT_BOARD, board, 5* sizeof(Bitboard));
    // reset counters
    if (COUNTERS)
      free(COUNTERS);
    COUNTERS = (u64*)calloc(totalThreads*10, sizeof(u64));
    // prepare hash history
    for(i=0;i<totalThreads;i++)
    {
      memcpy(&GLOBAL_HASHHISTORY[i*1024], HashHistory, 1024* sizeof(Hash));
    }

    // call GPU functions
/*
    state = cl_init_device();
    // something went wrong...
    if (!state)
    {
      free_resources();
      exit(EXIT_FAILURE);
    }
*/
    state = cl_init_objects();
    // something went wrong...
    if (!state)
    {
      free_resources();
      exit(EXIT_FAILURE);
    }
    state = cl_run_kernel(stm, depth);
    // something went wrong...
    if (!state)
    {
      free_resources();
      exit(EXIT_FAILURE);
    }
    state = cl_get_and_release_memory();
    // something went wrong...
    if (!state)
    {
      free_resources();
      exit(EXIT_FAILURE);
    }
/*
    state = cl_release_device();
    // something went wrong...
    if (!state)
    {
      free_resources();
      exit(EXIT_FAILURE);
    }
*/
    // get best move from tree copied from cl device
    score = -2147483647;
    for(i=0; i < NODES[0].children; i++)
    {
        j = NODES[0].child + i;
        tmpscore = -NODES[j].score;
        tmpvisits = NODES[j].visits;
/*
        FILE 	*Stats;
        Stats = fopen("zeta.debug", "ab+");
        fprintf(Stats, "#node: %d, score:%f \n", j,(float)tmpscore/1000);
        fclose(Stats);
*/
        if (ISINF(tmpscore)) // skip illegal
            continue;
        if (tmpscore > score || (tmpscore == score && tmpvisits > visits))
        {
            score = tmpscore;
            visits = tmpvisits;
            // collect bestmove
            bestmove = NODES[j].move&0x000000003FFFFFFF;
        }
    }
    bestscore = score;
    // single reply
    if (NODES[0].children == 1) {
        bestscore = NODES[NODES[0].child].score;
        bestmove = NODES[NODES[0].child].move&0x000000003FFFFFFF;
    }
    Bestmove = bestmove;
    // collect counters
    for (i=0; i < totalThreads; i++) {
        NODECOUNT+=     COUNTERS[i*10+0];
        TNODECOUNT+=    COUNTERS[i*10+1];
        ABNODECOUNT+=   COUNTERS[i*10+2];
    }
//    bestscore = (s32)COUNTERS[totalThreads*4+0];
    MOVECOUNT = COUNTERS[3];
    plyreached = COUNTERS[5];
    MEMORYFULL = COUNTERS[6];
    bestmoveply = COUNTERS[7];
    // timers
    end = get_time();
    Elapsed = end-start;
    Elapsed/=1000;
    // compute next nps value
    nps_current =  (s32 )(ABNODECOUNT/(Elapsed));
    nodes_per_second+= (ABNODECOUNT > (u64)nodes_per_second)? (nps_current > nodes_per_second)? (nps_current-nodes_per_second)*0.66 : (nps_current-nodes_per_second)*0.33 :0;
    // print xboard output
    if (post_mode == true || xboard_mode == false) {
        if ( xboard_mode == false )
            printf("depth score time nodes bfdepth pv \n");
        printf("%i %i %i %" PRIu64 " %i 	", bestmoveply, bestscore/10, (s32 )(Elapsed*100), ABNODECOUNT, plyreached);          
        print_movealg(bestmove);
        printf("\n");
    }
    print_stats();
    fflush(stdout);        
    return bestmove;
}
// run an benchmark for current set up
s32 benchmark(Bitboard *board, bool stm, s32 depth, Move lastmove)
{
    bool state;
    s32 i,j= 0;
    Score score = -2147483647;
    Score tmpscore;
    s32 tmpvisits = 0;
    s32 visits = 0;

    Move bestmove = 0;
    double start, end;

    PLYPLAYED++;
    
    NODECOUNT   = 0;
    TNODECOUNT  = 0;
    ABNODECOUNT = 0;
    MOVECOUNT   = 0;
    NODECOPIES  = 0;

    start = get_time();

    // prepare root node
    BOARD_STACK_TOP = 1;
    NODES[0].move                =  lastmove;
    NODES[0].score               = -INF;
    NODES[0].visits              =  0;
    NODES[0].children            = -1;
    NODES[0].parent              = -1;
    NODES[0].child               = -1;
    NODES[0].lock                =  0; // assign root node to process 0   

    // init board
    memcpy(GLOBAL_INIT_BOARD, board, 5* sizeof(Bitboard));
    // reset counters
    if (COUNTERS)
      free(COUNTERS);
    COUNTERS = (u64*)calloc(totalThreads*10, sizeof(u64));
    // prepare hash history
    for(i=0;i<totalThreads;i++)
    {
      memcpy(&GLOBAL_HASHHISTORY[i*1024], HashHistory, 1024* sizeof(Hash));
    }

    // call GPU functions
/*
    state = cl_init_device();
    if (!state)
        return -1;
*/
    state = cl_init_objects();
    // something went wrong...
    if (!state)
    {
      return -1;
    }
    state = cl_run_kernel(stm, depth);
    // something went wrong...
    if (!state)
    {
      return -1;
    }
    // timers
    end = get_time();
    Elapsed = end-start;
    Elapsed/=1000;

    state= cl_get_and_release_memory();
    if (!state)
        return -1;
/*
    state = cl_release_device();
    if (!state)
        return -1;
*/
    // get best move from tree copied from cl device
    score = -2147483647;
    for(i=0; i < NODES[0].children; i++)
    {
        j = NODES[0].child + i;
        tmpscore = -NODES[j].score;
        tmpvisits = NODES[j].visits;
/*
        FILE 	*Stats;
        Stats = fopen("zeta.debug", "ab+");
        fprintf(Stats, "#node: %d, score:%f \n", j,(float)tmpscore/1000);
        fclose(Stats);
*/
        if (ISINF(tmpscore)) // skip illegal
            continue;
        if (tmpscore > score || (tmpscore == score && tmpvisits > visits))
        {
            score = tmpscore;
            visits = tmpvisits;
            // collect bestmove
            bestmove = NODES[j].move&0x000000003FFFFFFF;
        }
    }
    bestscore = score;
    // single reply
    if (NODES[0].children == 1) {
        bestscore = NODES[NODES[0].child].score;
        bestmove = NODES[NODES[0].child].move&0x000000003FFFFFFF;
    }
    Bestmove = bestmove;
    // collect counters
    for (i=0; i < totalThreads; i++) {
        NODECOUNT+=     COUNTERS[i*10+0];
        TNODECOUNT+=    COUNTERS[i*10+1];
        ABNODECOUNT+=   COUNTERS[i*10+2];
    }
//    bestscore = (s32)COUNTERS[totalThreads*4+0];
    MOVECOUNT = COUNTERS[3];
    plyreached = COUNTERS[5];
    MEMORYFULL = COUNTERS[6];
    bestmoveply = COUNTERS[7];
    // print cli output
    printf("depth: %i, nodes %" PRIu64 ", nps: %i, time: %lf sec, score: %i ", plyreached, ABNODECOUNT, (int)(ABNODECOUNT/Elapsed), Elapsed, bestscore);          
    printf(" move ");
    print_movealg(bestmove);
    printf("\n");
    print_stats();
    fflush(stdout);        
    return 0;
}
// get nodes per second for temp config and specified position
s32 benchmarkNPS(s32 benchsec)
{
    bool state;
    s32 bench = 0;

    PLY =0;
    // read temp config created by clconfig
    read_config("config.tmp");
    inits();

    state = cl_init_device();
    // something went wrong...
    if (!state)
    {
      free_resources();
      return -1;
    }
//    setboard((char *)"setboard r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq");
//    setboard((char *)"setboard r3kb1r/pbpp1ppp/1p2Pn2/7q/2P1PB2/2Nn2P1/PP2NP1P/R2QK2R b KQkq -");
    setboard((char *)"setboard 1rbqk2r/1p3p1p/p3pbp1/2N1n3/5Q2/2P1B1P1/P3PPBP/3R1RK1 b k -");

    print_board(BOARD);
    Elapsed = 0;
    max_nodes = 8192; // search 8k nodes
    while (Elapsed <= benchsec) {
        if (Elapsed *2 >= benchsec)
            break;
        PLY = 0;
        bench = benchmark(BOARD, STM, max_leaf_depth, Lastmove);                
        if (bench != 0 )
            break;
        if (MEMORYFULL == 1) {
		    printf("#> Lack of Device Memory\n\n");
            break;
        }
        max_nodes*=2; // search double the nodes for next iteration
        setboard((char *)"setboard 1rbqk2r/1p3p1p/p3pbp1/2N1n3/5Q2/2P1B1P1/P3PPBP/3R1RK1 b k -");
    }
    free_resources();
    if (Elapsed <= 0 || ABNODECOUNT <= 0)
        return -1;
    return (ABNODECOUNT/(Elapsed));
}



/* ############################# */
/* ### main loop, and Xboard ### */
/* ############################# */
int main(void) {

  bool state;
  char line[256];
  char command[256];
  char c_usermove[256];
  s32 go = false;
  Move move;
  Move usermove;
  s32 status = 0;
  s32 benchsec = 0;
  char configfile[256] = "config.ini";
    
  /* print engine info to console */
  fprintf(stdout,"Zeta %s\n",VERSION);
  fprintf(stdout,"Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");

    // load zeta.cl
    status = load_file_to_string(filename, &source);
    if (status <= 0) {
        printf("zeta.cl file missing\n");
        exit(0);
    }
    else
        sourceSize    = strlen(source);


    // xboard command loop
    for(;;) {

        if (!xboard_mode) 
            printf("#> ");   

        fflush(stdout);

		if (!fgets(line, 256, stdin)) {
			continue;
        }
		if (line[0] == '\n') {
        }

		sscanf(line, "%s", command);

		if (!strcmp(command, "xboard")) {			
            xboard_mode = true;
            printf("\n");
            fflush(stdout);
            continue;
        }


        if (strstr(command, "help")) {
            
		    printf("#> Options:\n\n");
		    printf("#> help                 -> will show help\n");
		    printf("#> memory 512           -> machine will try to use max 512 MB for guessconfig\n");
		    printf("#> memory_slots 2       -> machine use memory*2 for guessconfig \n");
		    printf("#> guessconfig          -> will guess minimal config for CPU|GPU device\n");
		    printf("#> guessconfigx         -> will guess best config for CPU|GPU device\n");
		    printf("#> test combined.epd 2  -> will run epd test with 2 sec per move, evaluates only best move option\n");
		    printf("#> bench 10 config.ini  -> will run benchmark with 10 sec and config.ini as config file\n");
		    printf("#> new                  -> init machine for new game\n");
		    printf("#> level 40 5           -> set time control to 5 min for 40 moves\n");
		    printf("#> go                   -> let machhine play\n");
		    printf("#> usermove d7d5        -> play given move\n");
		    printf("#> quit                 -> exit program\n\n");
        }
        if (strstr(command, "protover")) {
            printf("feature done=0\n");  
		    printf("feature myname=\"Zeta %s\"\n", VERSION);
		    printf("feature variants=\"normal\"\n");
            printf("feature reuse=0\n");
            printf("feature analyze=0\n");
            printf("feature setboard=1\n");
            printf("feature memory=01\n");
            printf("feature smp=0\n");
            printf("feature usermove=1\n");
            printf("feature san=0\n");
            printf("feature time=1\n");
            printf("feature sigint=0\n");
            printf("feature debug=0\n");
            printf("feature done=1\n");
		    continue;
        }        

  	if (!strcmp(command, "bench")) {

            benchsec = 10;
			sscanf(line, "bench %i %s", &benchsec, configfile);

            PLY =0;
            read_config(configfile);
            inits();
            state = cl_init_device();
            // something went wrong...
            if (!state)
            {
              free_resources();
              exit(EXIT_FAILURE);
            }
            setboard((char *)"setboard rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            printf("\n#>Benchmarking Position 1/3\n");
            printf("#>rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\n");
            print_board(BOARD);
            Elapsed = 0;
            max_nodes = 8192;
            while (Elapsed <= benchsec) {

                if (Elapsed *2 >= benchsec)
                    break;
                PLY = 0;
                status = benchmark(BOARD, STM, max_leaf_depth, Lastmove);                
                if (status != 0)
                    break;
                if (MEMORYFULL == 1) {
        		    printf("#> Lack of Device Memory, try to set memory_slots to 2\n\n");
                    break;
                }
                max_nodes*=2;
                setboard((char *)"setboard rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            }
            free_resources();


            PLY =0;
            read_config(configfile);
            inits();
            state = cl_init_device();
            // something went wrong...
            if (!state)
            {
              free_resources();
              exit(EXIT_FAILURE);
            }
            setboard((char *)"setboard r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq");
            printf("\n#>Benchmarking Position 2/3\n");
            printf("#>r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq\n");
            print_board(BOARD);
            Elapsed = 0;
            max_nodes = 8192;
            while (Elapsed <= benchsec) {

                if (Elapsed *2 >= benchsec)
                    break;
                PLY = 0;
                status = benchmark(BOARD, STM, max_leaf_depth, Lastmove);                
                if (status != 0)
                    break;
                if (MEMORYFULL == 1) {
        		    printf("#> Lack of Device Memory, try to set memory_slots to 2\n\n");
                    break;
                }
                max_nodes*=2;
                setboard((char *)"setboard r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq");
            }
            free_resources();

            PLY =0;
            read_config(configfile);
            inits();
            state = cl_init_device();
            // something went wrong...
            if (!state)
            {
              free_resources();
              exit(EXIT_FAILURE);
            }
            setboard((char *)"setboard 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
            printf("\n#>Benchmarking Position 3/3\n");
            printf("#>8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -\n");
            print_board(BOARD);
            Elapsed = 0;
            max_nodes = 8192;
            while (Elapsed <= benchsec) {

                if (Elapsed *2 >= benchsec)
                    break;
                PLY = 0;
                status = benchmark(BOARD, STM, max_leaf_depth, Lastmove);                
                if (status != 0)
                    break;
                if (MEMORYFULL == 1) {
        		    printf("#> Lack of Device Memory, try to set memory_slots to 2\n\n");
                    break;
                }
                max_nodes*=2;
                setboard((char *)"setboard 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
            }
            free_resources();

			continue;
		}

		if (!strcmp(command, "new")) {
            PLY =0;
            PLYPLAYED = 0;
            if (NODES!=NULL)
                free_resources();
            read_config(configfile);
            inits();
            state = cl_init_device();
            // something went wrong...
            if (!state)
            {
              free_resources();
              exit(EXIT_FAILURE);
            }
            setboard((char *)"setboard rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            Bestmove = 0;
            go = false;
            force_mode = false;
            random_mode = false;
            if (xboard_mode == false)
                print_board(BOARD);
			continue;
		}
		if (!strcmp(command, "setboard")) {
            setboard(line);
            if (xboard_mode == false)
                print_board(BOARD);
			continue;
		}
		if (!strcmp(command, "quit")) {
            break;
        }
		if (!strcmp(command, "force")) {
            force_mode = true;
			continue;
		}
		if (!strcmp(command, "hard")) {
			continue;
		}
		if (!strcmp(command, "random")) {
			continue;
		}
		if (!strcmp(command, "post")) {
            post_mode = true;
			continue;
		}
		if (!strcmp(command, "nopost")) {
            post_mode = false;
			continue;
		}
		if (!strcmp(command, "debug")) {
            post_mode = true;
			continue;
		}
		if (!strcmp(command, "white")) {
			continue;
		}
		if (!strcmp(command, "black")) {
			continue;
		}
		if (!strcmp(command, "sd")) {
			sscanf(line, "sd %i", &search_depth);
      search_depth = (search_depth >= max_depth) ? max_depth : search_depth;
			continue;
		}
		if (!strcmp(command, "memory")) {
			sscanf(line, "memory %" PRIu64 , &max_mem_mb);
			continue;
		}
		if (!strcmp(command, "memory_slots")) {
			sscanf(line, "memory_slots %i", &memory_slots);
			continue;
		}
		if (!strcmp(command, "cores")) {
			sscanf(line, "cores %i", &max_cores);
			continue;
		}

    if (!strcmp (command, "st")) {
      sscanf(line, "st %lf", &time_left_computer); 
        if (time_management == true) {
	          max_time_per_move = time_left_computer;
            max_time_per_move = (max_time_per_move < 1)? 1 : max_time_per_move;
            max_nps_per_move = nodes_per_second*max_time_per_move;
            max_nodes = nodes_per_second*max_time_per_move;
        }
        else {
            max_nps_per_move = max_nodes;
        }
      continue;
    }    


        if (!strcmp (command, "time")) {
	        sscanf(line, "time %lf", &time_left_computer); 
            if (time_management == true) {
    	        max_time_per_move = ( time_left_computer / ((max_moves-(PLYPLAYED%max_moves))+1)  ) /100;
                max_time_per_move = (max_time_per_move < 1)? 1 : max_time_per_move;
                max_nps_per_move = nodes_per_second*max_time_per_move;
                max_nodes = nodes_per_second*max_time_per_move;
            }
            else {
                max_nps_per_move = max_nodes;
            }
	        continue;
        }    
        if (!strcmp(command, "otim")) { 
	        sscanf(line, "otim %lf", &time_left_opponent); 
	        continue;
        }	
        if (!strcmp (command, "level")){
            sscanf(line, "level %i %s", &max_moves, time_string);
            if (!strcmp (time_string, ":")) {
                sscanf(time_string, "%i:%i", &time_minutes, &time_seconds);
            }
            else {
                sscanf(time_string, "%i", &time_minutes);
                time_seconds = 0;
            }
            if (time_management == true) {
                max_moves = (max_moves == 0)? 40 : max_moves;
	            max_time_per_move = (((time_minutes*60) + time_seconds) / max_moves);
	            time_per_move = (((time_minutes*60) + time_seconds) / max_moves);
                max_nps_per_move = nodes_per_second*max_time_per_move;
                max_nodes = max_nps_per_move; 
            }
            else {
                max_nps_per_move = max_nodes;
            }

            continue;
        }



		if (!strcmp(command, "go")) {

            force_mode = false;
            go = true;

            PLYPLAYED = (int)(PLY/2);

            move = rootsearch(BOARD, STM, max_leaf_depth, Lastmove);            
            PLY++;

            CR = (Lastmove>>36)&0xF;
            Lastmove = move;
            domove(BOARD, Lastmove);
            Lastmove = updateCR(Lastmove, CR);
            HashHistory[PLY] = BOARD[4];
            MoveHistory[PLY] = Lastmove;
            
//            if (xboard_mode == false)
                print_board(BOARD);

            printf("move ");
            print_movealg(move);
            printf("\n");
            STM = !STM;

            continue;
		}
        if (!strcmp(command, "usermove")){

            sscanf(line, "usermove %s", c_usermove);
            usermove = move_parser(c_usermove, BOARD, STM);

            PLY++;
            CR = (Lastmove>>36)&0xF;
            Lastmove = usermove;
            domove(BOARD, Lastmove);
            Lastmove = updateCR(Lastmove, CR);
            HashHistory[PLY] = BOARD[4];
            MoveHistory[PLY] = Lastmove;

//            if (xboard_mode == false)
                print_board(BOARD);

            STM = !STM;
            if ( force_mode == false && go == true) {
                go = true;
                move = rootsearch(BOARD, STM, max_leaf_depth, Lastmove);
                PLY++;
                CR = (Lastmove>>36)&0xF;
                Lastmove = move;
                domove(BOARD, Lastmove);
                Lastmove = updateCR(Lastmove, CR);
                HashHistory[PLY] = BOARD[4];
                MoveHistory[PLY] = Lastmove;

//                if (xboard_mode == false)
                    print_board(BOARD);

                printf("move ");
                print_movealg(move);
                printf("\n");
                STM = !STM;
            }
            continue;
        }
		if (!strcmp(command, "guessconfig")) {
            cl_guess_config(false);
            continue;
		}
		if (!strcmp(command, "guessconfigx")) {
            cl_guess_config(true);
            continue;
		}
    }

    // free memory
    if (NODES!=NULL)
        free_resources();

    return 0;
}


/* ############################# */
/* ###        parser         ### */
/* ############################# */

void move2alg(Move move, char * movec) {

    char rankc[8] = "12345678";
    char filec[8] = "abcdefgh";
    Square from = getfrom(move);
    Square to   = getto(move);
    Piece pfrom   = getpfrom(move);
    Piece pto   = getpto(move);


    movec[0] = filec[GETFILE(from)];
    movec[1] = rankc[GETRANK(from)];
    movec[2] = filec[GETFILE(to)];
    movec[3] = rankc[GETRANK(to)];
    movec[4] = ' ';
    movec[5] = '\0';

    /* pawn promo */
    if ( (pfrom == PAWN && ( ( (to&56)/8 == 7) || ((to&56)/8 == 0) ) )) {
        if (pto == QUEEN)
            movec[4] = 'q';
        if (pto == ROOK)
            movec[4] = 'r';
        if (pto == BISHOP)
            movec[4] = 'b';
        if (pto == KNIGHT)
            movec[4] = 'n';
    }

}


Move move_parser(char *usermove, Bitboard *board, bool stm) {

    s32 file;
    s32 rank;
    Square from,to,cpt;
    Piece pto, pfrom, pcpt;
    Move move;
    char promopiece;
    Square pdsq = 0;

    file = (int)usermove[0] -97;
    rank = (int)usermove[1] -49;
    from = MAKESQ(file,rank);
    file = (int)usermove[2] -97;
    rank = (int)usermove[3] -49;
    to = MAKESQ(file,rank);

    pfrom = getPiece(board, from);
    pto = pfrom;
    cpt = to;
    pcpt = getPiece(board, cpt);

    // en passant
    cpt = ( (pfrom>>1) == PAWN && (stm == WHITE) && (from>>3) == 4 && to-from != 8 && (pcpt>>1) == PNONE ) ? to-8 : cpt;
    cpt = ( (pfrom>>1) == PAWN && (stm == BLACK) && (from>>3) == 3 && from-to != 8 && (pcpt>>1) == PNONE ) ? to+8 : cpt;

    pcpt = getPiece(board, cpt);
    
    // pawn double square flag
    if ( (pfrom>>1) == PAWN && abs(to-from)==16 ) {
        pdsq = to;
    }
    // pawn promo piece
    promopiece = usermove[4];
    if (promopiece == 'q' || promopiece == 'Q' )
        pto = QUEEN<<1 | (Piece)(stm&0x1);
    else if (promopiece == 'n' || promopiece == 'N' )
        pto = KNIGHT<<1 | (Piece)(stm&0x1);
    else if (promopiece == 'b' || promopiece == 'B' )
        pto = BISHOP<<1 | (Piece)(stm&0x1);
    else if (promopiece == 'r' || promopiece == 'R' )
        pto = ROOK<<1 | (Piece)(stm&0x1);

    move = makemove(from, to, cpt, pfrom, pto , pcpt, pdsq);

    return move;
}

void setboard(char *fen) {

    s32 i, j, side;
    s32 index;
    s32 file = 0;
    s32 rank = 7;
    s32 pos  = 0;
    char temp;
    char position[255];
    char csom[1];
    char castle[5];
    char castlechar;
    char ep[3];
    s32 bla;
    s32 blubb;
    Cr cr = 0;
    char string[] = {" PNKBRQ pnkbrq/12345678"};

	sscanf(fen, "setboard %s %c %s %s %i %i", position, csom, castle, ep, &bla, &blubb);


    BOARD[0] = 0;
    BOARD[1] = 0;
    BOARD[2] = 0;
    BOARD[3] = 0;
    BOARD[4] = 0;

    i =0;
    while (!(rank==0 && file==8)) {
        temp = position[i];
        i++;        
        for (j=0;j<24;j++) {
    		if (temp == string[j]) {
                if (j == 14) {
                    rank--;
                    file=0;
                }
                else if (j >=15) {
                    file+=j-14;
                }
                else {
                    pos = (rank*8) + file;
                    side = (j>6) ? 1 :0;
                    index = side? j-7 : j;
                    BOARD[0] |= (side == BLACK)? ((Bitboard)1<<pos) : 0;
                    BOARD[1] |= ( (Bitboard)(index&1)<<pos);
                    BOARD[2] |= ( ( (Bitboard)(index>>1)&1)<<pos);
                    BOARD[3] |= ( ( (Bitboard)(index>>2)&1)<<pos);
                    file++;
                }
                break;                
            }
        }
    }

    // site on move
    if (csom[0] == 'w' || csom[0] == 'W') {
        STM = WHITE;
    }
    if (csom[0] == 'b' || csom[0] == 'B') {
        STM = BLACK;
    }

    // Castle Rights
    cr = 0;
    i = 0;
    castlechar = castle[0];
    if (castlechar != '-') {

        while (castlechar != '\0') {
    
            // white queenside
            if (castlechar == 'Q')
                cr |= 1;
            // white kingside
            if (castlechar == 'K')
                cr |= 1<<1;
            // black queenside
            if (castlechar == 'q')
                cr |= 1<<2;
            // black kingside
            if (castlechar == 'k')
                cr |= 1<<3;

            i++;            
            castlechar = castle[i];
        }
    }
    CR = cr;

    // TODO: set en passant flag
    Lastmove  = 0;
    Lastmove |= ((Move)cr)<<36;

    // set hash
    BOARD[4] = computeHash(BOARD, STM);
    HashHistory[PLY] = BOARD[4];

}

/* ############################# */
/* ###       print tools     ### */
/* ############################# */

void print_movealg(Move move) {

    char rankc[] = "12345678";
    char filec[] = "abcdefgh";
    char movec[6] = "";
    Square from = getfrom(move);
    Square to   = getto(move);
    Piece pfrom   = getpfrom(move);
    Piece pto   = getpto(move);


    movec[0] = filec[GETFILE(from)];
    movec[1] = rankc[GETRANK(from)];
    movec[2] = filec[GETFILE(to)];
    movec[3] = rankc[GETRANK(to)];

    /* pawn promo */
    if ( pfrom>>1 == PAWN && ( to>>3 == 7 || to>>3 == 0 ) ) {
        if (pto>>1 == QUEEN)
            movec[4] = 'q';
        if (pto>>1 == ROOK)
            movec[4] = 'r';
        if (pto>>1 == BISHOP)
            movec[4] = 'b';
        if (pto>>1 == KNIGHT)
            movec[4] = 'n';
    }
    movec[5] = '\0';
    

    printf("%s", movec);
    fflush(stdout);

}

void print_bitboard(Bitboard board) {

    s32 i,j,pos;
    printf("###ABCDEFGH###\n");
   
    for(i=8;i>0;i--) {
        printf("#%i ",i);
        for(j=0;j<8;j++) {
            pos = ((i-1)*8) + j;
            if (board&SETMASKBB(pos)) 
                printf("x");
            else 
                printf("-");

        }
       printf("\n");
    }
    printf("###ABCDEFGH###\n");
    fflush(stdout);
}

void print_board(Bitboard *board) {

    s32 i,j,pos;
    Piece piece = PNONE;
    char wpchars[] = "-PNKBRQ";
    char bpchars[] = "-pnkbrq";

//    print_bitboard(board[0]);
//    print_bitboard(board[1]);
//    print_bitboard(board[2]);
//    print_bitboard(board[3]);

    printf("###ABCDEFGH###\n");

    for(i=8;i>0;i--) {
        printf("#%i ",i);
        for(j=0;j<8;j++) {
            pos = ((i-1)*8) + j;

            piece = getPiece(board, pos);

            if (piece != PNONE && (piece&BLACK))
                printf("%c", bpchars[piece>>1]);
            else if (piece != PNONE)
                printf("%c", wpchars[piece>>1]);
            else 
                printf("-");
       }
       printf("\n");
    }
    fflush(stdout);
}



void print_stats() {
    FILE 	*Stats;
    Stats = fopen("zeta.debug", "ab+");
    fprintf(Stats, "iterations: %" PRIu64 " ,Expaned Nodes: %" PRIu64 " , MemoryFull: %" PRIu64 ", AB-Nodes: %" PRIu64 " , Movecount: %" PRIu64 " , Node Copies: %i, bestmove: %" PRIu64 ", depth: %i, Score: %i, ScoreDepth: %i,  sec: %f \n", NODECOUNT, TNODECOUNT, MEMORYFULL, ABNODECOUNT, MOVECOUNT, NODECOPIES, Bestmove, plyreached, bestscore/10, bestmoveply, Elapsed);
    fclose(Stats);
}


void read_config(char configfile[]) {
    FILE 	*fcfg;
    char line[256];

    fcfg = fopen(configfile, "r");

    if (fcfg == NULL) {
        printf("%s file missing\n", configfile);
        exit(0);
    }


    while (fgets(line, sizeof(line), fcfg)) { 

        sscanf(line, "threadsX: %i;", &threadsX);
        sscanf(line, "threadsY: %i;", &threadsY);
        sscanf(line, "threadsZ: %i;", &threadsZ);
        sscanf(line, "nodes_per_second: %i;", &nodes_per_second);
        sscanf(line, "max_nodes: %" PRIu64 ";", &max_nodes);
        sscanf(line, "max_nodes_to_expand: %i;", &max_nodes_to_expand);
        sscanf(line, "memory_slots: %i;", &memory_slots);
        sscanf(line, "max_depth: %i;", &max_depth);
        sscanf(line, "max_leaf_depth: %i;", &max_leaf_depth);
        sscanf(line, "opencl_platform_id: %i;", &opencl_platform_id);
        sscanf(line, "opencl_device_id: %i;", &opencl_device_id);

    }


/*
    FILE 	*Stats;
    Stats = fopen("zeta.debug", "ab+");
    fprintf(Stats, "max_nodes_to_expand: %i ", max_nodes_to_expand);
    fclose(Stats);
*/


    if (max_nodes == 0) {
        time_management = true;
        max_nodes = nodes_per_second; 
    }
    else
        max_nps_per_move = max_nodes;


    totalThreads = threadsX*threadsY*threadsZ;

    // allocate memory
    GLOBAL_INIT_BOARD = (Bitboard*)malloc(  5 * sizeof (Bitboard));

    NODES = (NodeBlock*)calloc((MAXMOVES+1), sizeof(NodeBlock));
    if (NODES == NULL) {
        printf("memory alloc failed\n");
        free_resources();
        exit(0);
    }

    COUNTERS = (u64*)calloc(10*totalThreads, sizeof(u64));

    if (COUNTERS == NULL) {
        printf("memory alloc failed\n");
        free_resources();
        exit(0);
    }

    GLOBAL_HASHHISTORY = (Hash*)malloc((totalThreads*1024) * sizeof (Hash));

    if (GLOBAL_HASHHISTORY == NULL) {
        printf("memory alloc failed\n");
        free_resources();
        exit(0);
    }


    fclose(fcfg);
}
int load_file_to_string(const char *filename, char **result) 
{ 
	unsigned int size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL) 
	{ 
		*result = NULL;
		return -1;
	} 
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (char *)malloc(size+1);
	if (size != fread(*result, sizeof(char), size, f)) 
	{ 
		free(*result);
		return -2;
	} 
	fclose(f);
	(*result)[size] = 0;
	return size;
}

