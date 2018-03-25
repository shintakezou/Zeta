/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2018-03-25
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

#include <stdio.h>        // for print and scan
#include <string.h>       // for string compare 

#include "bitboard.h"     // bitboard related functions
#include "clrun.h"        // OpenCL run functions
#include "io.h"           // various IO and format functions
#include "search.h"       // rootsearch and perft
#include "timer.h"        // timer functions
#include "types.h"        // types and defaults and macros 
#include "zeta.h"         // for global vars and functions

// run an benchmark for current set up
s32 benchmark(Bitboard *board, bool stm, s32 depth)
{
  Score score = -INF;
  Move bestmove = MOVENONE;
  Score bestscore = 0;

  ABNODECOUNT = 0;
  TTHITS = 0;
  MOVECOUNT   = 0;

  // init board
  memcpy(GLOBAL_BOARD, board, 8*sizeof(Bitboard));
  // reset counters
  memcpy(COUNTERS, COUNTERSZEROED, totalWorkUnits*64*sizeof(u64));
  // prepare hash history
  for(u64 i=0;i<totalWorkUnits;i++)
  {
    memcpy(&GLOBAL_HASHHISTORY[i*MAXGAMEPLY], HashHistory, MAXGAMEPLY * sizeof(Hash));
  }
  start = get_time();
  // inits
  if (!cl_write_objects())
  {
    return -1;
  }
  // run  benchmark
  if (!cl_run_alphabeta(stm, depth, MaxNodes))
  {
    return -1;
  }
  // copy results
  if (!cl_read_memory())
  {
    return -1;
  }
  // timers
  end = get_time();
  elapsed = end-start;
  elapsed += 1;
  elapsed/=1000;

  // collect counters
  for(u64 i=0;i<totalWorkUnits;i++)
  {
    ABNODECOUNT+=   COUNTERS[i*64+1];
    TTHITS+=        COUNTERS[i*64+3];
  }
  score = (Score)PV[0];
  bestmove = (Move)PV[1];
  bestscore = ISINF(score)?DRAWSCORE:score;

  // print cli output
  fprintf(stdout, "depth: %i, nodes %" PRIu64 ", nps: %i, time: %lf sec, score: %i ", depth, ABNODECOUNT, (int)(ABNODECOUNT/elapsed), elapsed, bestscore/10);
  fprintf(stdout, " move ");
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile, "depth: %i, nodes %" PRIu64 ", nps: %i, time: %lf sec, score: %i ", depth, ABNODECOUNT, (int)(ABNODECOUNT/elapsed), elapsed, bestscore/10);
    fprintf(LogFile, " move ");
  }
  printmovecan(bestmove);
  fprintf(stdout, "\n");
  if (LogFile)
    fprintf(LogFile, "\n");
  fflush(stdout);        
  if (LogFile)
    fflush(LogFile); 

  fflush(stdout);        
  if (LogFile)
    fflush(LogFile);        

  return 0;
}
// get nodes per second for temp config and specified position
s64 benchmarkWrapper(s32 benchsec)
{
  s32 sd = 1; 
  s32 bench = 0;
  // inits
  if (!gameinits())
  {
    return -1;
  }
  if (!read_and_init_config("config.tmp"))
  {
    release_gameinits();
    return -1;
  }
  if (!cl_init_device("alphabeta_gpu"))
  {
    release_gameinits();
    release_configinits();
    return -1;
  }
  printboard(BOARD);
  MaxNodes = 8192; // search n nodes initial
  // run bench
  elapsed = 0;
  while (elapsed <= benchsec) 
  {
    setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    bench = benchmark(BOARD, STM, sd);                
    if (bench != 0 )
      break;
    if (elapsed*4>=benchsec&&sd>1)
      break;
    sd++;
    MaxNodes = (u64)(ABNODECOUNT/elapsed)*(u64)benchsec;
  }
  // release inits
  cl_release_device();
  release_gameinits();
  release_configinits();
  if (elapsed <= 0 || ABNODECOUNT <= 0)
    return -1;

  return (ABNODECOUNT/elapsed);
}

