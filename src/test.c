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
#include <stdlib.h>       // for exit
#include <string.h>       // for string compare 
#include <math.h>         // for pow

#include "bitboard.h"     // bitboard related functions
#include "clrun.h"        // OpenCL run functions
#include "io.h"           // various IO and format functions
#include "search.h"       // rootsearch and perft
#include "timer.h"        // timer functions
#include "types.h"        // types and defaults and macros 
#include "xboard.h"       // xboard protocol command loop
#include "zeta.h"         // for global vars and functions

void selftest(void)
{
  bool state;
  u64 done;
  u64 passed = 0;
  const u64 todo = 14;

  char fenpositions[14][256]  =
  {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - -"
  };
  u32 depths[14] =
  {
    1,2,3,4,
    1,2,3,4,
    1,2,3,
    1,2,3
  };
  u64 nodecounts[14] =
  {
    20,400,8902,197281,
    14,191,2812,43238,
    48,2039,97862,
    46,2079,89890
  };

  state = cl_release_device();
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
  state = cl_init_device("perft_gpu");
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }

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
      fprintf(stdout,"#> OK, Nodecount Correct, %" PRIu64 " nodes in %lf seconds with %" PRIu64 " nps.\n", ABNODECOUNT, (elapsed/1000), (u64)((double)ABNODECOUNT/(elapsed/1000)));
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#> OK, Nodecount Correct, %" PRIu64 " nodes in %lf seconds with %" PRIu64 " nps.\n", ABNODECOUNT, (elapsed/1000), (u64)((double)ABNODECOUNT/(elapsed/1000)));
      }
    }
    else
    {
      fprintf(stdout,"#> Error, Nodecount NOT Correct, %" PRIu64 " computed nodes != %" PRIu64 " nodes for depth %d. in %lf seconds with %" PRIu64 " nps.\n", ABNODECOUNT, nodecounts[done], SD, (elapsed/1000), (u64)((double)ABNODECOUNT/(elapsed/1000)));
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#> Error, Nodecount NOT Correct, %" PRIu64 " computed nodes != %" PRIu64 " nodes for depth %d. in %lf seconds with %" PRIu64 " nps.\n", ABNODECOUNT, nodecounts[done], SD, (elapsed/1000), (u64)((double)ABNODECOUNT/(elapsed/1000)));
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
  state = cl_release_device();
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
  state = cl_init_device("alphabeta_gpu");
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }
}

