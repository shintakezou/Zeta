/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2017
  License:      GPL >= v2

  Copyright (C) 2011-2017 Srdja Matovic

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

#include "types.h"

void print_debug(char *debug);

// from ini config file
extern u64 threadsX;
extern u64 threadsY;
extern u64 threadsZ;
extern u64 totalWorkUnits;
extern s32 opencl_device_id;
extern s32 opencl_platform_id;
extern s32 opencl_gpugen;
extern u64 max_memory;
extern u64 memory_slots;
// globals
extern FILE *LogFile;
extern s32 PLY;
extern s32 SD;
extern u64 MaxNodes;
extern u64 ABNODECOUNT;
extern Bitboard *GLOBAL_BOARD;
extern u64 *COUNTERS;
extern u32 *RNUMBERS;
extern u64 *COUNTERSZEROED;
extern Move *PV;
extern Move *PVZEROED;
extern TTMove *KILLERZEROED;
extern TTMove *COUNTERZEROED;
extern Move *GLOBAL_HASHHISTORY;
extern Bitboard bbInBetween[64*64];
extern Bitboard bbLine[64*64];
extern s32 opencl_user_device;
extern s32 opencl_user_platform;
extern struct TTE *TT;
// OpenCL memory buffer objects
cl_mem   GLOBAL_BOARD_Buffer;
cl_mem   GLOBAL_globalbbMoves_Buffer;
cl_mem	 GLOBAL_COUNTERS_Buffer;
cl_mem   GLOBAL_RNUMBERS_Buffer;
cl_mem	 GLOBAL_PV_Buffer;
cl_mem	 GLOBAL_HASHHISTORY_Buffer;
cl_mem	 GLOBAL_bbInBetween_Buffer;
cl_mem	 GLOBAL_bbLine_Buffer;
cl_mem   GLOBAL_TT1_Buffer;
cl_mem   GLOBAL_TT2_Buffer;
cl_mem   GLOBAL_TT3_Buffer;
cl_mem   GLOBAL_TT4_Buffer;
cl_mem   GLOBAL_Killer_Buffer;
cl_mem   GLOBAL_Counter_Buffer;
cl_mem   GLOBAL_finito_Buffer;
// OpenCL runtime objects
cl_context          context;
cl_device_id        *devices;
cl_command_queue    commandQueue;
cl_program          program;
cl_kernel           kernel;
// for OpenCL config
cl_uint numPlatforms;
cl_platform_id platform;
cl_uint deviceListSize;
cl_context_properties cps[3];
// zeta.cl as zetacl.h
extern const char zeta1stgen_cl[];
extern const size_t zeta1stgen_cl_len;
extern const char zeta2ndgen_cl[];
extern const size_t zeta2ndgen_cl_len;
extern const char zeta3rdgen_cl[];
extern const size_t zeta3rdgen_cl_len;
extern const char zetaperft_cl[];
extern const size_t zetaperft_cl_len;

#endif // ZETA_H_INCLUDED

