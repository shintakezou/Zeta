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

#include <stdio.h>      /* for print and scan */
#include <stdlib.h>     /* for malloc free */
#include <string.h>     /* for string compare */ 
#include <getopt.h>     /* for getopt_long */

#include "bitboard.h"   // bit functions
#include "timer.h"
#include "types.h"
#include "zobrist.h"

/* global variables */
FILE *LogFile = NULL;         /* logfile for debug */
char *Line;                   /* for fgetting the input on stdin */
char *Command;                /* for pasring the xboard command */
char *Fen;                    /* for storing the fen chess baord string */
const char filename[]  = "zeta.cl";
char *source;
size_t sourceSize;
/* counters */
u64 MOVECOUNT = 0;
u64 NODECOUNT = 0;
u64 TNODECOUNT = 0;
u64 ABNODECOUNT = 0;
s32 NODECOPIES = 0;
u64 MEMORYFULL = 0;
// config file
s32 threadsX            =  0;
s32 threadsY            =  0;
s32 threadsZ            =  0;
s32 totalThreads        =  0;
s32 nodes_per_second    =  0;
u64 max_nodes           =  0;
s32 nps_current         =  0;
s32 max_memory          =  0;
s32 max_nodes_to_expand =  0;
s32 memory_slots        =  1;
s32 max_ab_depth        =  0;
s32 max_depth           = 99;
s32 opencl_device_id    =  0;
s32 opencl_platform_id  =  0;
// further config
s32 max_nps_per_move= 0;
s32 search_depth    = 0;
u64 max_mem_mb      = MAXDEVICEMB;
s32 max_cores       = 1;
s32 time_management = false;
/* xboard flags */
bool xboard_mode    = false;  /* chess GUI sets to true */
bool xboard_force   = false;  /* if true aplly only moves, do not think */
bool xboard_post    = false;  /* post search thinking output */
bool xboard_san     = false;  /* use san move notation instead of can */
bool xboard_time    = false;  /* use xboards time command for time management */
bool xboard_debug   = false;  /* print debug information */
s32 xboardmb        = 64;     /* mega bytes for hash table */
/* timers */
double start        = 0;
double end          = 0;
double elapsed      = 0;
bool TIMEOUT        = false;  /* global value for time control*/
/* time control in milli-seconds */
s32 timemode    = 0;      /* 0 = single move, 1 = conventional clock, 2 = ics clock */
s32 MovesLeft   = 1;      /* moves left unit nex time increase */
s32 MaxMoves    = 1;      /* moves to play in time frame */
double TimeInc  = 0;      /* time increase */
double TimeBase = 5*1000; /* time base for conventional inc, 5s default */
double TimeLeft = 5*1000; /* overall time on clock, 5s default */
double MaxTime  = 5*1000; /* max time per move */
s64 MaxNodes    = 1;
/* game state */
bool STM        = WHITE;  /* site to move */
s32 SD          = 0; /* max search depth*/
s32 GAMEPLY     = 0;      /* total ply, considering depth via fen string */
s32 PLY         = 0;      /* engine specifix ply counter */
Move *MoveHistory;
Hash *HashHistory;
// time management
/*
static double max_time_per_move = 0;
static double time_per_move     = 0;
static s32 max_moves            = 0;
static s32 time_seconds         = 0;
static s32 time_minutes         = 0;
static double time_left_opponent = 0;  
static double time_left_computer = 0;  
static char time_string[128];
static s32 max_nps_per_move     = 0;
*/
double Elapsed;
Score bestscore = 0;
s32 plyreached = 0;
s32 bestmoveply = 0;
/* Quad Bitboard */
/* based on http://chessprogramming.wikispaces.com/Quad-Bitboards */
/* by Gerd Isenberg */
Bitboard BOARD[6];
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  4   hash
  5   lastmove
*/
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
static void print_help(void);
static void print_version(void);
static void selftest(void);
Move can2move(char *usermove, Bitboard *board, bool stm);
static bool setboard(Bitboard *board, char *fen);
void move2can(Move move, char * movec);
void print_movealg(Move move);
void print_bitboard(Bitboard board);
void printboard(Bitboard *board);
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
Square getkingpos(Bitboard *board, bool side)
{
  Bitboard bbTemp = (side)?board[0]:board[0]^(board[1]|board[2]|board[3]);;
  bbTemp &= board[1]&board[2]&~board[3]; // get king
  return first1(bbTemp);
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
/* innitialize memory, files and tables */
static bool engineinits(void)
{
  /* memory allocation */
  Line         = (char *)calloc(1024       , sizeof (char));
  Command      = (char *)calloc(1024       , sizeof (char));
  Fen          = (char *)calloc(1024       , sizeof (char));
  MoveHistory = (Move *)calloc(MAXGAMEPLY , sizeof (Move));
  HashHistory = (Hash *)calloc(MAXGAMEPLY , sizeof (Hash));

  if (!Line) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Line[%d]", 1024);
    return false;
  }
  if (!Command) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Command[%d]", 1024);
    return false;
  }
  if (!Fen) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Fen[%d]", 1024);
    return false;
  }
  if (!MoveHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): u64 MoveHistory[%d]",
             MAXGAMEPLY);
    return false;
  }
  if (!HashHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): u64 HashHistory[%d]",
            MAXGAMEPLY);
    return false;
  }

  return true;
}
static bool gameinits(void)
{
  if (MoveHistory)
    free(MoveHistory);
  MoveHistory = (Move*)calloc(MAXGAMEPLY, sizeof(Move));
  if (!MoveHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): Move MoveHistory[%d]",
            MAXGAMEPLY);
    exit(EXIT_FAILURE);
  }
  if (HashHistory)
    free(HashHistory);
  HashHistory = (Hash*)calloc(MAXGAMEPLY, sizeof(Hash));
  if (!HashHistory) 
  {
    fprintf(stdout,"Error (memory allocation failed): Hash HashHistory[%d]",
            MAXGAMEPLY);
    exit(EXIT_FAILURE);
  }
  return true;
}
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
/* ############################# */
/* ###         Hash          ### */
/* ############################# */
Hash computeHash(Bitboard *board, bool stm)
{
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
  for(side=0; side<2;side++)
  {
    bbWork = bbBoth[side];
    // each piece
    while (bbWork)
    {
      pos = popfirst1(&bbWork);
      piece = GETPIECE(board, pos);
      zobrist = Zobrist[GETCOLOR(piece)*6+GETPTYPE(piece)-1];
      hash ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
    }
  }
  // TODO: add castle rights
  // TODO: add en passant
  // file en passant 
/*
  if (GETSQEP(board[QBBLAST]))
  {
    sq = GETFILE(GETSQEP(board[QBBLAST]));
    zobrist = Zobrist[16];
    hash ^= ((zobrist<<sq)|(zobrist>>(64-sq)));; // rotate left 64
  }
*/
  // site to move
  if (!stm)
    hash ^= 0x1ULL;

  return hash;    
}
/* incremental board hash update */
void updateHash(Bitboard *board, Move move)
{
  Square castlefrom   = (Square)((move>>40) & 0x7F); // is set to illegal square 64 when empty
  Square castleto     = (Square)((move>>47) & 0x7F); // is set to illegal square 64 when empty
  Bitboard castlepciece = ((move>>54) & 0xF)>>1;  // is set to 0 when PNONE
  Square pos;
  Hash zobrist;

  // from
  zobrist = Zobrist[(((move>>18)&0xF)&0x1)*6+(((move>>18)&0xF)>>1)-1];
  pos = (Square)(move&0x3F);
  board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  // to
  zobrist = Zobrist[(((move>>22)&0xF)&0x1)*6+(((move>>22)&0xF)>>1)-1];
  pos = (Square)((move>>6)&0x3F);
  board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  // capture
  if ( ((move>>26)&0xF)!=PNONE)
  {
    zobrist = Zobrist[(((move>>26)&0xF)&0x1)*6+(((move>>26)&0xF)>>1)-1];
    pos = (Square)((move>>12)&0x3F);
    board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  }
  // castle from
  zobrist = Zobrist[(((move>>18)&0xF)&0x1)*6+ROOK-1];
  if (castlefrom<ILL&&castlepciece!=PNONE )
  {
    pos = castlefrom;
    board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  }
  // castle to
  if (castleto<ILL&&castlepciece!= PNONE)
  {
    pos = castleto;
    board[4] ^= ((zobrist<<pos)|(zobrist>>(64-pos)));; // rotate left 64
  }
  // site to move
  board[4]^=0x1ULL;
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
void domove(Bitboard *board, Move move)
{
  Square from = GETSQFROM(move);
  Square to   = GETSQTO(move);
  Square cpt  = GETSQCPT(move);

  Bitboard pfrom   = GETPFROM(move);
  Bitboard pto   = GETPTO(move);

  // Castle move kingside move rook
  if (((GETPTYPE(pfrom))==KING)&&(to-from==2))
  {
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
  if ((GETPTYPE(pfrom)==KING)&&(from-to==2))
  {
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
  board[4] = computeHash(board, GETCOLOR(pfrom));
//    updateHash(board, move);
}
void undomove(Bitboard *board, Move move)
{
  Square from = GETSQFROM(move);
  Square to   = GETSQTO(move);
  Square cpt  = GETSQCPT(move);

  Bitboard pfrom = GETPFROM(move);
  Bitboard pcpt  = GETPCPT(move);

  // Castle move kingside move rook
  if ((GETPTYPE(pfrom)==KING)&&(to-from==2))
  {
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
  if ((GETPTYPE(pfrom)==KING)&&(from-to==2))
  {
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
  board[4] = computeHash(board, GETCOLOR(pfrom));
}

/* ############################# */
/* ###      root search      ### */
/* ############################# */
Score perft(Bitboard *board, bool stm, s32 depth)
{
  return 0;
}
Move rootsearch(Bitboard *board, bool stm, s32 depth)
{
  bool state;
  s32 i,j;
  Score score;
  Score tmpscore;
  s32 visits = 0;
  s32 tmpvisits = 0;

  Move bestmove = MOVENONE;
  double start, end;

  NODECOUNT   = 0;
  TNODECOUNT  = 0;
  ABNODECOUNT = 0;
  MOVECOUNT   = 0;
  NODECOPIES  = 0;
  MEMORYFULL  = 0;

  start = get_time(); 

  // prepare root node
  BOARD_STACK_TOP = 1;
  NODES[0].move                =  board[QBBLAST];
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
  // single reply
  score = -NODES[NODES[0].child].score;
  bestmove = NODES[NODES[0].child].move&0x000000003FFFFFFF;
  // get best move from tree copied from cl device
  for(i=0; i < NODES[0].children; i++)
  {
    j = NODES[0].child + i;
    tmpscore = -NODES[j].score;
    tmpvisits = NODES[j].visits;
  /*
    FILE 	*Stats;
    Stats = fopen("zeta.debug", "a");
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
  bestscore = ISINF(score)?DRAWSCORE:score;
  Bestmove = bestmove;
  // collect counters
  for (i=0; i < totalThreads; i++) {
    NODECOUNT+=     COUNTERS[i*10+0];
    TNODECOUNT+=    COUNTERS[i*10+1];
    ABNODECOUNT+=   COUNTERS[i*10+2];
  }
//  bestscore = (s32)COUNTERS[totalThreads*4+0];
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
  if (xboard_post == true || xboard_mode == false) {
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
s32 benchmark(Bitboard *board, bool stm, s32 depth)
{
  bool state;
  s32 i,j;
  Score score = -2147483647;
  Score tmpscore;
  s32 tmpvisits = 0;
  s32 visits = 0;

  Move bestmove = MOVENONE;
  double start, end;

  NODECOUNT   = 0;
  TNODECOUNT  = 0;
  ABNODECOUNT = 0;
  MOVECOUNT   = 0;
  NODECOPIES  = 0;

  start = get_time();

  // prepare root node
  BOARD_STACK_TOP = 1;
  NODES[0].move                =  board[QBBLAST];
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
  // single reply
  score = -NODES[NODES[0].child].score;
  bestmove = NODES[NODES[0].child].move&0x000000003FFFFFFF;
  // get best move from tree copied from cl device
  for(i=0; i < NODES[0].children; i++)
  {
    j = NODES[0].child + i;
    tmpscore = -NODES[j].score;
    tmpvisits = NODES[j].visits;
  /*
    FILE 	*Stats;
    Stats = fopen("zeta.debug", "a");
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
  bestscore = ISINF(score)?DRAWSCORE:score;
  Bestmove = bestmove;
  // collect counters
  for (i=0; i < totalThreads; i++) {
    NODECOUNT+=     COUNTERS[i*10+0];
    TNODECOUNT+=    COUNTERS[i*10+1];
    ABNODECOUNT+=   COUNTERS[i*10+2];
  }
//  bestscore = (s32)COUNTERS[totalThreads*4+0];
  MOVECOUNT = COUNTERS[3];
  plyreached = COUNTERS[5];
  MEMORYFULL = COUNTERS[6];
  bestmoveply = COUNTERS[7];
  // print cli output
  printf("depth: %i, nodes %" PRIu64 ", nps: %i, time: %lf sec, score: %i ", plyreached, ABNODECOUNT, (int)(ABNODECOUNT/Elapsed), Elapsed, bestscore/10);
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
  gameinits();

  state = cl_init_device();
  // something went wrong...
  if (!state)
  {
    free_resources();
    return -1;
  }
  setboard(BOARD, (char *)"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq");
//  setboard(BOARD, (char *)"r3kb1r/pbpp1ppp/1p2Pn2/7q/2P1PB2/2Nn2P1/PP2NP1P/R2QK2R b KQkq -");
//  setboard(BOARD, (char *)"1rbqk2r/1p3p1p/p3pbp1/2N1n3/5Q2/2P1B1P1/P3PPBP/3R1RK1 b k -");

  printboard(BOARD);
  Elapsed = 0;
  max_nodes = 8192; // search 8k nodes
  // run bench
  while (Elapsed <= benchsec) {
    if (Elapsed *2 >= benchsec)
      break;
    PLY = 0;
    bench = benchmark(BOARD, STM, max_ab_depth);                
    if (bench != 0 )
      break;
    if (MEMORYFULL == 1)
    {
      printf("#> Lack of Device Memory, try to set memory_slots to 2\n");
      printf("#");
      printf("#");
      break;
    }
    max_nodes*=2; // search double the nodes for next iteration
    setboard(BOARD, (char *)"1rbqk2r/1p3p1p/p3pbp1/2N1n3/5Q2/2P1B1P1/P3PPBP/3R1RK1 b k -");
  }
  free_resources();
  if (Elapsed <= 0 || ABNODECOUNT <= 0)
    return -1;

  return (ABNODECOUNT/(Elapsed));
}
/* ############################# */
/* ###        parser         ### */
/* ############################# */
void move2can(Move move, char * movec) {

    char rankc[8] = "12345678";
    char filec[8] = "abcdefgh";
    Square from = GETSQFROM(move);
    Square to   = GETSQTO(move);
    Piece pfrom   = GETPFROM(move);
    Piece pto   = GETPTO(move);


    movec[0] = filec[GETFILE(from)];
    movec[1] = rankc[GETRANK(from)];
    movec[2] = filec[GETFILE(to)];
    movec[3] = rankc[GETRANK(to)];
    movec[4] = '\0';

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
        movec[5] = '\0';
    }

}


Move can2move(char *usermove, Bitboard *board, bool stm) {

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

    pfrom = GETPIECE(board, from);
    pto = pfrom;
    cpt = to;
    pcpt = GETPIECE(board, cpt);

    // en passant
    cpt = ( (pfrom>>1) == PAWN && (stm == WHITE) && (from>>3) == 4 && to-from != 8 && (pcpt>>1) == PNONE ) ? to-8 : cpt;
    cpt = ( (pfrom>>1) == PAWN && (stm == BLACK) && (from>>3) == 3 && from-to != 8 && (pcpt>>1) == PNONE ) ? to+8 : cpt;

    pcpt = GETPIECE(board, cpt);
    
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

    move = MAKEMOVE(from, to, cpt, pfrom, pto , pcpt, pdsq);

    return move;
}

static bool setboard(Bitboard *board, char *fen) {

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

	sscanf(fen, "%s %c %s %s %i %i", position, csom, castle, ep, &bla, &blubb);


    board[0] = 0;
    board[1] = 0;
    board[2] = 0;
    board[3] = 0;
    board[4] = 0;

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
                    board[0] |= (side == BLACK)? ((Bitboard)1<<pos) : 0;
                    board[1] |= ( (Bitboard)(index&1)<<pos);
                    board[2] |= ( ( (Bitboard)(index>>1)&1)<<pos);
                    board[3] |= ( ( (Bitboard)(index>>2)&1)<<pos);
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
    board[4] = computeHash(BOARD, STM);
    HashHistory[PLY] = BOARD[4];

  return true;
}

/* ############################# */
/* ###       print tools     ### */
/* ############################# */
void printmovecan(Move move)
{
  char movec[4];
  move2can(move, movec);
  fprintf(stdout, "%s",movec);
  if (LogFile)
    fprintf(LogFile, "%s",movec);
}

void print_movealg(Move move) {

    char rankc[] = "12345678";
    char filec[] = "abcdefgh";
    char movec[6] = "";
    Square from = GETSQFROM(move);
    Square to   = GETSQTO(move);
    Piece pfrom   = GETPFROM(move);
    Piece pto   = GETPTO(move);


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

void printboard(Bitboard *board) {

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

            piece = GETPIECE(board, pos);

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
    Stats = fopen("zeta.debug", "a");
    fprintf(Stats, "iterations: %" PRIu64 " ,Expaned Nodes: %" PRIu64 " , MemoryFull: %" PRIu64 ", AB-Nodes: %" PRIu64 " , Movecount: %" PRIu64 " , Node Copies: %i, bestmove: %" PRIu64 ", depth: %i, Score: %i, ScoreDepth: %i,  sec: %f \n", NODECOUNT, TNODECOUNT, MEMORYFULL, ABNODECOUNT, MOVECOUNT, NODECOPIES, Bestmove, plyreached, bestscore/10, bestmoveply, Elapsed);
    fclose(Stats);
}


void read_config(char configfile[]) {
    FILE 	*fcfg;
    char line[256];

    fcfg = fopen(configfile, "r");

    if (fcfg == NULL) {
        printf("%s file missing\n", configfile);
        printf("try help command for options\n");
        printf("or guessconfig command to create a config.ini file\n");
        exit(0);
    }


    while (fgets(line, sizeof(line), fcfg)) { 

        sscanf(line, "threadsX: %i;", &threadsX);
        sscanf(line, "threadsY: %i;", &threadsY);
        sscanf(line, "threadsZ: %i;", &threadsZ);
        sscanf(line, "nodes_per_second: %i;", &nodes_per_second);
        sscanf(line, "max_nodes: %" PRIu64 ";", &max_nodes);
        sscanf(line, "max_memory: %d;", &max_memory);
        sscanf(line, "memory_slots: %i;", &memory_slots);
        sscanf(line, "max_depth: %i;", &max_depth);
        sscanf(line, "max_ab_depth: %i;", &max_ab_depth);
        sscanf(line, "opencl_platform_id: %i;", &opencl_platform_id);
        sscanf(line, "opencl_device_id: %i;", &opencl_device_id);

    }

    max_nodes_to_expand = max_memory*1024*1024/sizeof(NodeBlock);

/*
    FILE 	*Stats;
    Stats = fopen("zeta.debug", "a");
    fprintf(Stats, "max_nodes_to_expand: %i ", max_nodes_to_expand);
    fclose(Stats);
*/


    if (max_nodes == 0) {
        time_management = true;
        MaxNodes = nodes_per_second; 
    }
    else
        MaxNodes = max_nodes;


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
	FILE *f = fopen(filename, "r");
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
static void selftest(void)
{
}
/* print engine info to console */
static void print_version(void)
{
  fprintf(stdout,"Zeta version: %s\n",VERSION);
  fprintf(stdout,"Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");
}
/* engine options and usage */
static void print_help(void)
{
  fprintf(stdout,"Zeta, experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"You will need an config.ini file to run the engine,\n");
  fprintf(stdout,"start from command line and type guessconfig to create one.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Options:\n");
  fprintf(stdout," -l, --log          Write output/debug to file zeta.log\n");
  fprintf(stdout," -v, --version      Print Zeta version info.\n");
  fprintf(stdout," -h, --help         Print Zeta program usage help.\n");
  fprintf(stdout," -s, --selftest     Run an internal test, usefull after compile.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"To play against the engine use an CECP v2 protocol capable chess GUI\n");
  fprintf(stdout,"like Arena, Cutechess, Winboard or Xboard.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Alternatively you can use Xboard commmands directly on commmand Line,\n"); 
  fprintf(stdout,"e.g.:\n");
  fprintf(stdout,"new            // init new game from start position\n");
  fprintf(stdout,"level 40 4 0   // set time control to 40 moves in 4 minutes\n");
  fprintf(stdout,"go             // let engine play site to move\n");
  fprintf(stdout,"usermove d7d5  // let engine apply usermove in coordinate algebraic\n");
  fprintf(stdout,"               // notation and optionally start thinking\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Not supported Xboard commands:\n");
  fprintf(stdout,"analyze        // enter analyze mode\n");
  fprintf(stdout,"?              // move now\n");
  fprintf(stdout,"draw           // handle draw offers\n");
  fprintf(stdout,"hard/easy      // turn on/off pondering\n");
  fprintf(stdout,"hint           // give user a hint move\n");
  fprintf(stdout,"bk             // book Lines\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Non-Xboard commands:\n");
//  fprintf(stdout,"perft          // perform a performance test, depth set by sd command\n");
  fprintf(stdout,"selftest       // run an internal test\n");
  fprintf(stdout,"help           // print usage info\n");
  fprintf(stdout,"log            // turn log on\n");
  fprintf(stdout,"guessconfig    // guess minimal config for OpenCL devices\n");
  fprintf(stdout,"guessconfigx   // guess best config for OpenCL devices\n");
  fprintf(stdout,"\n");
}
// Zeta, experimental chess engine written in OpenCL.
int main(int argc, char* argv[])
{
  bool state;
  // config file
  char configfile[256] = "config.ini";
  /* xboard states */
  s32 xboard_protover = 0;      /* Zeta works with protocoll version >= v2 */
  /* for get opt */
  s32 c;
  static struct option long_options[] = 
  {
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {"selftest", 0, 0, 's'},
    {"log", 0, 0, 'l'},
    {NULL, 0, NULL, 0}
  };
  s32 option_index = 0;

  /* no buffers */
  setbuf (stdout, NULL);
  setbuf (stdin, NULL);

  /* turn log on */
  for (c=1;c<argc;c++)
  {
    if (!strcmp(argv[c], "-l") || !strcmp(argv[c],"--log"))
    {
      /* open/create log file */
      LogFile = fopen("zeta.log", "a");
      if (!LogFile ) 
      {
        fprintf(stdout,"Error (opening logfile zeta.log): --log");
      }
    }
  }
  /* getopt loop, parsing for help, version and logging */
  while ((c = getopt_long_only (argc, argv, "",
               long_options, &option_index)) != -1) {
    switch (option_index) 
    {
      case 0:
        print_help ();
        exit (EXIT_SUCCESS);
        break;
      case 1:
        print_version ();
        exit (EXIT_SUCCESS);
        break;
      case 2:
        selftest ();
        exit (EXIT_SUCCESS);
        break;
      case 3:
        break;
    }
  }
  /* open log file */
  if (LogFile)
  {
    /* no buffers */
    setbuf(LogFile, NULL);
    /* print binary call to log */
    fprintdate(LogFile);
    for (c=0;c<argc;c++)
    {
      fprintf(LogFile, "%s ",argv[c]);
    }
    fprintf(LogFile, "\n");
  }
  /* print engine info to console */
  fprintf(stdout,"Zeta %s\n",VERSION);
  fprintf(stdout,"Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");

  /* init memory, files and tables */
  if (!engineinits()||!gameinits())
  {
    free_resources();
    exit (EXIT_FAILURE);
  }
  /* init starting position */
  setboard(BOARD,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  // load zeta.cl
  if (load_file_to_string(filename, &source)<=0)
  {
    printf("zeta.cl file missing\n");
    exit(0);
  }
  else
    sourceSize    = strlen(source);

  /* xboard command loop */
  for (;;)
  {
    /* print cli prompt */
    if (!xboard_mode) 
      printf("#> ");   
    /* just to be sure, flush the output...*/
    fflush (stdout);
    if (LogFile)
      fflush (LogFile);
    /* get Line */
    if (!fgets (Line, 1023, stdin)) {}
    /* ignore empty Lines */
    if (Line[0] == '\n')
      continue;
    /* print io to log file */
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "%s\n",Line);
    }
    /* get command */
    sscanf (Line, "%s", Command);
    /* xboard commands */
    /* set xboard mode */
    if (!strcmp(Command, "xboard"))
    {
      fprintf(stdout,"feature done=0\n");  
      xboard_mode = true;
      continue;
    }
    /* set epd mode */
    if (!strcmp(Command, "epd"))
    {
      fprintf(stdout,"\n");
      xboard_mode = false;
      continue;
    }
    /* get xboard protocoll version */
    if (!strcmp(Command, "protover")) 
    {
      sscanf (Line, "protover %d", &xboard_protover);
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        fprintf(stdout,"Error (unsupported protocoll version,  < v2): protover\n");
        fprintf(stdout,"tellusererror (unsupported protocoll version, < v2): protover\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (unsupported protocoll version,  < v2): protover\n");
        }
      }
      else
      {
        /* send feature list to xboard */
        fprintf(stdout,"feature myname=\"Zeta%s\"\n",VERSION);
        fprintf(stdout,"feature ping=0\n");
        fprintf(stdout,"feature setboard=1\n");
        fprintf(stdout,"feature playother=0\n");
        fprintf(stdout,"feature san=0\n");
        /* check feature san accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "rejected"))
          xboard_san = true;
        fprintf(stdout,"feature usermove=1\n");
        /* check feature usermove accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "rejected"))
        {
          fprintf(stdout,"Error (unsupported feature usermove): rejected\n");
          fprintf(stdout,"tellusererror (unsupported feature usermove): rejected\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile,"Error (unsupported feature usermove): rejected\n");
          }
          free_resources();
          exit(EXIT_FAILURE);
        }
        fprintf(stdout,"feature time=1\n");
        /* check feature time accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "accepted"))
          xboard_time = true;
        fprintf(stdout,"feature draw=0\n");
        fprintf(stdout,"feature sigint=0\n");
        fprintf(stdout,"feature reuse=1\n");
        fprintf(stdout,"feature analyze=0\n");
        fprintf(stdout,"feature variants=\"normal\"\n");
        fprintf(stdout,"feature colors=0\n");
        fprintf(stdout,"feature ics=0\n");
        fprintf(stdout,"feature name=0\n");
        fprintf(stdout,"feature pause=0\n");
        fprintf(stdout,"feature nps=0\n");
        fprintf(stdout,"feature debug=1\n");
        /* check feature debug accepted  */
        if (!fgets (Line, 1023, stdin)) {}
        /* get command */
        sscanf (Line, "%s", Command);
        if (strstr(Command, "accepted"))
          xboard_debug = true;
        fprintf(stdout,"feature memory=1\n");
        fprintf(stdout,"feature smp=0\n");
        fprintf(stdout,"feature san=0\n");
        fprintf(stdout,"feature exclude=0\n");
        fprintf(stdout,"feature done=1\n");
      }
      continue;
    }
    if (!strcmp(Command, "accepted")) 
      continue;
    if (!strcmp(Command, "rejected")) 
      continue;
    /* initialize new game */
		if (!strcmp(Command, "new"))
    {
      if (!setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
      {
        fprintf(stdout,"Error (in setting start postition): new\n");        
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (in setting start postition): new\n");        
        }
      }
      SD = 0;
      free_resources();
      read_config(configfile);
      gameinits();
      state = cl_init_device();
      // something went wrong...
      if (!state)
      {
        free_resources();
        exit(EXIT_FAILURE);
      }
      if (!xboard_mode)
        printboard(BOARD);
      xboard_force  = false;
			continue;
		}
    /* set board to position in FEN */
		if (!strcmp(Command, "setboard"))
    {
      sscanf (Line, "setboard %1023[0-9a-zA-Z /-]", Fen);
      if(*Fen != '\n' && *Fen != '\0'  && !setboard (BOARD, Fen))
      {
        fprintf(stdout,"Error (in setting chess psotition via fen string): setboard\n");        
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (in setting chess psotition via fen string): setboard\n");        
        }
      }
      if (!xboard_mode)
        printboard(BOARD);
      continue;
		}
    if (!strcmp(Command, "go"))
    {
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        fprintf(stdout,"Error (unsupported protocoll version, < v2): go\n");
        fprintf(stdout,"tellusererror (unsupported protocoll version. < v2): go\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (unsupported protocoll version, < v2): go\n");
        }
      }
      else 
      {
        bool kic = false;
        Move move;
        xboard_force = false;
        NODECOUNT = 0;
        MOVECOUNT = 0;
        start = get_time();

        HashHistory[PLY] = BOARD[QBBHASH];

        /* start thinking */
        move = rootsearch(BOARD, STM, SD);

        fprintf(stdout,"move ");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"move ");
        }
        printmovecan(move);
        fprintf(stdout,"\n");
        if (LogFile)
          fprintf(LogFile,"\n");

        fflush(stdout);
        fflush(LogFile);

        domove(BOARD, move);

        end = get_time();   
        elapsed = end-start;

        if ((!xboard_mode)||xboard_debug)
        {
          printboard(BOARD);
          fprintf(stdout,"#%" PRIu64 " searched nodes in %lf seconds, nps: %" PRIu64 " \n", NODECOUNT, elapsed/1000, (u64)(NODECOUNT/(elapsed/1000)));
        }

        PLY++;
        STM = !STM;

        HashHistory[PLY] = BOARD[QBBHASH];
        MoveHistory[PLY] = move;

        /* time mngmt */
        TimeLeft-=elapsed;
        /* get moves left, one move as time spare */
        if (timemode==1)
          MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
        /* get new time inc */
        if (timemode==0)
          TimeLeft = TimeBase;
        if (timemode==2)
          TimeLeft+= TimeInc;
        /* set max time per move */
        MaxTime = TimeLeft/MovesLeft+TimeInc;
        /* get new time inc */
        if (timemode==1&&MovesLeft==2)
          TimeLeft+= TimeBase;
        /* get max nodes to search */
        MaxNodes = MaxTime/1000*nodes_per_second;
      }
      continue;
    }
    /* set xboard force mode, no thinking just apply moves */
		if (!strcmp(Command, "force"))
    {
      xboard_force = true;
      continue;
    }
    /* set time control */
		if (!strcmp(Command, "level"))
    {
      s32 sec   = 0;
      s32 min   = 0;
      TimeBase  = 0;
      TimeLeft  = 0;
      TimeInc   = 0;
      MovesLeft = 0;
      MaxMoves  = 0;

      if(sscanf(Line, "level %d %d %lf",
               &MaxMoves, &min, &TimeInc)!=3 &&
         sscanf(Line, "level %d %d:%d %lf",
               &MaxMoves, &min, &sec, &TimeInc)!=4)
           continue;

      /* from seconds to milli seconds */
      TimeBase  = 60*min + sec;
      TimeBase *= 1000;
      TimeInc  *= 1000;
      TimeLeft  = TimeBase;

      if (MaxMoves==0)
        timemode = 2; /* ics clocks */
      else
        timemode = 1; /* conventional clock mode */

      /* set moves left to 40 in sudden death or ics time control */
      if (timemode==2)
        MovesLeft = 40;

      /* get moves left */
      if (timemode==1)
        MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
      /* set max time per move */
      MaxTime = TimeLeft/MovesLeft+TimeInc;
      /* get max nodes to search */
      MaxNodes = MaxTime/1000*nodes_per_second;

      continue;
    }
    /* set time control to n seconds per move */
		if (!strcmp(Command, "st"))
    {
      sscanf(Line, "st %lf", &TimeBase);
      TimeBase *= 1000; 
      TimeLeft  = TimeBase; 
      TimeInc   = 0;
      MovesLeft = MaxMoves = 1; /* just one move*/
      timemode  = 0;
      /* set max time per move */
      MaxTime   = TimeLeft/MovesLeft+TimeInc;
      /* get max nodes to search */
      MaxNodes = MaxTime/1000*nodes_per_second;

      continue;
    }
    /* time left on clock */
		if (!strcmp(Command, "time"))
    {
      sscanf(Line, "time %lf", &TimeLeft);
      TimeLeft *= 10;  /* centi-seconds to milliseconds */
      /* get moves left, one move time spare */
      if (timemode==1)
        MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
      /* set max time per move */
      MaxTime = TimeLeft/MovesLeft+TimeInc;
      /* get max nodes to search */
      MaxNodes = MaxTime/1000*nodes_per_second;

      continue;
    }
    /* opp time left, ignore */
		if (!strcmp(Command, "otim"))
      continue;
    /* memory for hash size  */
		if (!strcmp(Command, "memory"))
    {
      sscanf(Line, "memory %d", &xboardmb);
      continue;
    }
    if (!strcmp(Command, "usermove"))
    {
      char movec[6];
      Move move;
      /* zeta supports only CECP >= v2 */
      if (xboard_mode && xboard_protover<2)
      {
        fprintf(stdout,"Error (unsupported protocoll version, < v2): usermove\n");
        fprintf(stdout,"tellusererror (unsupported protocoll version, <v2): usermove\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (unsupported protocoll version, < v2): usermove\n");
        }
      }
      /* apply given move */
      sscanf (Line, "usermove %s", movec);
      move = can2move(movec, BOARD,STM);

      domove(BOARD, move);

      PLY++;
      STM = !STM;

      HashHistory[PLY] = BOARD[QBBHASH];
      MoveHistory[PLY] = move;

      if (!xboard_mode||xboard_debug)
          printboard(BOARD);
      /* start thinking */
      if (!xboard_force)
      {
        /* start thinking */
        move = rootsearch(BOARD, STM, SD);

        fprintf(stdout,"move ");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"move ");
        }
        printmovecan(move);
        fprintf(stdout,"\n");
        if (LogFile)
          fprintf(LogFile,"\n");

        fflush(stdout);
        fflush(LogFile);

        domove(BOARD, move);

        end = get_time();   
        elapsed = end-start;

        if (!xboard_mode||xboard_debug)
        {
          printboard(BOARD);
          fprintf(stdout,"#%" PRIu64 " searched nodes in %lf seconds, nps: %" PRIu64 " \n", NODECOUNT, elapsed/1000, (u64)(NODECOUNT/(elapsed/1000)));
        }

        PLY++;
        STM = !STM;

        HashHistory[PLY] = BOARD[QBBHASH];
        MoveHistory[PLY] = move;

        /* time mngmt */
        TimeLeft-=elapsed;
        /* get moves left, one move as time spare */
        if (timemode==1)
          MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
        /* get new time inc */
        if (timemode==0)
          TimeLeft = TimeBase;
        if (timemode==2)
          TimeLeft+= TimeInc;
        /* set max time per move */
        MaxTime = TimeLeft/MovesLeft+TimeInc;
        /* get new time inc */
        if (timemode==1&&MovesLeft==2)
          TimeLeft+= TimeBase;
        /* get max nodes to search */
        MaxNodes = MaxTime/1000*nodes_per_second;
      }
      continue;
    }
    /* back up one ply */
		if (!strcmp(Command, "undo"))
    {
      if (PLY>0)
      {
        undomove(BOARD, MoveHistory[PLY]);
        PLY--;
        STM = !STM;
      }
      continue;
    }
    /* back up two plies */
		if (!strcmp(Command, "remove"))
    {
      if (PLY>=2)
      {
        undomove(BOARD, MoveHistory[PLY]);
        PLY--;
        STM = !STM;
        undomove(BOARD, MoveHistory[PLY]);
        PLY--;
        STM = !STM;
      }
      continue;
    }
    /* exit program */
		if (!strcmp(Command, "quit"))
    {
      break;
    }
    /* set search depth */
    if (!strcmp(Command, "sd"))
    {
      sscanf (Line, "sd %d", &SD);
      SD = (SD>=max_ab_depth)?max_ab_depth:SD;
      continue;
    }
    /* turn on thinking output */
		if (!strcmp(Command, "post"))
    {
      xboard_post = true;
      continue;
    }
    /* turn off thinking output */
		if (!strcmp(Command, "nopost"))
    {
      xboard_post = false;
      continue;
    }
    /* xboard commands to ignore */
		if (!strcmp(Command, "random"))
    {
      continue;
    }
		if (!strcmp(Command, "white"))
    {
      continue;
    }
		if (!strcmp(Command, "black"))
    {
      continue;
    }
		if (!strcmp(Command, "draw"))
    {
      continue;
    }
		if (!strcmp(Command, "ping"))
    {
      continue;
    }
		if (!strcmp(Command, "result"))
    {
      continue;
    }
		if (!strcmp(Command, "hint"))
    {
      continue;
    }
		if (!strcmp(Command, "bk"))
    {
      continue;
    }
		if (!strcmp(Command, "hard"))
    {
      continue;
    }
		if (!strcmp(Command, "easy"))
    {
      continue;
    }
		if (!strcmp(Command, "name"))
    {
      continue;
    }
		if (!strcmp(Command, "rating"))
    {
      continue;
    }
		if (!strcmp(Command, "ics"))
    {
      continue;
    }
		if (!strcmp(Command, "computer"))
    {
      continue;
    }
    /* non xboard commands */
    /* do an node count to depth defined via sd  */
    if (!xboard_mode && !strcmp(Command, "perft"))
    {
      NODECOUNT = 0;
      MOVECOUNT = 0;

      fprintf(stdout,"### doing perft depth %d: ###\n", SD);  
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"### doing perft depth %d: ###\n", SD);  
      }

      start = get_time();

      perft(BOARD, STM, SD);

      end = get_time();   
      elapsed = end-start;

      fprintf(stdout,"nodecount:%" PRIu64 ", seconds: %lf, nps: %" PRIu64 " \n", 
              NODECOUNT, (elapsed/1000), (u64)(NODECOUNT/(elapsed/1000)));
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"nodecount:%" PRIu64 ", seconds: %lf, nps: %" PRIu64 " \n", 
              NODECOUNT, (elapsed/1000), (u64)(NODECOUNT/(elapsed/1000)));
      }

      fflush(stdout);
      fflush(LogFile);
  
      continue;
    }
    /* do an internal self test */
    if (!xboard_mode && !strcmp(Command, "selftest"))
    {
      selftest();
      continue;
    }
    /* print help */
    if (!xboard_mode && !strcmp(Command, "help"))
    {
      print_help();
      continue;
    }
    /* toggle log flag */
    if (!xboard_mode && !strcmp(Command, "log"))
    {
      /* open/create log file */
      if (!LogFile ) 
      {
        LogFile = fopen("zeta.log", "a");
        if (!LogFile ) 
        {
          fprintf(stdout,"Error (opening logfile zeta.log): log");
        }
      }
      continue;
    }
    /* not supported xboard commands...tell user */
		if (!strcmp(Command, "edit"))
    {
      fprintf(stdout,"Error (unsupported command): %s\n",Command);
      fprintf(stdout,"tellusererror (unsupported command): %s\n",Command);
      fprintf(stdout,"tellusererror engine supports only CECP (Xboard) version >=2\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"Error (unsupported command): %s\n",Command);
      }
      continue;
    }
		if (
        !strcmp(Command, "analyze")||
        !strcmp(Command, "pause")||
        !strcmp(Command, "resume")
        )
    {
      fprintf(stdout,"Error (unsupported command): %s\n",Command);
      fprintf(stdout,"tellusererror (unsupported command): %s\n",Command);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"Error (unsupported command): %s\n",Command);
      }
      continue;
    }
    /* unknown command...*/
    fprintf(stdout,"Error (unsupported command): %s\n",Command);
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"Error (unsupported command): %s\n",Command);
    }
  }
  /* release memory, files and tables */
  free_resources();
  exit(EXIT_SUCCESS);
}

