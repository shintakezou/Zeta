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

#include <stdio.h>      // for print and scan
#include <stdlib.h>     // for malloc free
#include <string.h>     // for string compare 
#include <getopt.h>     // for getopt_long

#include "bitboard.h"   // bit functions
#include "timer.h"
#include "types.h"
#include "zetacl.h"     // OpenCL source file zeta.cl as string
#include "zobrist.h"

// global variables
FILE *LogFile = NULL;         // logfile for debug
char *Line;                   // for fgetting the input on stdin
char *Command;                // for pasring the xboard command
char *Fen;                    // for storing the fen chess baord string
const char filename[]  = "zeta.cl";
// counters
u64 ITERCOUNT = 0;
u64 EXNODECOUNT = 0;
u64 ABNODECOUNT = 0;
u64 MOVECOUNT = 0;
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
s32 max_cores       = 1;
s32 time_management = false;
// xboard flags
bool xboard_mode    = false;  // chess GUI sets to true
bool xboard_force   = false;  // if true aplly only moves, do not think
bool xboard_post    = false;  // post search thinking output
bool xboard_san     = false;  // use san move notation instead of can
bool xboard_time    = false;  // use xboards time command for time management
bool xboard_debug   = false;  // print debug information
// timers
double start        = 0;
double end          = 0;
double elapsed      = 0;
bool TIMEOUT        = false;  // global value for time control*/
// time control in milli-seconds
s32 timemode    = 0;      // 0 = single move, 1 = conventional clock, 2 = ics clock
s32 MovesLeft   = 1;      // moves left unit nex time increase
s32 MaxMoves    = 1;      // moves to play in time frame
double TimeInc  = 0;      // time increase
double TimeBase = 5*1000; // time base for conventional inc, 5s default
double TimeLeft = 5*1000; // overall time on clock, 5s default
double MaxTime  = 5*1000; // max time per move
s64 MaxNodes    = 1;
// game state
bool STM        = WHITE;  // site to move
s32 SD          = 0;      // max search depth*/
s32 GAMEPLY     = 0;      // total ply, considering depth via fen string
s32 PLY         = 0;      // engine specifix ply counter
Move *MoveHistory;
Hash *HashHistory;
Hash *CRHistory;
// Quad Bitboard
// based on http://chessprogramming.wikispaces.com/Quad-Bitboards
// by Gerd Isenberg
Bitboard BOARD[7];
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  4   hash
  5   lastmove
*/
// for exchange with OpenCL device
Bitboard *GLOBAL_INIT_BOARD = NULL;
NodeBlock *NODES = NULL;
u64 *COUNTERS = NULL;
Hash *GLOBAL_HASHHISTORY = NULL;
s32 BOARD_STACK_TOP;
// functions 
Score perft(Bitboard *board, bool stm, s32 depth);
static void print_help(void);
static void print_version(void);
static void selftest(void);
static bool setboard(Bitboard *board, char *fenstring);
static void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply);
static void move2can(Move move, char *movec);
static Move can2move(char *usermove, Bitboard *board, bool stm);
void printboard(Bitboard *board);
void printbitboard(Bitboard board);
bool read_and_init_config();
s32 load_file_to_string(const char *filename, char **result);
// cl functions
extern bool cl_init_device();
extern bool cl_init_objects();
extern bool cl_run_search(bool stm, s32 depth);
extern bool cl_run_perft(bool stm, s32 depth);
extern bool cl_get_and_release_memory();
extern bool cl_release_device();
extern bool cl_guess_config(bool extreme);
// precomputed attack tables for move generation and square in check
const Bitboard AttackTablesPawnPushes[2*64] = 
{
  // white pawn pushes
  0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x1010000,0x2020000,0x4040000,0x8080000,0x10100000,0x20200000,0x40400000,0x80800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10000000000,0x20000000000,0x40000000000,0x80000000000,0x100000000000,0x200000000000,0x400000000000,0x800000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000,0x100000000000000,0x200000000000000,0x400000000000000,0x800000000000000,0x1000000000000000,0x2000000000000000,0x4000000000000000,0x8000000000000000,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
  // black pawn pushes
  0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x4000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x100000000,0x200000000,0x400000000,0x800000000,0x1000000000,0x2000000000,0x4000000000,0x8000000000,0x10100000000,0x20200000000,0x40400000000,0x80800000000,0x101000000000,0x202000000000,0x404000000000,0x808000000000,0x1000000000000,0x2000000000000,0x4000000000000,0x8000000000000,0x10000000000000,0x20000000000000,0x40000000000000,0x80000000000000
};
// piece attack tables
const Bitboard AttackTables[7*64] = 
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
Square getkingpos(Bitboard *board, bool side)
{
  Bitboard bbTemp = (side)?board[0]:board[0]^(board[1]|board[2]|board[3]);;
  bbTemp &= board[1]&board[2]&~board[3]; // get king
  return first1(bbTemp);
}
// is square attacked by an enemy piece, via superpiece approach
bool squareunderattack(Bitboard *board, bool stm, Square sq) 
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
// check for two opposite kings
bool isvalid(Bitboard *board)
{
  if ( (popcount(board[QBBBLACK]&(board[QBBP1]&board[QBBP2]&~board[QBBP3]))==1) 
        && (popcount( (board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]))
                      &(board[QBBP1]&board[QBBP2]&~board[QBBP3]))==1)
     )
  {
    return true;
  }

  return false;
}
// #############################
// ###        inits          ###
// #############################
// innitialize engine memory
static bool engineinits(void)
{
  // memory allocation
  Line         = (char *)calloc(1024       , sizeof (char));
  Command      = (char *)calloc(1024       , sizeof (char));
  Fen          = (char *)calloc(1024       , sizeof (char));

  if (Line==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Line[%d]", 1024);
    return false;
  }
  if (Command==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Command[%d]", 1024);
    return false;
  }
  if (Fen==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): char Fen[%d]", 1024);
    return false;
  }

  return true;
}
// innitialize game memory
static bool gameinits(void)
{
  MoveHistory = (Move*)calloc(MAXGAMEPLY, sizeof(Move));
  if (MoveHistory==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): Move MoveHistory[%d]",
            MAXGAMEPLY);
    return false;
  }
  HashHistory = (Hash*)calloc(MAXGAMEPLY, sizeof(Hash));
  if (HashHistory==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): Hash HashHistory[%d]",
            MAXGAMEPLY);
    return false;
  }
  CRHistory = (Cr*)calloc(MAXGAMEPLY, sizeof(Hash));
  if (CRHistory==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): Cr CRHistory[%d]",
            MAXGAMEPLY);
    return false;
  }

  return true;
}
void release_gameinits()
{
  // release memory
  free(MoveHistory);
  free(HashHistory);
  free(CRHistory);
}
void release_configinits()
{
  // opencl related
  free(GLOBAL_INIT_BOARD);
  free(COUNTERS);
  free(NODES);
  free(GLOBAL_HASHHISTORY);
}
void release_engineinits()
{
  // close log file
  if (LogFile)
    fclose (LogFile);
  // release memory
  free(Line);
  free(Command);
  free(Fen);
}
void quitengine(s32 flag)
{
  cl_release_device();
  release_configinits();
  release_gameinits();
  release_engineinits();
  exit(flag);
}
// #############################
// ###         Hash          ###
// #############################
// compute zobrist hash from position
Hash computehash(Bitboard *board, bool stm)
{
  Piece piece;
  Bitboard bbWork;
  Square sq;
  Hash hash = HASHNONE;
  Hash zobrist;
  u8 side;

  // for each color
  for (side=WHITE;side<=BLACK;side++)
  {
    bbWork = (side==BLACK)?board[QBBBLACK]:(board[QBBBLACK]^(board[QBBP1]|board[QBBP2]|board[QBBP3]));
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
  if (((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
      hash ^= Zobrist[12];
  if (((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
      hash ^= Zobrist[13];
  if (((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
      hash ^= Zobrist[14];
  if (((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
      hash ^= Zobrist[15];
 
  // file en passant
  if (GETSQEP(board[QBBLAST]))
  {
    sq = GETFILE(GETSQEP(board[QBBLAST]));
    zobrist = Zobrist[16];
    hash ^= ((zobrist<<sq)|(zobrist>>(64-sq)));; // rotate left 64
  }
 
  // site to move
  if (!stm)
    hash ^= 0x1ULL;

  return hash;
}
// #############################
// ###     domove undomove   ###
// #############################
// apply null-move on board
void donullmove(Bitboard *board)
{
  // color flipping
  board[QBBHASH] ^= 0x1ULL;
  board[QBBLAST] = MOVENONE|(CMMOVE&board[QBBLAST]);
}
// restore board again after nullmove
void undonullmove(Bitboard *board, Move lastmove, Hash hash)
{
  board[QBBHASH] = hash;
  board[QBBLAST] = lastmove;
}
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
  Bitboard pcpt   = GETPCPT(move);
  Bitboard bbTemp = BBEMPTY;
  Bitboard pcastle= PNONE;
  u64 hmc         = GETHMC(board[QBBLAST]);
  Hash zobrist;

  // check for edges
  if (move==MOVENONE)
    return;

  // increase half move clock
  hmc++;

  // do hash increment , clear old
  // castle rights
  if(((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
    board[QBBHASH] ^= Zobrist[12];
  if(((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
    board[QBBHASH] ^= Zobrist[13];
  if(((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
    board[QBBHASH] ^= Zobrist[14];
  if(((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
    board[QBBHASH] ^= Zobrist[15];

  // file en passant
  if (GETSQEP(board[QBBLAST]))
  {
    zobrist = Zobrist[16];
    board[QBBHASH] ^= ((zobrist<<GETFILE(GETSQEP(board[QBBLAST])))|(zobrist>>(64-GETFILE(GETSQEP(board[QBBLAST])))));; // rotate left 64
  }

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

  // set piece moved flag, for castle rights
  board[QBBPMVD]  |= SETMASKBB(sqfrom);
  board[QBBPMVD]  |= SETMASKBB(sqto);
  board[QBBPMVD]  |= SETMASKBB(sqcpt);

  // handle castle rook, queenside
  pcastle = (move&MOVEISCRQ)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  // unset castle rook from
  bbTemp  = (move&MOVEISCRQ)?CLRMASKBB(sqfrom-4):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  // set castle rook to
  board[QBBBLACK] |= (pcastle&0x1)<<(sqto+1);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto+1);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto+1);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto+1);
  // set piece moved flag, for castle rights
  board[QBBPMVD]  |= (pcastle)?SETMASKBB(sqfrom-4):BBEMPTY;
  // reset halfmoveclok
  hmc = (pcastle)?0:hmc;  // castle move
  // do hash increment, clear old rook
  zobrist = Zobrist[GETCOLOR(pcastle)*6+ROOK-1];
  board[QBBHASH] ^= (pcastle)?((zobrist<<(sqfrom-4))|(zobrist>>(64-(sqfrom-4)))):BBEMPTY;
  // do hash increment, set new rook
  board[QBBHASH] ^= (pcastle)?((zobrist<<(sqto+1))|(zobrist>>(64-(sqto+1)))):BBEMPTY;

  // handle castle rook, kingside
  pcastle = (move&MOVEISCRK)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  // unset castle rook from
  bbTemp  = (move&MOVEISCRK)?CLRMASKBB(sqfrom+3):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  // set castle rook to
  board[QBBBLACK] |= (pcastle&0x1)<<(sqto-1);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqto-1);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqto-1);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqto-1);
  // set piece moved flag, for castle rights
  board[QBBPMVD]  |= (pcastle)?SETMASKBB(sqfrom+3):BBEMPTY;
  // reset halfmoveclok
  hmc = (pcastle)?0:hmc;  // castle move
  // do hash increment, clear old rook
  board[QBBHASH] ^= (pcastle)?((zobrist<<(sqfrom+3))|(zobrist>>(64-(sqfrom+3)))):BBEMPTY;
  // do hash increment, set new rook
  board[QBBHASH] ^= (pcastle)?((zobrist<<(sqto-1))|(zobrist>>(64-(sqto-1)))):BBEMPTY;

  // handle halfmove clock
  hmc = (GETPTYPE(pfrom)==PAWN)?0:hmc;   // pawn move
  hmc = (GETPTYPE(pcpt)!=PNONE)?0:hmc;  // capture move

  // do hash increment , set new
  // do hash increment, clear piece from
  zobrist = Zobrist[GETCOLOR(pfrom)*6+GETPTYPE(pfrom)-1];
  board[QBBHASH] ^= ((zobrist<<(sqfrom))|(zobrist>>(64-(sqfrom))));
  // do hash increment, set piece to
  zobrist = Zobrist[GETCOLOR(pto)*6+GETPTYPE(pto)-1];
  board[QBBHASH] ^= ((zobrist<<(sqto))|(zobrist>>(64-(sqto))));
  // do hash increment, clear piece capture
  zobrist = Zobrist[GETCOLOR(pcpt)*6+GETPTYPE(pcpt)-1];
  board[QBBHASH] ^= (pcpt)?((zobrist<<(sqcpt))|(zobrist>>(64-(sqcpt)))):BBEMPTY;
  // castle rights
  if(((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
    board[QBBHASH] ^= Zobrist[12];
  if(((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
    board[QBBHASH] ^= Zobrist[13];
  if(((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
    board[QBBHASH] ^= Zobrist[14];
  if(((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
    board[QBBHASH] ^= Zobrist[15];
 
  // file en passant
  if (GETSQEP(move))
  {
    zobrist = Zobrist[16];
    board[QBBHASH] ^= ((zobrist<<GETFILE(GETSQEP(move)))|(zobrist>>(64-GETFILE(GETSQEP(move)))));; // rotate left 64
  }
  // color flipping
  board[QBBHASH] ^= 0x1;

  // store hmc   
  move = SETHMC(move, hmc);
  // store lastmove in board
  board[QBBLAST] = move;
}
// restore board again
void undomove(Bitboard *board, Move move, Move lastmove, Cr cr, Hash hash)
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

  // restore lastmove with hmc and cr
  board[QBBLAST] = lastmove;
  // restore castle rights. via piece moved flags
  board[QBBPMVD] = cr;
  // restore hash
  board[QBBHASH] = hash;

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

  // handle castle rook, queenside
  pcastle = (move&MOVEISCRQ)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  // unset castle rook to
  bbTemp  = (move&MOVEISCRQ)?CLRMASKBB(sqto+1):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  // restore castle rook from
  board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom-4);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom-4);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom-4);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom-4);
  // handle castle rook, kingside
  pcastle = (move&MOVEISCRK)?MAKEPIECE(ROOK,GETCOLOR(pfrom)):PNONE;
  // restore castle rook from
  bbTemp  = (move&MOVEISCRK)?CLRMASKBB(sqto-1):BBFULL;
  board[QBBBLACK] &= bbTemp;
  board[QBBP1]    &= bbTemp;
  board[QBBP2]    &= bbTemp;
  board[QBBP3]    &= bbTemp;
  // set castle rook to
  board[QBBBLACK] |= (pcastle&0x1)<<(sqfrom+3);
  board[QBBP1]    |= ((pcastle>>1)&0x1)<<(sqfrom+3);
  board[QBBP2]    |= ((pcastle>>2)&0x1)<<(sqfrom+3);
  board[QBBP3]    |= ((pcastle>>3)&0x1)<<(sqfrom+3);
}
// #############################
// ###        IO tools       ###
// #############################
// print bitboard
void printbitboard(Bitboard board)
{
  s32 rank;
  s32 file;
  Square sq;

  fprintf(stdout,"###ABCDEFGH###\n");
  for(rank=RANK_8;rank>=RANK_1;rank--) {
    fprintf(stdout,"#%i ",rank+1);
    for(file=FILE_A;file<FILE_NONE;file++) {
      sq = MAKESQ(file, rank);
      if (board&SETMASKBB(sq)) 
        fprintf(stdout,"x");
      else 
        fprintf(stdout,"-");
    }
    fprintf(stdout,"\n");
  }
  fprintf(stdout,"###ABCDEFGH###\n");

  fflush(stdout);
}
void printmove(Move move)
{

  fprintf(stdout,"#sqfrom:%" PRIu64 "\n",GETSQFROM(move));
  fprintf(stdout,"#sqto:%" PRIu64 "\n",GETSQTO(move));
  fprintf(stdout,"#sqcpt:%" PRIu64 "\n",GETSQCPT(move));
  fprintf(stdout,"#pfrom:%" PRIu64 "\n",GETPFROM(move));
  fprintf(stdout,"#pto:%" PRIu64 "\n",GETPTO(move));
  fprintf(stdout,"#pcpt:%" PRIu64 "\n",GETPCPT(move));
  fprintf(stdout,"#sqep:%" PRIu64 "\n",GETSQEP(move));
  fprintf(stdout,"#hmc:%u\n",(u32)GETHMC(move));
  fprintf(stdout,"#score:%i\n",(Score)GETSCORE(move));
}
// move in algebraic notation, eg. e2e4, to internal packed move 
static Move can2move(char *usermove, Bitboard *board, bool stm) 
{
  File file;
  Rank rank;
  Square sqfrom;
  Square sqto;
  Square sqcpt;
  Piece pto;
  Piece pfrom;
  Piece pcpt;
  Move move;
  char promopiece;
  Square sqep = 0;

  file    = (int)usermove[0] -97;
  rank    = (int)usermove[1] -49;
  sqfrom  = MAKESQ(file, rank);
  file    = (int)usermove[2] -97;
  rank    = (int)usermove[3] -49;
  sqto    = MAKESQ(file, rank);

  pfrom = GETPIECE(board, sqfrom);
  pto = pfrom;
  sqcpt = sqto;
  pcpt = GETPIECE(board, sqcpt);

  // en passant move , set square capture
  sqcpt = ((pfrom>>1)==PAWN&&(!stm)&&GETRANK(sqfrom)==RANK_5  
            &&sqto-sqfrom!=8&&pcpt==PNONE)?sqto-8:sqcpt;

  sqcpt = ((pfrom>>1)==PAWN&&(stm)&&GETRANK(sqfrom)==RANK_4  
            &&sqfrom-sqto!=8 &&pcpt==PNONE)?sqto+8:sqcpt;

  pcpt = GETPIECE(board, sqcpt);

  // pawn double square move, set en passant target square
  if ((pfrom>>1)==PAWN&&GETRRANK(sqfrom,stm)==1&&GETRRANK(sqto,stm)==3)
    sqep = sqto;

  // pawn promo piece
  promopiece = usermove[4];
  if (promopiece == 'q' || promopiece == 'Q' )
      pto = MAKEPIECE(QUEEN,stm);
  else if (promopiece == 'n' || promopiece == 'N' )
      pto = MAKEPIECE(KNIGHT,stm);
  else if (promopiece == 'b' || promopiece == 'B' )
      pto = MAKEPIECE(BISHOP,stm);
  else if (promopiece == 'r' || promopiece == 'R' )
      pto = MAKEPIECE(ROOK,stm);

  // pack move, considering hmc, cr and score
  move = MAKEMOVE((Move)sqfrom, (Move)sqto, (Move)sqcpt, 
                  (Move)pfrom, (Move)pto , (Move)pcpt, 
                  (Move)sqep,
                  GETHMC(board[QBBLAST]), (u64)0);

  // set castle move flag
  if ((pfrom>>1)==KING&&!stm&&sqfrom==4&&sqto==2)
    move |= MOVEISCRQ;
  if ((pfrom>>1)==KING&&!stm&&sqfrom==4&&sqto==6)
    move |= MOVEISCRK;
  if ((pfrom>>1)==KING&&stm&&sqfrom==60&&sqto==58)
    move |= MOVEISCRQ;
  if ((pfrom>>1)==KING&&stm&&sqfrom==60&&sqto==62)
    move |= MOVEISCRK;
 
  return move;
}
/* packed move to move in coordinate algebraic notation,
  e.g. 
  e2e4 
  e1c1 => when king, indicates castle queenside  
  e7e8q => indicates pawn promotion to queen
*/
static void move2can(Move move, char * movec) 
{
  char rankc[8] = "12345678";
  char filec[8] = "abcdefgh";
  Square from   = GETSQFROM(move);
  Square to     = GETSQTO(move);
  Piece pfrom   = GETPFROM(move);
  Piece pto     = GETPTO(move);

  movec[0] = filec[GETFILE(from)];
  movec[1] = rankc[GETRANK(from)];
  movec[2] = filec[GETFILE(to)];
  movec[3] = rankc[GETRANK(to)];
  movec[4] = '\0';

  // pawn promo
  if ( (pfrom>>1) == PAWN && (pto>>1) != PAWN)
  {
    if ( (pto>>1) == QUEEN)
      movec[4] = 'q';
    if ( (pto>>1) == ROOK)
      movec[4] = 'r';
    if ( (pto>>1) == BISHOP)
      movec[4] = 'b';
    if ( (pto>>1) == KNIGHT)
      movec[4] = 'n';
    movec[5] = '\0';
  }
}
void printmovecan(Move move)
{
  char movec[4];
  move2can(move, movec);
  fprintf(stdout, "%s",movec);
  if (LogFile)
    fprintf(LogFile, "%s",movec);
}
// print quadbitbooard
void printboard(Bitboard *board)
{
  s32 rank;
  s32 file;
  Square sq;
  Piece piece;
  char wpchars[] = "-PNKBRQ";
  char bpchars[] = "-pnkbrq";
  char fenstring[1024];

  fprintf(stdout,"###ABCDEFGH###\n");
  for (rank = RANK_8; rank >= RANK_1; rank--) 
  {
    fprintf(stdout,"#%i ",rank+1);
    for (file = FILE_A; file < FILE_NONE; file++)
    {
      sq = MAKESQ(file, rank);
      piece = GETPIECE(board, sq);
      if (piece != PNONE && (piece&BLACK))
        fprintf(stdout,"%c", bpchars[piece>>1]);
      else if (piece != PNONE)
        fprintf(stdout,"%c", wpchars[piece>>1]);
      else 
        fprintf(stdout,"-");
    }
    fprintf(stdout,"\n");
  }
  fprintf(stdout,"###ABCDEFGH###\n");

  createfen (fenstring, BOARD, STM, GAMEPLY);
  fprintf(stdout,"#fen: %s\n",fenstring);

  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile, "#fen: %s\n",fenstring);
    fprintdate(LogFile);
    fprintf(LogFile, "###ABCDEFGH###\n");
    for (rank = RANK_8; rank >= RANK_1; rank--) 
    {
    fprintdate(LogFile);
      fprintf(LogFile, "#%i ",rank+1);
      for (file = FILE_A; file < FILE_NONE; file++)
      {
        sq = MAKESQ(file, rank);
        piece = GETPIECE(board, sq);
        if (piece != PNONE && (piece&BLACK))
          fprintf(LogFile, "%c", bpchars[piece>>1]);
        else if (piece != PNONE)
          fprintf(LogFile, "%c", wpchars[piece>>1]);
        else 
          fprintf(LogFile, "-");
      }
      fprintf(LogFile, "\n");
    }
    fprintdate(LogFile);
    fprintf(LogFile, "###ABCDEFGH###\n");
    fflush (LogFile);
  }
  fflush (stdout);
}
// create fen string from board state
static void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply)
{
  s32 rank;
  s32 file;
  Square sq;
  Piece piece;
  char wpchars[] = " PNKBRQ";
  char bpchars[] = " pnkbrq";
  char rankc[8] = "12345678";
  char filec[8] = "abcdefgh";
  char *stringptr = fenstring;
  s32 spaces = 0;

  // add pieces from board to string
  for (rank = RANK_8; rank >= RANK_1; rank--)
  {
    spaces=0;
    for (file = FILE_A; file < FILE_NONE; file++)
    {
      sq = MAKESQ(file, rank);
      piece = GETPIECE(board, sq);
      // handle empty squares
      if (spaces > 0 && piece != PNONE)
      {
        stringptr+=sprintf(stringptr, "%d", spaces);
        spaces=0;
      }
      // handle pieces, black and white
      if (piece != PNONE && (piece&BLACK))
        stringptr+=sprintf(stringptr, "%c", bpchars[piece>>1]);
      else if (piece != PNONE)
        stringptr+=sprintf(stringptr, "%c", wpchars[piece>>1]);
      else
        spaces++;
    }
    // handle empty squares
    if (spaces > 0)
    {
      stringptr+=sprintf(stringptr, "%d", spaces);
      spaces=0;
    }
    // handle rows delimeter
    if (rank <= RANK_8 && rank > RANK_1)
      stringptr+=sprintf(stringptr, "/");
  }

  stringptr+=sprintf(stringptr, " ");

  // add site to move
  if (stm&BLACK)
  {
    stringptr+=sprintf(stringptr, "b");
  }
  else
  {
    stringptr+=sprintf(stringptr, "w");
  }

  stringptr+=sprintf(stringptr, " ");

  // add castle rights
  if (((~board[QBBPMVD])&SMCRALL)==BBEMPTY)
    stringptr+=sprintf(stringptr, "-");
  else
  {
    // white kingside
    if (((~board[QBBPMVD])&SMCRWHITEK)==SMCRWHITEK)
      stringptr+=sprintf(stringptr, "K");
    // white queenside
    if (((~board[QBBPMVD])&SMCRWHITEQ)==SMCRWHITEQ)
      stringptr+=sprintf(stringptr, "Q");
    // black kingside
    if (((~board[QBBPMVD])&SMCRBLACKK)==SMCRBLACKK)
      stringptr+=sprintf(stringptr, "k");
    // black queenside
    if (((~board[QBBPMVD])&SMCRBLACKQ)==SMCRBLACKQ)
      stringptr+=sprintf(stringptr, "q");
  }

  stringptr+=sprintf(stringptr," ");

  // add en passant target square
  sq = GETSQEP(board[QBBLAST]);
  if (sq > 0)
  {
    if (stm)
      sq-=8;
    if (!stm)
      sq+=8;
    stringptr+=sprintf(stringptr, "%c", filec[GETFILE(sq)]);
    stringptr+=sprintf(stringptr, "%c", rankc[GETRANK(sq)]);
  }
  else
    stringptr+=sprintf(stringptr, "-");

  stringptr+=sprintf(stringptr," ");

  // add halpfmove clock 
  stringptr+=sprintf(stringptr, "%" PRIu64 "",GETHMC(board[QBBLAST]));
  stringptr+=sprintf(stringptr, " ");

  stringptr+=sprintf(stringptr, "%d", ((gameply+PLY)/2));
}
// set internal chess board presentation to fen string
static bool setboard(Bitboard *board, char *fenstring)
{
  char tempchar;
  char *position; // piece types and position, row_8, file_a, to row_1, file_h*/
  char *cstm;     // site to move
  char *castle;   // castle rights
  char *cep;      // en passant target square
  char fencharstring[24] = {" PNKBRQ pnkbrq/12345678"}; // mapping
  File file;
  Rank rank;
  Bitboard piece;
  Square sq;
  u64 i;
  u64 j;
  u64 hmc = 0;        // half move clock
  u64 fendepth = 1;   // game depth
  Move lastmove = MOVENONE;
  Bitboard bbCr = BBEMPTY;

  // memory, fen string ist max 1023 char in size
  position  = malloc (1024 * sizeof (char));
  if (!position) 
  {
    fprintf(stdout,"Error (memory allocation failed): char position[%d]", 1024);
  }
  cstm  = malloc (1024 * sizeof (char));
  if (!cstm) 
  {
    fprintf(stdout,"Error (memory allocation failed): char cstm[%d]", 1024);
  }
  castle  = malloc (1024 * sizeof (char));
  if (!castle) 
  {
    fprintf(stdout,"Error (memory allocation failed): char castle[%d]", 1024);
  }
  cep  = malloc (1024 * sizeof (char));
  if (!cep) 
  {
    fprintf(stdout,"Error (memory allocation failed): char cep[%d]", 1024);
  }
  if (!position||!cstm||!castle||!cep)
  {
    // release memory
    if (position) 
      free(position);
    if (cstm) 
      free(cstm);
    if (castle) 
      free(castle);
    if (cep) 
      free(cep);
    return false;
  }

  // get data from fen string
	sscanf (fenstring, "%s %s %s %s %" PRIu64 " %" PRIu64 "", 
          position, cstm, castle, cep, &hmc, &fendepth);

  // empty the board
  board[QBBBLACK] = 0x0ULL;
  board[QBBP1]    = 0x0ULL;
  board[QBBP2]    = 0x0ULL;
  board[QBBP3]    = 0x0ULL;
  board[QBBPMVD]  = BBFULL;
  board[QBBHASH]  = 0x0ULL;
  board[QBBLAST]  = 0x0ULL;

  // parse piece types and position from fen string
  file = FILE_A;
  rank = RANK_8;
  i=  0;
  while (!(rank <= RANK_1 && file >= FILE_NONE))
  {
    tempchar = position[i++];
    // iterate through all characters
    for (j = 0; j <= 23; j++) 
    {
  		if (tempchar == fencharstring[j])
      {
        // delimeter /
        if (j == 14)
        {
            rank--;
            file = FILE_A;
        }
        // empty squares
        else if (j >= 15)
        {
            file+=j-14;
        }
        else if (j >= 7)
        {
            sq               = MAKESQ(file, rank);
            piece            = j-7;
            piece            = MAKEPIECE(piece,BLACK);
            board[QBBBLACK] |= (piece&0x1)<<sq;
            board[QBBP1]    |= ((piece>>1)&0x1)<<sq;
            board[QBBP2]    |= ((piece>>2)&0x1)<<sq;
            board[QBBP3]    |= ((piece>>3)&0x1)<<sq;
            file++;
        }
        else if (j <= 6)
        {
            sq               = MAKESQ(file, rank);
            piece            = j;
            piece            = MAKEPIECE(piece,WHITE);
            board[QBBBLACK] |= (piece&0x1)<<sq;
            board[QBBP1]    |= ((piece>>1)&0x1)<<sq;
            board[QBBP2]    |= ((piece>>2)&0x1)<<sq;
            board[QBBP3]    |= ((piece>>3)&0x1)<<sq;
            file++;
        }
        break;                
      } 
    }
  }
  // site to move
  STM = WHITE;
  if (cstm[0] == 'b' || cstm[0] == 'B')
  {
    STM = BLACK;
  }
  // castle rights
  tempchar = castle[0];
  if (tempchar != '-')
  {
    i = 0;
    while (tempchar != '\0')
    {
      // white queenside
      if (tempchar == 'Q')
        bbCr |= SMCRWHITEQ;
      // white kingside
      if (tempchar == 'K')
        bbCr |= SMCRWHITEK;
      // black queenside
      if (tempchar == 'q')
        bbCr |= SMCRBLACKQ;
      // black kingside
      if (tempchar == 'k')
        bbCr |= SMCRBLACKK;
      i++;
      tempchar = castle[i];
    }
  }
  // store castle rights via piece moved flags in board
  board[QBBPMVD]  ^= bbCr;
  // store halfmovecounter into lastmove
  lastmove = SETHMC(lastmove, hmc);

  // set en passant target square
  tempchar = cep[0];
  file  = 0;
  rank  = 0;
  if (tempchar != '-' && tempchar != '\0' && tempchar != '\n')
  {
    file  = cep[0] - 97;
    rank  = cep[1] - 49;
  }
  sq    = MAKESQ(file, rank);
  lastmove = SETSQEP(lastmove, sq);

  // ply starts at zero
  PLY = 0;
  // game ply can be more
  GAMEPLY = fendepth*2+STM;

  // compute zobrist hash
  board[QBBHASH] = computehash(BOARD, STM);
  HashHistory[PLY] = board[QBBHASH];

  // store lastmove+ in board
  board[QBBLAST] = lastmove;
  // store lastmove+ in history
  MoveHistory[PLY] = lastmove;

  // release memory
  if (position) 
    free(position);
  if (cstm) 
    free(cstm);
  if (castle) 
    free(castle);
  if (cep) 
    free(cep);

  // board valid check
  if (!isvalid(board))
  {
    fprintf(stdout,"Error (given fen position is illegal): setboard\n");        
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"Error (given fen position is illegal): setboard\n");        
    }
    return false;
  }

  return true;
}
bool read_and_init_config(char configfile[])
{
  FILE 	*fcfg;
  char line[256];

  fcfg = fopen(configfile, "r");
  if (fcfg == NULL)
  {
    fprintf(stdout,"Error (");
    fprintf(stdout, "%s file missing) ", configfile);
    fprintf(stdout, "try --guessconfig option to create a config.ini file ");
    fprintf(stdout, "or --help option for further options\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"Error (");
      fprintf(LogFile, "%s file missing) ", configfile);
      fprintf(LogFile, "try --guessconfig option to create a config.ini file ");
      fprintf(LogFile, "or --help option for further options\n");
    }
    return false;
  }
  while (fgets(line, sizeof(line), fcfg))
  { 
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
  fclose(fcfg);

  SD = max_ab_depth;

  max_nodes_to_expand = max_memory*1024*1024/sizeof(NodeBlock);

/*
  FILE 	*Stats;
  Stats = fopen("zeta.debug", "a");
  fprintf(Stats, "max_nodes_to_expand: %i ", max_nodes_to_expand);
  fclose(Stats);
*/
  if (max_nodes==0)
  {
    time_management = true;
    MaxNodes = nodes_per_second; 
  }
  else
    MaxNodes = max_nodes;

  totalThreads = threadsX*threadsY*threadsZ;

  // allocate memory
  GLOBAL_INIT_BOARD = (Bitboard*)malloc(7*sizeof(Bitboard));
  if (GLOBAL_INIT_BOARD==NULL)
  {
    fprintf(stdout, "memory alloc, GLOBAL_INIT_BOARD, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, GLOBAL_INIT_BOARD, failed\n");
    }
    return false;
  }
  NODES = (NodeBlock*)calloc((MAXMOVES+1), sizeof(NodeBlock));
  if (NODES==NULL)
  {
    fprintf(stdout, "memory alloc, NODES, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, NODES, failed\n");
    }
    return false;
  }
  COUNTERS = (u64*)calloc(10*totalThreads, sizeof(u64));
  if (COUNTERS==NULL)
  {
    fprintf(stdout, "memory alloc, COUNTERS, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, COUNTERS, failed\n");
    }
    return false;
  }
  GLOBAL_HASHHISTORY = (Hash*)malloc((totalThreads*1024) * sizeof (Hash));
  if (GLOBAL_HASHHISTORY==NULL)
  {
    fprintf(stdout, "memory alloc, GLOBAL_HASHHISTORY, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, GLOBAL_HASHHISTORY, failed\n");
    }
    return false;
  }

  return true;
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
  u64 done;
  u64 passed = 0;
  const u64 todo = 20;

  char fenpositions[20][256]  =
  {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -"
  };
  u32 depths[] =
  {
    1,2,3,4,
    1,2,3,
    1,2,3,4,
    1,2,3,
    1,2,3,
    1,2,3
  };
  u64 nodecounts[] =
  {
    20,400,8902,197281,
    48,2039,97862,
    14,191,2812,43238,
    6,264,9467,
    44,1486,62379,
    46,2079,89890,
  };

  for (done=0;done<todo;done++)
  {
    ABNODECOUNT = 0;

    SD = depths[done];
    
    fprintf(stdout,"#> doing perft depth: %d for position %" PRIu64 " of %" PRIu64 "\n", SD, done+1, todo);  
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"#> doing perft depth: %d for position %" PRIu64 " of %" PRIu64 "\n", SD, done+1, todo);  
    }
    if (!setboard(BOARD,  fenpositions[done]))
    {
      fprintf(stdout,"# Error (in setting fen position): setboard\n");        
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"# Error (in setting fen position): setboard\n");        
      }
      continue;
    }
    else
      printboard(BOARD);

    // time measurement
    start = get_time();
    // perfomance test, just leaf nodecount to given depth
    perft(BOARD, STM, SD);
    // time measurement
    end = get_time();   
    elapsed = end-start;

    if(ABNODECOUNT==nodecounts[done])
      passed++;

    if(ABNODECOUNT==nodecounts[done])
    {
      fprintf(stdout,"#> OK, Nodecount Correct, %" PRIu64 " nodes in %lf seconds with %" PRIu64 " nps.\n", ABNODECOUNT, (elapsed/1000), (u64)(ABNODECOUNT/(elapsed/1000)));
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#> OK, Nodecount Correct, %" PRIu64 " nodes in %lf seconds with %" PRIu64 " nps.\n", ABNODECOUNT, (elapsed/1000), (u64)(ABNODECOUNT/(elapsed/1000)));
      }
    }
    else
    {
      fprintf(stdout,"#> Error, Nodecount NOT Correct, %" PRIu64 " computed nodes != %" PRIu64 " nodes for depth %d.\n", ABNODECOUNT, nodecounts[done], SD);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#> Error, Nodecount NOT Correct, %" PRIu64 " computed nodes != %" PRIu64 " nodes for depth %d.\n", ABNODECOUNT, nodecounts[done], SD);
      }
    }
  }
  fprintf(stdout,"#\n###############################\n");
  fprintf(stdout,"### passed %" PRIu64 " from %" PRIu64 " tests ###\n", passed, todo);
  fprintf(stdout,"###############################\n");
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#\n###############################\n");
    fprintdate(LogFile);
    fprintf(LogFile,"### passed %" PRIu64 " from %" PRIu64 " tests ###\n", passed, todo);
    fprintdate(LogFile);
    fprintf(LogFile,"###############################\n");
  }
}
// print engine info to console
static void print_version(void)
{
  fprintf(stdout,"Zeta version: %s\n",VERSION);
  fprintf(stdout,"Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");
}
// engine options and usage
static void print_help(void)
{
  fprintf(stdout,"Zeta, experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"You will need an config.ini file to run the engine,\n");
  fprintf(stdout,"start engine from command line with --guessconfig option,\n");
  fprintf(stdout,"to create config files for all OpenCL devices.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Options:\n");
  fprintf(stdout," -l, --log          Write output/debug to file zeta.log\n");
  fprintf(stdout," -v, --version      Print Zeta version info.\n");
  fprintf(stdout," -h, --help         Print Zeta program usage help.\n");
  fprintf(stdout," -s, --selftest     Run an internal test, usefull after compile.\n");
  fprintf(stdout," --guessconfig      Guess minimal config for all OpenCL devices\n");
  fprintf(stdout," --guessconfigx     Guess optimal config for all OpenCL devices\n");
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
  fprintf(stdout,"The implemented Time Control is a bit shacky, tuned for 40 moves in 4 minutes.\n");
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
/*
  fprintf(stdout,"perft          // perform a performance test, depth set by sd command\n");
*/
  fprintf(stdout,"selftest       // run an internal test\n");
  fprintf(stdout,"help           // print usage info\n");
  fprintf(stdout,"log            // turn log on\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"WARNING:\n");
  fprintf(stdout,"It is recommended to run the engine on an discrete GPU,\n");
  fprintf(stdout,"without display connected,\n");
  fprintf(stdout,"otherwise system and display can freeze during computation.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Windows OS have an internal gpu timeout, double click the .reg file \n");
  fprintf(stdout,"\"SetWindowsGPUTimeoutTo20s.reg\"\n");
  fprintf(stdout,"and reboot the OS to set the timeout to 20 seconds.\n");
  fprintf(stdout,"\n");
}
// #############################
// ###      root search      ###
// #############################
Score perft(Bitboard *board, bool stm, s32 depth)
{
  bool state;

  ITERCOUNT   = 0;
  EXNODECOUNT = 0;
  ABNODECOUNT = 0;
  MOVECOUNT   = 0;
  MEMORYFULL  = 0;

  // init board
  memcpy(GLOBAL_INIT_BOARD, board, 7*sizeof(Bitboard));
  // reset counters
  if (COUNTERS)
    free(COUNTERS);
  COUNTERS = (u64*)calloc(totalThreads*10, sizeof(u64));
  // prepare hash history
  for(s32 i=0;i<totalThreads;i++)
  {
    memcpy(&GLOBAL_HASHHISTORY[i*1024], HashHistory, 1024* sizeof(Hash));
  }

  start = get_time(); 

  state = cl_init_objects("perft_gpu");
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
  state = cl_run_perft(stm, depth);
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
  state = cl_get_and_release_memory();
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }

  // collect counters
  for (s32 i=0;i<totalThreads;i++)
  {
    ABNODECOUNT+=   COUNTERS[i*10+2];
  }

  return 0;
}
Move rootsearch(Bitboard *board, bool stm, s32 depth)
{
  bool state;
  s32 i,j;
  Score score;
  Score tmpscore;
  Score xboard_score;
  s32 visits = 0;
  s32 tmpvisits = 0;
  Move bestmove = MOVENONE;
  Score bestscore = 0;
  s32 plyreached = 0;
  s32 bestmoveply = 0;
  double start, end;

  ITERCOUNT   = 0;
  EXNODECOUNT = 0;
  ABNODECOUNT = 0;
  MOVECOUNT   = 0;
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
  memcpy(GLOBAL_INIT_BOARD, board, 7*sizeof(Bitboard));
  // clear counters
  if (COUNTERS)
    free(COUNTERS);
  COUNTERS = (u64*)calloc(10*totalThreads, sizeof(u64));
  if (COUNTERS==NULL)
  {
    fprintf(stdout, "memory alloc, COUNTERS, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, COUNTERS, failed\n");
    }
    exit(EXIT_FAILURE);
  }
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
    quitengine(EXIT_FAILURE);
  }
*/
  state = cl_init_objects("bestfirst_gpu");
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
  state = cl_run_search(stm, depth);
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
  state = cl_get_and_release_memory();
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
/*
  state = cl_release_device();
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
*/
  // single reply
  score = -NODES[NODES[0].child].score;
  bestmove = NODES[NODES[0].child].move;
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
      bestmove = NODES[j].move;
    }
  }
  bestscore = ISINF(score)?DRAWSCORE:score;
  // collect counters
  for (i=0; i < totalThreads; i++) {
    ITERCOUNT+=     COUNTERS[i*10+0];
    EXNODECOUNT+=    COUNTERS[i*10+1];
    ABNODECOUNT+=   COUNTERS[i*10+2];
  }
//  bestscore = (s32)COUNTERS[totalThreads*4+0];
//  MOVECOUNT = COUNTERS[3];
  plyreached = COUNTERS[5];
  MEMORYFULL = COUNTERS[6];
  bestmoveply = COUNTERS[7];
  // timers
  end = get_time();
  elapsed = end-start;
  elapsed/=1000;
  // compute next nps value
  nps_current =  (s32 )(ABNODECOUNT/(elapsed));
  nodes_per_second+= (ABNODECOUNT > (u64)nodes_per_second)? (nps_current > nodes_per_second)? (nps_current-nodes_per_second)*0.66 : (nps_current-nodes_per_second)*0.33 :0;
  // xboard mate scores
  xboard_score = bestscore/10;
  xboard_score = (bestscore<=-MATESCORE)?-100000-(INF+bestscore-PLY):xboard_score;
  xboard_score = (bestscore>=MATESCORE)?100000-(-INF+bestscore+PLY):xboard_score;
  // print xboard output
  if (xboard_post==true||xboard_mode == false)
  {
    if (xboard_mode==false)
    { 
      fprintf(stdout, "depth score time nodes bfdepth pv \n");
    }
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "depth score time nodes bfdepth pv \n");
    }
    fprintf(stdout,"%i %i %i %" PRIu64 " %i 	", bestmoveply, xboard_score, (s32 )(elapsed*100), ABNODECOUNT, plyreached);          
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"%i %i %i %" PRIu64 " %i 	", bestmoveply, xboard_score, (s32 )(elapsed*100), ABNODECOUNT, plyreached);          
    }
    printmovecan(bestmove);
    fprintf(stdout,"\n");
    if (LogFile)
      fprintf(LogFile, "\n");
  }
  if ((!xboard_mode)||xboard_debug)
  {
    printboard(BOARD);
    fprintf(stdout, "#Runs: %" PRIu64 "\n", ITERCOUNT);
    fprintf(stdout, "#Expaned-Nodes: %" PRIu64 "\n", EXNODECOUNT);
    fprintf(stdout, "#AB-Nodes: %" PRIu64 "\n", ABNODECOUNT);
    fprintf(stdout, "#BF-Depth: %d\n", plyreached);
    fprintf(stdout, "#ScoreDepth: %d\n", bestmoveply);
    fprintf(stdout, "#Score: %d\n", xboard_score);
    fprintf(stdout, "#nps: %" PRIu64 "\n", (u64)(ABNODECOUNT/elapsed));
    fprintf(stdout, "#sec: %lf\n", elapsed);
  }
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile, "#Runs: %" PRIu64 "\n", ITERCOUNT);
    fprintdate(LogFile);
    fprintf(LogFile, "#Expaned-Nodes: %" PRIu64 "\n", EXNODECOUNT);
    fprintdate(LogFile);
    fprintf(LogFile, "#AB-Nodes: %" PRIu64 "\n", ABNODECOUNT);
    fprintdate(LogFile);
    fprintf(LogFile, "#BF-Depth: %d\n", plyreached);
    fprintdate(LogFile);
    fprintf(LogFile, "#ScoreDepth: %d\n", bestmoveply);
    fprintdate(LogFile);
    fprintf(LogFile, "#Score: %d\n", xboard_score);
    fprintdate(LogFile);
    fprintf(LogFile, "#nps: %" PRIu64 "\n", (u64)(ABNODECOUNT/elapsed));
    fprintdate(LogFile);
    fprintf(LogFile, "#sec: %lf\n", elapsed);
  }

  fflush(stdout);

  return bestmove;
}
// run an benchmark for current set up
s32 benchmark(Bitboard *board, bool stm, s32 depth)
{
  s32 i,j;
  Score score = -2147483647;
  Score tmpscore;
  s32 tmpvisits = 0;
  s32 visits = 0;
  Move bestmove = MOVENONE;
  Score bestscore = 0;
  s32 plyreached = 0;
  double start, end;

  ITERCOUNT   = 0;
  EXNODECOUNT = 0;
  ABNODECOUNT = 0;
  MOVECOUNT   = 0;

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
  memcpy(GLOBAL_INIT_BOARD, board, 7*sizeof(Bitboard));
  // clear counters
  if (COUNTERS)
    free(COUNTERS);
  COUNTERS = (u64*)calloc(10*totalThreads, sizeof(u64));
  if (COUNTERS==NULL)
  {
    fprintf(stdout, "memory alloc, COUNTERS, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, COUNTERS, failed\n");
    }
    exit(EXIT_FAILURE);
  }
  // prepare hash history
  for(i=0;i<totalThreads;i++)
  {
    memcpy(&GLOBAL_HASHHISTORY[i*MAXGAMEPLY], HashHistory, MAXGAMEPLY*sizeof(Hash));
  }
  // inits
  if (!cl_init_objects("bestfirst_gpu"))
  {
    return -1;
  }
  // run  benchmark
  if (!cl_run_search(stm, depth))
  {
    return -1;
  }
  // copy results
  if (!cl_get_and_release_memory())
  {
    return -1;
  }
  // timers
  end = get_time();
  elapsed = end-start;
  elapsed/=1000;

  // single reply
  score = -NODES[NODES[0].child].score;
  bestmove = NODES[NODES[0].child].move;
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
      bestmove = NODES[j].move;
    }
  }
  bestscore = ISINF(score)?DRAWSCORE:score;
  // collect counters
  for (i=0; i < totalThreads; i++) {
    ITERCOUNT+=     COUNTERS[i*10+0];
    EXNODECOUNT+=    COUNTERS[i*10+1];
    ABNODECOUNT+=   COUNTERS[i*10+2];
  }
//  bestscore = (s32)COUNTERS[totalThreads*4+0];
//  MOVECOUNT = COUNTERS[3];
  plyreached = COUNTERS[5];
  MEMORYFULL = COUNTERS[6];
//  bestmoveply = COUNTERS[7];
  // print cli output
  fprintf(stdout, "depth: %i, nodes %" PRIu64 ", nps: %i, time: %lf sec, score: %i ", plyreached, ABNODECOUNT, (int)(ABNODECOUNT/elapsed), elapsed, bestscore/10);
  fprintf(stdout, " move ");
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile, "depth: %i, nodes %" PRIu64 ", nps: %i, time: %lf sec, score: %i ", plyreached, ABNODECOUNT, (int)(ABNODECOUNT/elapsed), elapsed, bestscore/10);
    fprintf(LogFile, " move ");
  }
  printmovecan(bestmove);
  fprintf(stdout, "\n");
  if (LogFile)
    fprintf(LogFile, "\n");
  fflush(stdout);        
  if (LogFile)
    fflush(LogFile);        

  return 0;
}
// get nodes per second for temp config and specified position
s32 benchmarkWrapper(s32 benchsec)
{
  s32 bench = 0;
  // inits
  if (!gameinits())
  {
    release_gameinits();
    return -1;
  }
  if (!read_and_init_config("config.tmp"))
  {
    release_gameinits();
    release_configinits();
    return -1;
  }
  if (!cl_init_device())
  {
    release_gameinits();
    release_configinits();
    cl_release_device();
    return -1;
  }
  setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  printboard(BOARD);
  elapsed = 0;
  MaxNodes = max_nodes = 8192; // search n nodes initial
  // run bench
  while (elapsed <= benchsec) {
    if (elapsed *2 >= benchsec)
      break;
    PLY = 0;
    bench = benchmark(BOARD, STM, SD);                
    if (bench != 0 )
      break;
    if (MEMORYFULL == 1)
    {
      fprintf(stdout, "#\n");
      fprintf(stdout, "#> Lack of Device Memory, try to set memory_slots to 2 or 3\n");
      fprintf(stdout, "#\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#\n");
        fprintdate(LogFile);
        fprintf(LogFile, "#> Lack of Device Memory, try to set memory_slots to 2 or 3\n");
        fprintdate(LogFile);
        fprintf(LogFile, "#\n");
      }
      break;
    }
    max_nodes*=2; // search double the nodes for next iteration
    MaxNodes = max_nodes;
    setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  }
  // release inits
  cl_release_device();
  release_gameinits();
  release_configinits();
  if (elapsed <= 0 || ABNODECOUNT <= 0)
    return -1;

  return (ABNODECOUNT/elapsed);
}
// Zeta, experimental chess engine written in OpenCL.
int main(int argc, char* argv[])
{
  // config file
  char configfile[256] = "config.ini";
  // xboard states
  s32 xboard_protover = 0;      // Zeta works with protocoll version >= v2
  // for get opt
  s32 c;
  static struct option long_options[] = 
  {
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {"selftest", 0, 0, 's'},
    {"log", 0, 0, 'l'},
    {"guessconfig", 0, 0, 0},
    {"guessconfigx", 0, 0, 0},
    {NULL, 0, NULL, 0}
  };
  s32 option_index = 0;

  // no buffers
  setbuf (stdout, NULL);
  setbuf (stdin, NULL);

  // turn log on
  for (c=1;c<argc;c++)
  {
    if (!strcmp(argv[c], "-l") || !strcmp(argv[c],"--log"))
    {
      // open/create log file
      LogFile = fopen("zeta.log", "a");
      if (LogFile==NULL) 
      {
        fprintf(stdout,"Error (opening logfile zeta.log): --log");
      }
    }
  }
  // open log file
  if (LogFile)
  {
    // no buffers
    setbuf(LogFile, NULL);
    // print binary call to log
    fprintdate(LogFile);
    for (c=0;c<argc;c++)
    {
      fprintf(LogFile, "%s ",argv[c]);
    }
    fprintf(LogFile, "\n");
  }
  // getopt loop, parsing for help, version and logging
  while ((c = getopt_long_only (argc, argv, "",
               long_options, &option_index)) != -1) {
    switch (option_index) 
    {
      case 0:
        print_help();
        exit(EXIT_SUCCESS);
        break;
      case 1:
        print_version();
        exit(EXIT_SUCCESS);
        break;
      case 2:
        // init engine and game memory, read config ini file and init OpenCL device
        if (!engineinits()||!gameinits()||!read_and_init_config(configfile)||!cl_init_device())
        {
          quitengine(EXIT_FAILURE);
        }
        selftest();
        quitengine(EXIT_SUCCESS);
        break;
      case 3:
        break;
      case 4:
        // init engine and game memory
        if (!engineinits())
        {
            exit(EXIT_FAILURE);
        }
        cl_guess_config(false);
        release_engineinits();
        exit(EXIT_SUCCESS);
        break;
      case 5:
        // init engine and game memory
        if (!engineinits())
        {
          exit(EXIT_FAILURE);
        }
        cl_guess_config(true);
        release_engineinits();
        exit(EXIT_SUCCESS);
        break;
    }
  }
  // print engine info to console
  fprintf(stdout,"#> Zeta %s\n",VERSION);
  fprintf(stdout,"#> Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"#> Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"#> This is free software, licensed under GPL >= v2\n");
  fprintf(stdout,"#> engine is initialising...\n");  
  fprintf(stdout,"feature done=0\n");  
  if (LogFile) 
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#> Zeta %s\n",VERSION);
    fprintdate(LogFile);
    fprintf(LogFile,"#> Experimental chess engine written in OpenCL.\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#> Copyright (C) 2011-2016 Srdja Matovic, Montenegro\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#> This is free software, licensed under GPL >= v2\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#> eninge is initialising...\n");  
    fprintdate(LogFile);
    fprintf(LogFile,"feature done=0\n");  
  }
  // init engine and game memory, read config ini file and init OpenCL device
  if (!engineinits()||!gameinits()||!read_and_init_config(configfile)||!cl_init_device())
  {
    quitengine(EXIT_FAILURE);
  }
  fprintf(stdout,"#> ...finished basic inits.\n");  
  if (LogFile) 
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#> ...finished basic inits.\n");  
  }
  // xboard command loop
  for (;;)
  {
    // just to be sure, flush the output...*/
    fflush (stdout);
    if (LogFile)
      fflush (LogFile);
    // get Line
    if (!fgets (Line, 1023, stdin)) {}
    // ignore empty Lines
    if (Line[0] == '\n')
      continue;
    // print io to log file
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, ">> %s",Line);
    }
    // get command
    sscanf (Line, "%s", Command);
    // xboard commands
    // set xboard mode
    if (!strcmp(Command, "xboard"))
    {
      fprintf(stdout,"feature done=0\n");  
      xboard_mode = true;
      continue;
    }
    // get xboard protocoll version
    if (!strcmp(Command, "protover")) 
    {
      sscanf (Line, "protover %d", &xboard_protover);
      // zeta supports only CECP >= v2
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
        // send feature list to xboard
        fprintf(stdout,"feature myname=\"Zeta %s\"\n",VERSION);
        fprintf(stdout,"feature ping=0\n");
        fprintf(stdout,"feature setboard=1\n");
        fprintf(stdout,"feature playother=0\n");
        fprintf(stdout,"feature san=0\n");
        // check feature san accepted 
        if (!fgets (Line, 1023, stdin)) {}
        // get command
        sscanf (Line, "%s", Command);
        if (strstr(Command, "rejected"))
          xboard_san = true;
        fprintf(stdout,"feature usermove=1\n");
        // check feature usermove accepted 
        if (!fgets (Line, 1023, stdin)) {}
        // get command
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
          quitengine(EXIT_FAILURE);
        }
        fprintf(stdout,"feature time=1\n");
        // check feature time accepted 
        if (!fgets (Line, 1023, stdin)) {}
        // get command
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
        // check feature debug accepted 
        if (!fgets (Line, 1023, stdin)) {}
        // get command
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
    // initialize new game
		if (!strcmp(Command, "new"))
    {
      release_gameinits();
      gameinits();
      if (!setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
      {
        fprintf(stdout,"Error (in setting start postition): new\n");        
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (in setting start postition): new\n");        
        }
      }
      SD = max_ab_depth;
      // reset time control
      MaxNodes = MaxTime/1000*nodes_per_second;
      if (!xboard_mode)
        printboard(BOARD);
      xboard_force  = false;
			continue;
		}
    // set board to position in FEN
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
      // zeta supports only CECP >= v2
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
        bool kic = squareunderattack(BOARD, STM, getkingpos(BOARD,STM));
        Move move;
        xboard_force = false;
        ITERCOUNT = 0;
        MOVECOUNT = 0;
        start = get_time();

        HashHistory[PLY] = BOARD[QBBHASH];

        // check bounds
        if (PLY>=MAXGAMEPLY)
        {
          if (STM)
          {
            fprintf(stdout, "result 1-0 { resign - max game ply reached }\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "result 1-0 { resign - max game ply reached }\n");
            }
          }
          else if (!STM)
          {
            fprintf(stdout, "result 0-1 { resign - max game ply reached}\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "result 0-1 { resign - max game ply reached}\n");
            }
          }
        }
        else
        {
          // start thinking
          move = rootsearch(BOARD, STM, SD);

          // check for root node expanded
          if (EXNODECOUNT==0)
          {
            if (STM)
            {
              fprintf(stdout, "result 1-0 { resign - internal error }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 1-0 { resign - internal error }\n");
              }
            }
            else if (!STM)
            {
              fprintf(stdout, "result 0-1 { resign - internal error}\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 0-1 { resign - internal error}\n");
              }
            }
          }
          // checkmate
          else if (kic&&move==MOVENONE)
          {
            if (STM)
            {
              fprintf(stdout, "result 1-0 { checkmate }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 1-0 { checkmate }\n");
              }
            }
            else if (!STM)
            {
              fprintf(stdout, "result 0-1 { checkmate }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 0-1 { checkmate }\n");
              }
            }
          }
          // stalemate
          else if (!kic&&move==MOVENONE)
          {
              fprintf(stdout, "result 1/2-1/2 { stalemate }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 1/2-1/2 { stalemate }\n");
              }
          }
          else 
          {
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
              printboard(BOARD);

            PLY++;
            STM = !STM;

            HashHistory[PLY] = BOARD[QBBHASH];
            MoveHistory[PLY] = move;
            CRHistory[PLY] = BOARD[QBBPMVD];

            // time mngmt
            TimeLeft-=elapsed;
            // get moves left, one move as time spare
            if (timemode==1)
              MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
            // get new time inc
            if (timemode==0)
              TimeLeft = TimeBase;
            if (timemode==2)
              TimeLeft+= TimeInc;
            // set max time per move
            MaxTime = TimeLeft/MovesLeft+TimeInc;
            // get new time inc
            if (timemode==1&&MovesLeft==2)
              TimeLeft+= TimeBase;
            // get max nodes to search
            MaxNodes = MaxTime/1000*nodes_per_second;
          }
        }
      }
      continue;
    }
    // set xboard force mode, no thinking just apply moves
		if (!strcmp(Command, "force"))
    {
      xboard_force = true;
      continue;
    }
    // set time control
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

      // from seconds to milli seconds
      TimeBase  = 60*min + sec;
      TimeBase *= 1000;
      TimeInc  *= 1000;
      TimeLeft  = TimeBase;

      if (MaxMoves==0)
        timemode = 2; // ics clocks
      else
        timemode = 1; // conventional clock mode

      // set moves left to 40 in sudden death or ics time control
      if (timemode==2)
        MovesLeft = 40;

      // get moves left
      if (timemode==1)
        MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
      // set max time per move
      MaxTime = TimeLeft/MovesLeft+TimeInc;
      // get max nodes to search
      MaxNodes = MaxTime/1000*nodes_per_second;

      continue;
    }
    // set time control to n seconds per move
		if (!strcmp(Command, "st"))
    {
      sscanf(Line, "st %lf", &TimeBase);
      TimeBase *= 1000; 
      TimeLeft  = TimeBase; 
      TimeInc   = 0;
      MovesLeft = MaxMoves = 1; // just one move*/
      timemode  = 0;
      // set max time per move
      MaxTime   = TimeLeft/MovesLeft+TimeInc;
      // get max nodes to search
      MaxNodes = MaxTime/1000*nodes_per_second;

      continue;
    }
    // time left on clock
		if (!strcmp(Command, "time"))
    {
      sscanf(Line, "time %lf", &TimeLeft);
      TimeLeft *= 10;  // centi-seconds to milliseconds
      // get moves left, one move time spare
      if (timemode==1)
        MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
      // set max time per move
      MaxTime = TimeLeft/MovesLeft+TimeInc;
      // get max nodes to search
      MaxNodes = MaxTime/1000*nodes_per_second;

      continue;
    }
    // opp time left, ignore
		if (!strcmp(Command, "otim"))
      continue;
    // xboard memory in mb, ignore, use config.ini file
		if (!strcmp(Command, "memory"))
    {
      continue;
    }
    if (!strcmp(Command, "usermove"))
    {
      char movec[6];
      Move move;
      // zeta supports only CECP >= v2
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
      // apply given move
      sscanf (Line, "usermove %s", movec);
      move = can2move(movec, BOARD,STM);

      domove(BOARD, move);

      PLY++;
      STM = !STM;

      HashHistory[PLY] = BOARD[QBBHASH];
      MoveHistory[PLY] = move;
      CRHistory[PLY] = BOARD[QBBPMVD];

      if (!xboard_mode||xboard_debug)
          printboard(BOARD);

      // we are on move
      if (!xboard_force)
      {
        bool kic = squareunderattack(BOARD, STM, getkingpos(BOARD,STM));
        ITERCOUNT = 0;
        MOVECOUNT = 0;
        start = get_time();

        HashHistory[PLY] = BOARD[QBBHASH];

        // check bounds
        if (PLY>=MAXGAMEPLY)
        {
          if (STM)
          {
            fprintf(stdout, "result 1-0 { resign - max game ply reached }\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "result 1-0 { resign - max game ply reached }\n");
            }
          }
          else if (!STM)
          {
            fprintf(stdout, "result 0-1 { resign - max game ply reached}\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "result 0-1 { resign - max game ply reached}\n");
            }
          }
        }
        else
        {
          // start thinking
          move = rootsearch(BOARD, STM, SD);

          // check for root node expanded
          if (EXNODECOUNT==0)
          {
            if (STM)
            {
              fprintf(stdout, "result 1-0 { resign - internal error }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 1-0 { resign - internal error }\n");
              }
            }
            else if (!STM)
            {
              fprintf(stdout, "result 0-1 { resign - internal error}\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 0-1 { resign - internal error}\n");
              }
            }
          }
          // checkmate
          else if (kic&&move==MOVENONE)
          {
            if (STM)
            {
              fprintf(stdout, "result 1-0 { checkmate }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 1-0 { checkmate }\n");
              }
            }
            else if (!STM)
            {
              fprintf(stdout, "result 0-1 { checkmate }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 0-1 { checkmate }\n");
              }
            }
          }
          // stalemate
          else if (!kic&&move==MOVENONE)
          {
              fprintf(stdout, "result 1/2-1/2 { stalemate }\n");
              if (LogFile)
              {
                fprintdate(LogFile);
                fprintf(LogFile, "result 1/2-1/2 { stalemate }\n");
              }
          }
          else 
          {
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
              printboard(BOARD);

            PLY++;
            STM = !STM;

            HashHistory[PLY] = BOARD[QBBHASH];
            MoveHistory[PLY] = move;
            CRHistory[PLY] = BOARD[QBBPMVD];

            // time mngmt
            TimeLeft-=elapsed;
            // get moves left, one move as time spare
            if (timemode==1)
              MovesLeft = (MaxMoves-(((PLY+1)/2)%MaxMoves))+1;
            // get new time inc
            if (timemode==0)
              TimeLeft = TimeBase;
            if (timemode==2)
              TimeLeft+= TimeInc;
            // set max time per move
            MaxTime = TimeLeft/MovesLeft+TimeInc;
            // get new time inc
            if (timemode==1&&MovesLeft==2)
              TimeLeft+= TimeBase;
            // get max nodes to search
            MaxNodes = MaxTime/1000*nodes_per_second;
          }
        }
      }
      continue;
    }
    // back up one ply
		if (!strcmp(Command, "undo"))
    {
      if (PLY>0)
      {
        undomove(BOARD, MoveHistory[PLY], MoveHistory[PLY-1], CRHistory[PLY], HashHistory[PLY]);
        PLY--;
        STM = !STM;
      }
      continue;
    }
    // back up two plies
		if (!strcmp(Command, "remove"))
    {
      if (PLY>=2)
      {
        undomove(BOARD, MoveHistory[PLY], MoveHistory[PLY-1], CRHistory[PLY], HashHistory[PLY]);
        PLY--;
        STM = !STM;
        undomove(BOARD, MoveHistory[PLY], MoveHistory[PLY-1], CRHistory[PLY], HashHistory[PLY]);
        PLY--;
        STM = !STM;
      }
      continue;
    }
    // exit program
		if (!strcmp(Command, "quit"))
    {
      break;
    }
    // set search depth
    if (!strcmp(Command, "sd"))
    {
      sscanf (Line, "sd %d", &SD);
//      SD = (SD>=max_ab_depth)?max_ab_depth:SD;
      continue;
    }
    // turn on thinking output
		if (!strcmp(Command, "post"))
    {
      xboard_post = true;
      continue;
    }
    // turn off thinking output
		if (!strcmp(Command, "nopost"))
    {
      xboard_post = false;
      continue;
    }
    // xboard commands to ignore
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
    // non xboard commands
    // do an node count to depth defined via sd 
    if (!xboard_mode && !strcmp(Command, "perft"))
    {
      ITERCOUNT = 0;
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
      elapsed/=1000;

      fprintf(stdout,"nodecount:%" PRIu64 ", seconds: %lf, nps: %" PRIu64 " \n", 
              ABNODECOUNT, elapsed, (u64)(ABNODECOUNT/elapsed));
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"nodecount:%" PRIu64 ", seconds: %lf, nps: %" PRIu64 " \n", 
              ABNODECOUNT, elapsed, (u64)(ABNODECOUNT/elapsed));
      }

      fflush(stdout);
      fflush(LogFile);
  
      continue;
    }
    // do an internal self test
    if (!xboard_mode && !strcmp(Command, "selftest"))
    {
      selftest();
      continue;
    }
    // print help
    if (!xboard_mode && !strcmp(Command, "help"))
    {
      print_help();
      continue;
    }
    // toggle log flag
    if (!xboard_mode && !strcmp(Command, "log"))
    {
      // open/create log file
      if (LogFile==NULL ) 
      {
        LogFile = fopen("zeta.log", "a");
        if (LogFile==NULL ) 
        {
          fprintf(stdout,"Error (opening logfile zeta.log): log");
        }
      }
      continue;
    }
    // not supported xboard commands...tell user
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
    // unknown command...*/
    fprintf(stdout,"Error (unsupported command): %s\n",Command);
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"Error (unsupported command): %s\n",Command);
    }
  }
  // release memory, files and tables
  quitengine(EXIT_SUCCESS);
}

