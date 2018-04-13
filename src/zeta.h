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

#ifndef ZETA_H_INCLUDED
#define ZETA_H_INCLUDED

#include "types.h"        // types and defaults and macros 

void quitengine(s32 flag);
bool engineinits(void);
bool gameinits(void);
void release_gameinits();
void release_configinits();
void release_engineinits();
void print_help(void);

// io
extern char *Line;
extern char *Command;
extern char *Fen;
// counters
extern u64 ABNODECOUNT;
extern u64 TTHITS;
extern u64 TTSCOREHITS;
extern u64 IIDHITS;
extern u64 MOVECOUNT;
// config file
extern char configfile[256];
extern u64 threadsX;
extern u64 threadsY;
extern const u64 threadsZ;
extern u64 totalWorkUnits;
extern s64 nodes_per_second;
extern s64 nps_current;
extern u64 tt1_memory;
extern u64 tt2_memory;
//extern u64 tt3_memory;
extern s32 opencl_device_id;
extern s32 opencl_platform_id;
extern s32 opencl_user_device;
extern s32 opencl_user_platform;
extern s32 opencl_gpugen;
// further config
extern s32 search_depth;
// timers
extern double start;
extern double end;
extern double elapsed;
extern bool TIMEOUT;
extern s32 timemode;
extern s32 MovesLeft;
extern s32 MaxMoves;
extern double TimeInc;
extern double TimeBase;
extern double TimeLeft;
extern double MaxTime;
extern u64 MaxNodes;
// game state
extern bool STM;
extern s32 SD;
extern s32 GAMEPLY;
extern s32 PLY;
// game histories
extern Move *MoveHistory;
extern Hash *HashHistory;
extern Hash *CRHistory;
extern u64 *HMCHistory;
// globals
extern FILE *LogFile;
extern Bitboard BOARD[7];
extern s32 PLY;
extern s32 SD;
extern u64 ABNODECOUNT;
extern Bitboard *GLOBAL_BOARD;
extern u64 *COUNTERS;
extern u32 *RNUMBERS;
extern u64 *COUNTERSZEROED;
extern Move *PV;
extern Move *PVZEROED;
extern TTMove *KILLERZEROED;
extern TTMove *COUNTERZEROED;
extern Hash *GLOBAL_HASHHISTORY;
extern Bitboard bbInBetween[64*64];
extern Bitboard bbLine[64*64];
extern TTE *TT1ZEROED;
extern ABDADATTE *TT2ZEROED;
//extern TTE *TT3ZEROED;
// OpenCL memory buffer objects
extern cl_mem  GLOBAL_BOARD_Buffer;
extern cl_mem  GLOBAL_globalbbMoves1_Buffer;
extern cl_mem  GLOBAL_globalbbMoves2_Buffer;
extern cl_mem	 GLOBAL_COUNTERS_Buffer;
extern cl_mem  GLOBAL_RNUMBERS_Buffer;
extern cl_mem	 GLOBAL_PV_Buffer;
extern cl_mem	 GLOBAL_HASHHISTORY_Buffer;
extern cl_mem	 GLOBAL_bbInBetween_Buffer;
extern cl_mem	 GLOBAL_bbLine_Buffer;
extern cl_mem  GLOBAL_TT1_Buffer;
extern cl_mem  GLOBAL_TT2_Buffer;
//extern cl_mem  GLOBAL_TT3_Buffer;
extern cl_mem  GLOBAL_Killer_Buffer;
extern cl_mem  GLOBAL_Counter_Buffer;
extern cl_mem  GLOBAL_finito_Buffer;
extern cl_mem  GLOBAL_RScore_Buffer;
// OpenCL runtime objects
extern cl_context          context;
extern cl_device_id        *devices;
extern cl_command_queue    commandQueue;
extern cl_program          program;
extern cl_kernel           kernel;
// for OpenCL config
extern cl_uint numPlatforms;
extern cl_platform_id platform;
extern cl_uint deviceListSize;
extern cl_context_properties cps[3];
// zeta.cl as zetacl.h
extern const char zeta_cl[];
extern const size_t zeta_cl_len;
extern const char zetaperft_cl[];
extern const size_t zetaperft_cl_len;

#endif // ZETA_H_INCLUDED

