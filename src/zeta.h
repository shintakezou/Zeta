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

int load_file_to_string(const char *filename, char **result);
void print_debug(char *debug);

extern char *source;
extern size_t sourceSize;

extern int threadsW;
extern int threadsX;
extern int threadsY;
extern int threadsZ;
extern int totalThreads;
extern int max_depth;
extern int opencl_device_id;
extern int opencl_platform_id;
extern int max_nodes_to_expand;
extern int memory_slots;
extern u64 max_nodes;
extern int max_depth;
extern int max_leaf_depth;
extern int PLY;
extern s32 BOARD_STACK_TOP;
extern int reuse_node_tree;

extern u64 *rand_array;
extern Bitboard *GLOBAL_INIT_BOARD;
extern Move *GLOBAL_INIT_LASTMOVE;
extern u64 *COUNTERS;
extern signed int *NODES;
extern Move *GLOBAL_HASHHISTORY;

extern Bitboard RAttacks[0x19000];
extern Bitboard BAttacks[0x1480];


cl_mem   GLOBAL_INIT_BOARD_Buffer;
cl_mem   GLOBAL_BOARD_STACK_1_Buffer;
cl_mem   GLOBAL_BOARD_STACK_2_Buffer;
cl_mem   GLOBAL_BOARD_STACK_3_Buffer;
cl_mem	 COUNTERS_Buffer;
cl_mem	 GLOBAL_BOARD_STAK_TOP_Buffer;
cl_mem	 GLOBAL_TOTAL_NODES_Buffer;
cl_mem	 GLOBAL_PID_MOVECOUNTER_Buffer;
cl_mem	 GLOBAL_PID_TODOINDEX_Buffer;
cl_mem	 GLOBAL_PID_AB_SCORES_Buffer;
cl_mem	 GLOBAL_PID_DEPTHS_Buffer;
cl_mem	 GLOBAL_PID_MOVES_Buffer;
cl_mem	 GLOBAL_PID_MOVE_HISTORY_Buffer;
cl_mem	 GLOBAL_FINISHED_Buffer;
cl_mem	 GLOBAL_MOVECOUNT_Buffer;
cl_mem	 GLOBAL_PLYREACHED_Buffer;
cl_mem	 GLOBAL_HASHHISTORY_Buffer;

cl_context          context;
cl_device_id        *devices;
cl_command_queue    commandQueue;
cl_program          program;
cl_kernel           kernel;

const char *content;
const size_t *contentSize;

extern char *source;
extern size_t sourceSize;

cl_uint numPlatforms;
cl_platform_id platform;
cl_uint deviceListSize;
cl_context_properties cps[3];


#endif // ZETA_H_INCLUDED

