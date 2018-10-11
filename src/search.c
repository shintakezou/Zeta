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
#include <stdlib.h>       // for rand
#include <string.h>       // for string compare 
#include <math.h>         // for pow

#include "bitboard.h"     // bitboard related functions
#include "clrun.h"        // OpenCL run functions
#include "io.h"           // various IO and format functions
#include "timer.h"        // timer functions
#include "types.h"        // types and defaults and macros 
#include "xboard.h"       // xboard protocol command loop
#include "zeta.h"         // for global vars and functions

Move rootsearch(Bitboard *board, bool stm, s32 depth)
{
  bool state;
  s32 xboard_score;
  Move bestmove = MOVENONE;
  Score bestscore = DRAWSCORE;
  s32 idf = 1;

  ABNODECOUNT = 0;
  TTHITS = 0;
  TTSCOREHITS = 0;
  IIDHITS = 0;

  start = get_time(); 

  // init board
  memcpy(GLOBAL_BOARD, board, 7*sizeof(Bitboard));
  // reset counters
  memcpy(COUNTERS, COUNTERSZEROED, totalWorkUnits*threadsZ*sizeof(u64));
  // init prng
  srand((unsigned int)start);
  for(u64 i=0;i<totalWorkUnits;i++)
  {
    // prepare hash history
    memcpy(&GLOBAL_HASHHISTORY[i*MAXGAMEPLY], 
           HashHistory, MAXGAMEPLY * sizeof(Hash));
    // set random numbers
    for(u64 j=0;j<64;j++)
      RNUMBERS[i*64+j] = (u32)rand();
  }

  if (xboard_mode==false)
  { 
    fprintf(stdout, "depth score time nodes pv \n");
  }
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile, "depth score time nodes pv \n");
  }

  // iterative deepening framework
  do {

    // call GPU functions
  /*
    state = cl_init_device();
    // something went wrong...
    if (!state)
    {
      quitengine(EXIT_FAILURE);
    }
  */
    state = cl_write_objects();
    // something went wrong...
    if (!state)
    {
      quitengine(EXIT_FAILURE);
    }
    state = cl_run_alphabeta(stm, idf, MaxNodes/totalWorkUnits);
    // something went wrong...
    if (!state)
    {
      quitengine(EXIT_FAILURE);
    }
    state = cl_read_memory();
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
    // collect counters
    for(u64 i=0;i<totalWorkUnits;i++)
    {
      ABNODECOUNT+=   COUNTERS[i*64+1];
      TTHITS+=        COUNTERS[i*64+3];
      TTSCOREHITS+=   COUNTERS[i*64+4];
      IIDHITS+=       COUNTERS[i*64+5];
    }
    // timers
    end = get_time();
    elapsed = end-start;
    elapsed+=1;
    elapsed/=1000; // to seconds

    // get a bestmove anyway
    if (idf==1&&JUSTMOVE((Move)PV[1])!=MOVENONE)
      bestmove = (Move)PV[1];

    // only if gpu search was not interrupted by maxnodes
    if (COUNTERS[1]<MaxNodes/totalWorkUnits)
    {
      if (JUSTMOVE((Move)PV[1])!=MOVENONE)
        bestmove = (Move)PV[1];
      bestscore = ISINF((Score)PV[0])?DRAWSCORE:(Score)PV[0];
      // xboard mate scores
      xboard_score = (s32)bestscore;
      xboard_score = (bestscore<=-MATESCORE)?
                      -100000-(INF+bestscore)
                     :xboard_score;
      xboard_score = (bestscore>=MATESCORE)?
                      100000-(-INF+bestscore)
                     :xboard_score;
      // print xboard output
      if ((xboard_post==true||xboard_mode == false)
          &&(JUSTMOVE((Move)PV[1])!=MOVENONE))
      {
        fprintf(stdout,"%i %i %i %" PRIu64 " ", idf, xboard_score, (s32 )(elapsed*100), ABNODECOUNT);          
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"%i %i %i %" PRIu64 " ", idf, xboard_score, (s32 )(elapsed*100), ABNODECOUNT);          
        }
      
        // print PV line
        int i = 1;
        while(i<MAXPLY&&i<=idf)
        { 
          if (JUSTMOVE(PV[i])==MOVENONE)
            break;
          printmovecan(PV[i]);
          fprintf(stdout," ");
          if (LogFile)
            fprintf(LogFile, " ");
          i++;
        };

        fprintf(stdout,"\n");
        if (LogFile)
          fprintf(LogFile, "\n");

        fflush(stdout);
        if (LogFile)
          fflush(LogFile);
      }
      else
        break;
    }
    // mate in n idf cut
    if (ISMATE((s32)bestscore)
        &&((s32)bestscore>=MATESCORE)
        &&idf>=(INF-(s32)bestscore)
       )
      break;
    if (ISMATE((s32)bestscore)
        &&((s32)bestscore<=-MATESCORE)
        &&idf>=(INF+(s32)bestscore)
       )
      break;
    idf++;
  } while (idf<=depth
           &&elapsed*ESTEBF<MaxTime
           &&ABNODECOUNT*ESTEBF<=MaxNodes
           &&ABNODECOUNT>1
           &&idf<MAXPLY
          );


  if ((!xboard_mode)||xboard_debug)
  {
    fprintf(stdout,"#%" PRIu64 " searched nodes in %lf seconds, with %" PRIu64 " ttmovehits, and %" PRIu64 " ttscorehits, %" PRIu64 " iidhits, ebf: %lf, nps: %" PRIu64 " \n", ABNODECOUNT, elapsed, TTHITS, TTSCOREHITS, IIDHITS, (double)pow(ABNODECOUNT, (double)1/idf), (u64)((double)ABNODECOUNT/(elapsed)));
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"#%" PRIu64 " searched nodes in %lf seconds, with %" PRIu64 " ttmovehits, and %" PRIu64 " ttscorehits, %" PRIu64 " iidhits, ebf: %lf, nps: %" PRIu64 "  \n", ABNODECOUNT, elapsed, TTHITS, TTSCOREHITS, IIDHITS, (double)pow(ABNODECOUNT, (double)1/idf), (u64)((double)ABNODECOUNT/(elapsed)));
    }
  }

  fflush(stdout);
  fflush(LogFile);

  // compute next nps value
  nps_current =  (s64)((double)ABNODECOUNT/elapsed);

 
  nodes_per_second+= (nps_current>nodes_per_second)?
                      (s64)((double)(nps_current-nodes_per_second)*0.66) // inc by 66 %
                      :(s64)((double)(nps_current-nodes_per_second)*0.33); // dec by 33 

  return bestmove;
}

Score perft(Bitboard *board, bool stm, s32 depth)
{
  bool state;

  ABNODECOUNT = 0;
  MOVECOUNT   = 0;

  // init board
  memcpy(GLOBAL_BOARD, board, 7*sizeof(Bitboard));
  // reset counters
  memcpy(COUNTERS, COUNTERSZEROED, totalWorkUnits*threadsZ*sizeof(u64));
  // prepare hash history
  for(u64 i=0;i<totalWorkUnits;i++)
  {
    memcpy(&GLOBAL_HASHHISTORY[i*MAXGAMEPLY], 
            HashHistory, MAXGAMEPLY* sizeof(Hash));
  }

  start = get_time(); 

  state = cl_write_objects();
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
  state = cl_read_memory();
  // something went wrong...
  if (!state)
  {
    quitengine(EXIT_FAILURE);
  }

  // collect counters
  for(u64 i=0;i<totalWorkUnits;i++)
  {
    ABNODECOUNT+= COUNTERS[i*64+1];
  }

  return 0;
}

