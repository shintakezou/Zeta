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

#ifndef ZETA_H_INCLUDED
#define ZETA_H_INCLUDED

#include "types.h"

s32  load_file_to_string(const char *filename, char **result);
void print_debug(char *debug);

// from ini config file
extern s32 threadsX;
extern s32 threadsY;
extern s32 threadsZ;
extern s32 totalThreads;
extern s32 max_depth;
extern s32 opencl_device_id;
extern s32 opencl_platform_id;
extern s32 max_nodes_to_expand;
extern s32 memory_slots;
extern u64 max_nodes;
extern s32 max_depth;
extern s32 max_ab_depth;
extern s64 MaxNodes;
// globals
extern FILE *LogFile;
extern s32 PLY;
extern s32 SD;
extern s32 BOARD_STACK_TOP;
extern Bitboard *GLOBAL_INIT_BOARD;
extern Move *GLOBAL_INIT_LASTMOVE;
extern u64 *COUNTERS;
extern NodeBlock *NODES;
extern Move *GLOBAL_HASHHISTORY;
// OpenCL memory buffer objects
cl_mem   GLOBAL_INIT_BOARD_Buffer;
cl_mem   GLOBAL_BOARD_STACK_1_Buffer;
cl_mem   GLOBAL_BOARD_STACK_2_Buffer;
cl_mem   GLOBAL_BOARD_STACK_3_Buffer;
cl_mem	 COUNTERS_Buffer;
cl_mem	 GLOBAL_BOARD_STAK_TOP_Buffer;
cl_mem	 GLOBAL_TOTAL_NODES_Buffer;
cl_mem	 GLOBAL_FINISHED_Buffer;
cl_mem	 GLOBAL_PID_MOVECOUNTER_Buffer;
cl_mem	 GLOBAL_PID_TODOINDEX_Buffer;
cl_mem	 GLOBAL_PID_AB_SCORES_Buffer;
cl_mem	 GLOBAL_PID_DEPTHS_Buffer;
cl_mem	 GLOBAL_PID_MOVES_Buffer;
cl_mem	 GLOBAL_PID_MOVE_HISTORY_Buffer;
cl_mem	 GLOBAL_PID_CR_HISTORY_Buffer;
cl_mem	 GLOBAL_HASHHISTORY_Buffer;
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
extern const char zeta_cl[];
extern const size_t zeta_cl_len;

#endif // ZETA_H_INCLUDED

