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

#include "bitboard.h"     // bitboard related functions
#include "clrun.h"        // OpenCL run functions
#include "io.h"           // various IO and format functions
#include "search.h"       // rootsearch and perft
#include "test.h"         // selftest
#include "timer.h"        // timer functions
#include "types.h"        // types and defaults and macros 
#include "zeta.h"         // for global vars and functions

// xboard flags
bool xboard_mode    = false;  // chess GUI sets to true
bool xboard_force   = false;  // if true aplly only moves, do not think
bool xboard_post    = false;  // post search thinking output
bool xboard_san     = false;  // use san move notation instead of can
bool xboard_time    = false;  // use xboards time command for time management
bool xboard_debug   = false;  // print debug information

// xboard protocol command loop, ugly but works
void xboard(void) {

  s32 xboard_protover = 0;      // works only with protocoll version >= v2
  bool state = false;

  // input loop
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
      // supports only CECP >= v2
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
      state = gameinits();
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
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
      if (!setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
      {
        fprintf(stdout,"Error (in setting start postition): new\n");        
        fprintf(stdout,"tellusererror (Error in setting start postition): new\n");        
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"Error (in setting start postition): new\n");        
        }
      }
      SD = MAXPLY; // reset search depth
      // reset time control
      MaxNodes = (u64)(MaxTime/1000*(double)nodes_per_second);
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
        fprintf(stdout,"tellusererror (Error in setting chess psotition via fen string): setboard\n");        
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
      // supports only CECP >= v2
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
        bool kic = squareunderattack(BOARD, !STM, getkingpos(BOARD,STM));
        Move move;
        xboard_force = false;
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

          // check for root node searched
          if (ABNODECOUNT==0)
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
          else if (kic&&JUSTMOVE(move)==MOVENONE)
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
          else if (!kic&&JUSTMOVE(move)==MOVENONE)
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

            if ((!xboard_mode)||xboard_debug)
            {
              printboard(BOARD);
            }

            PLY++;
            STM = !STM;

            HashHistory[PLY] = BOARD[QBBHASH];
            MoveHistory[PLY] = move;
            CRHistory[PLY] = BOARD[QBBPMVD];
            HMCHistory[PLY] = BOARD[QBBHMC];

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
            MaxNodes = (u64)(MaxTime/1000*(double)nodes_per_second);
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
      MaxNodes = (u64)(MaxTime/1000*(double)nodes_per_second);

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
      MaxNodes = (u64)(MaxTime/1000*(double)nodes_per_second);

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
      MaxNodes = (u64)(MaxTime/1000*(double)nodes_per_second);

      continue;
    }
    // opp time left, ignore
		if (!strcmp(Command, "otim"))
      continue;
    // xboard memory in mb, ignore, use config.txt file
		if (!strcmp(Command, "memory"))
    {
      continue;
    }
    if (!strcmp(Command, "usermove"))
    {
      char movec[6];
      Move move;
      // supports only CECP >= v2
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
      HMCHistory[PLY] = BOARD[QBBHMC];

      if (!xboard_mode||xboard_debug)
          printboard(BOARD);

      // we are on move
      if (!xboard_force)
      {
        bool kic = squareunderattack(BOARD, !STM, getkingpos(BOARD,STM));
        MOVECOUNT = 0;
        start = get_time();

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

          // check for root node searched
          if (ABNODECOUNT==0)
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
          else if (kic&&JUSTMOVE(move)==MOVENONE)
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
          else if (!kic&&JUSTMOVE(move)==MOVENONE)
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

            if ((!xboard_mode)||xboard_debug)
            {
              printboard(BOARD);
            }

            PLY++;
            STM = !STM;

            HashHistory[PLY] = BOARD[QBBHASH];
            MoveHistory[PLY] = move;
            CRHistory[PLY] = BOARD[QBBPMVD];
            HMCHistory[PLY] = BOARD[QBBHMC];

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
            MaxNodes = (u64)(MaxTime/1000*(double)nodes_per_second);
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
        undomove(BOARD, MoveHistory[PLY], CRHistory[PLY], HashHistory[PLY], HMCHistory[PLY]);
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
        undomove(BOARD, MoveHistory[PLY], CRHistory[PLY], HashHistory[PLY], HMCHistory[PLY]);
        PLY--;
        STM = !STM;
        undomove(BOARD, MoveHistory[PLY], CRHistory[PLY], HashHistory[PLY], HMCHistory[PLY]);
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
      SD = (SD>MAXPLY)?MAXPLY:SD;
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
      ABNODECOUNT = 0;
      MOVECOUNT = 0;

      fprintf(stdout,"### doing inits for perft depth %d: ###\n", SD);  
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"### doing inits for perft depth %d: ###\n", SD);  
      }

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

      fprintf(stdout,"### computing perft depth %d: ###\n", SD);  
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"### computing perft depth %d: ###\n", SD);  
      }

      start = get_time();

      perft(BOARD, STM, SD);

      end = get_time();   
      elapsed = end-start;
      elapsed += 1;
      elapsed/=1000;

      state = cl_release_device();
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
      }

      fprintf(stdout,"nodecount:%" PRIu64 ", seconds: %lf, nps: %" PRIu64 " \n", 
              ABNODECOUNT, elapsed, (u64)((double)ABNODECOUNT/elapsed));
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"nodecount:%" PRIu64 ", seconds: %lf, nps: %" PRIu64 " \n", 
              ABNODECOUNT, elapsed, (u64)((double)ABNODECOUNT/elapsed));
      }

      state = cl_init_device("alphabeta_gpu");
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
      }

      fflush(stdout);
      fflush(LogFile);
  
      continue;
    }
    // do an smp benchmark for current position to depth defined via sd 
    if (!xboard_mode && !strcmp(Command, "benchsmp"))
    {
      u64 x = threadsX;
      u64 y = threadsY;
      int iter = 0;
      double *timearr = (double *)calloc(threadsX*threadsY, sizeof (double));
      u64 *workerssarr = (u64 *)calloc(threadsX*threadsY, sizeof (u64));
      u64 *npsarr = (u64 *)calloc(threadsX*threadsY, sizeof (u64));

      ABNODECOUNT = 0;
      MOVECOUNT = 0;

//      state = cl_run_alphabeta(STM, 0, 1);

      threadsX = 1;
      threadsY = 1;

      // for threadsY
      while (true)
      {
        // for threadsX
        while(true)
        {

          fprintf(stdout,"### doing inits for benchsmp depth %d: ###\n", SD);  
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile,"### doing inits for benchsmp depth %d: ###\n", SD);  
          }

          totalWorkUnits = threadsX*threadsY;

          release_gameinits();
          state = gameinits();
          // something went wrong...
          if (!state)
          {
            quitengine(EXIT_FAILURE);
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

          fprintf(stdout,"### computing benchsmp depth %d: ###\n", SD);  
          fprintf(stdout,"### work-groups: %" PRIu64 " ###\n", threadsX*threadsY);  
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile,"### computing benchsmp depth %d: ###\n", SD);  
            fprintdate(LogFile);
            fprintf(LogFile,"### work-groups: %" PRIu64 " ###\n", threadsX*threadsY);  
          }

          start = get_time();
         
          rootsearch(BOARD, STM, SD);

          end = get_time();   
          elapsed = end-start;
          elapsed += 1;
          elapsed/=1000;

          // collect results
          timearr[iter] = elapsed;
          npsarr[iter] = (u64)((double)ABNODECOUNT/elapsed);
          workerssarr[iter] = threadsX*threadsY;

          iter++;

          if (threadsX>=x)
            break;
          if (threadsX*2>x)
            threadsX = x; 
          else
            threadsX*=2; 
        }
        if (threadsY>=y)
          break;
        if (threadsY*2>y)
          threadsY = y; 
        else
          threadsY*=2; 
      }
      // print results
      fprintf(stdout,"### workers\t#nps\t\t#nps speedup\t#time in s\t#ttd speedup \t#relative ttd speedup ###\n");
      fprintf(stdout,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[0], npsarr[0], (double)1, timearr[0], (double)1, (double)1);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"### workers\t#nps\t\t#nps speedup\t#time in s\t#ttd speedup\t#relative ttd speedup ###\n");
        fprintdate(LogFile);
        fprintf(LogFile,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[0], npsarr[0], (double)1, timearr[0], (double)1, (double)1);
      }
      for (int i=1;i<iter;i++)
      {
        fprintf(stdout,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[i], npsarr[i], (double)npsarr[i]/(double)npsarr[0], timearr[i], timearr[0]/timearr[i], timearr[i-1]/timearr[i]);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[i], npsarr[i], (double)npsarr[i]/(double)npsarr[0], timearr[i], timearr[0]/timearr[i], timearr[i-1]/timearr[i]);
        }
      }
      //reset 
      release_gameinits();
      state = read_and_init_config(configfile);
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
      }
      state = gameinits();
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
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

      fflush(stdout);
      fflush(LogFile);
  
      continue;
    }
    // do an smp benchmark for Kaufmann positions, depth defined via sd 
    if (!xboard_mode && !strcmp(Command, "benchkaufmann"))
    {
      u64 x = threadsX;
      u64 y = threadsY;
      int iter = 0;
      int posi = 0;
      double *timearr = (double *)calloc(threadsX*threadsY, sizeof (double));
      double *timearrall = (double *)calloc(threadsX*threadsY, sizeof (double));
      u64 *workerssarr = (u64 *)calloc(threadsX*threadsY, sizeof (u64));
      u64 *npsarr = (u64 *)calloc(threadsX*threadsY, sizeof (u64));

      char fenpositions[25][1024] =
      {
          "1rbq1rk1/p1b1nppp/1p2p3/8/1B1pN3/P2B4/1P3PPP/2RQ1R1K w - -",
          "3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - -",
          "3r2k1/1p3ppp/2pq4/p1n5/P6P/1P6/1PB2QP1/1K2R3 w - -",
          "r1b1r1k1/1ppn1p1p/3pnqp1/8/p1P1P3/5P2/PbNQNBPP/1R2RB1K w - -",
          "2r4k/pB4bp/1p4p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - -",
          "r5k1/3n1ppp/1p6/3p1p2/3P1B2/r3P2P/PR3PP1/2R3K1 b - -",
          "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -",
          "5r1k/6pp/1n2Q3/4p3/8/7P/PP4PK/R1B1q3 b - -",
          "r3k2r/pbn2ppp/8/1P1pP3/P1qP4/5B2/3Q1PPP/R3K2R w KQkq -",
          "3r2k1/ppq2pp1/4p2p/3n3P/3N2P1/2P5/PP2QP2/K2R4 b - -",
          "q3rn1k/2QR4/pp2pp2/8/P1P5/1P4N1/6n1/6K1 w - -",
          "6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - -",
          "1r4k1/7p/5np1/3p3n/8/2NB4/7P/3N1RK1 w - -",
          "1r2r1k1/p4p1p/6pB/q7/8/3Q2P1/PbP2PKP/1R3R2 w - -",
          "r2q1r1k/pb3p1p/2n1p2Q/5p2/8/3B2N1/PP3PPP/R3R1K1 w - -",
          "8/4p3/p2p4/2pP4/2P1P3/1P4k1/1P1K4/8 w - -",
          "1r1q1rk1/p1p2pbp/2pp1np1/6B1/4P3/2NQ4/PPP2PPP/3R1RK1 w - -",
          "q4rk1/1n1Qbppp/2p5/1p2p3/1P2P3/2P4P/6P1/2B1NRK1 b - -",
          "r2q1r1k/1b1nN2p/pp3pp1/8/Q7/PP5P/1BP2RPN/7K w - -",
          "8/5p2/pk2p3/4P2p/2b1pP1P/P3P2B/8/7K w - -",
          "8/2k5/4p3/1nb2p2/2K5/8/6B1/8 w - -",
          "1B1b4/7K/1p6/1k6/8/8/8/8 w - -",
          "rn1q1rk1/1b2bppp/1pn1p3/p2pP3/3P4/P2BBN1P/1P1N1PP1/R2Q1RK1 b - -",
          "8/p1ppk1p1/2n2p2/8/4B3/2P1KPP1/1P5P/8 w - -",
          "8/3nk3/3pp3/1B6/8/3PPP2/4K3/8 w - -",
      };


      // for each pos
      for (posi=0;posi<25;posi++)
      {
        fprintf(stdout,"#\n");  
        fprintf(stdout,"#\n");  
        fprintf(stdout,"### setting up board %d: ###\n", posi+1);  
        if (LogFile)
        {
          fprintf(LogFile,"#\n");  
          fprintf(LogFile,"#\n");  
          fprintdate(LogFile);
          fprintf(LogFile,"### setting up board %d: ###\n", posi+1);  
        }

        setboard(BOARD,  fenpositions[posi]);

        printboard(BOARD);

        iter = 0;

        threadsX = 1;
        threadsY = 1;

        ABNODECOUNT = 0;
        MOVECOUNT = 0;

        // for threadsY
        while (true)
        {
          // for threadsX
          while(true)
          {

            fprintf(stdout,"### doing inits for benchkaufmann depth %d: ###\n", SD);  
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile,"### doing inits for benchkaufmann depth %d: ###\n", SD);  
            }

            totalWorkUnits = threadsX*threadsY;

            release_gameinits();
            state = gameinits();
            // something went wrong...
            if (!state)
            {
              quitengine(EXIT_FAILURE);
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


            fprintf(stdout,"### computing benchkaufmann depth %d: ###\n", SD);  
            fprintf(stdout,"### work-groups: %" PRIu64 " ###\n", threadsX*threadsY);  
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile,"### computing benchkaufmann depth %d: ###\n", SD);  
              fprintdate(LogFile);
              fprintf(LogFile,"### work-groups: %" PRIu64 " ###\n", threadsX*threadsY);  
            }

            start = get_time();
           
            rootsearch(BOARD, STM, SD);

            end = get_time();   
            elapsed = end-start;
            elapsed += 1;
            elapsed/=1000;

            // collect results
            timearrall[iter]+= timearr[iter] = elapsed;
            npsarr[iter] = (u64)((double)ABNODECOUNT/elapsed);
            workerssarr[iter] = threadsX*threadsY;

            iter++;

            if (threadsX>=x)
              break;
            if (threadsX*2>x)
              threadsX = x; 
            else
              threadsX*=2; 
          }
          if (threadsY>=y)
            break;
          if (threadsY*2>y)
            threadsY = y; 
          else
            threadsY*=2; 
        }
        // print results
        fprintf(stdout,"### workers\t#nps\t\t#nps speedup\t#time in s\t#ttd speedup \t#relative ttd speedup ###\n");
        fprintf(stdout,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[0], npsarr[0], (double)1, timearr[0], (double)1, (double)1);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"### workers\t#nps\t\t#nps speedup\t#time in s\t#ttd speedup\t#relative ttd speedup ###\n");
          fprintdate(LogFile);
          fprintf(LogFile,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[0], npsarr[0], (double)1, timearr[0], (double)1, (double)1);
        }
        for (int i=1;i<iter;i++)
        {
          fprintf(stdout,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[i], npsarr[i], (double)npsarr[i]/(double)npsarr[0], timearr[i], timearr[0]/timearr[i], timearr[i-1]/timearr[i]);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[i], npsarr[i], (double)npsarr[i]/(double)npsarr[0], timearr[i], timearr[0]/timearr[i], timearr[i-1]/timearr[i]);
          }
        }
      }
      // print overall results
      fprintf(stdout,"#\n");
      fprintf(stdout,"# overall results\n");
      fprintf(stdout,"### workers\t#ttd speedup\t#rel ttd speedup ###\n");
      fprintf(stdout,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[0], (double)1, (double)1);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#\n");
        fprintf(LogFile,"# overall results\n");
        fprintf(LogFile,"### workers\t#ttd speedup\t#rel ttd speedup ###\n");
        fprintf(LogFile,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[0], (double)1, (double)1);
      }
      for (int i=1;i<iter;i++)
      {
        fprintf(stdout,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[i], timearrall[0]/timearrall[i], timearrall[i-1]/timearrall[i]);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[i], timearrall[0]/timearrall[i], timearrall[i-1]/timearrall[i]);
        }
      }
      //reset 
      release_gameinits();
      state = read_and_init_config(configfile);
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
      }
      state = gameinits();
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
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

      fflush(stdout);
      fflush(LogFile);
  
      continue;
    }
    // do an smp benchmark for Hyat24 positions, depth defined via sd 
    if (!xboard_mode && !strcmp(Command, "benchhyatt24"))
    {
      u64 x = threadsX;
      u64 y = threadsY;
      int iter = 0;
      int posi = 0;
      double *timearr = (double *)calloc(threadsX*threadsY, sizeof (double));
      double *timearrall = (double *)calloc(threadsX*threadsY, sizeof (double));
      u64 *workerssarr = (u64 *)calloc(threadsX*threadsY, sizeof (u64));
      u64 *npsarr = (u64 *)calloc(threadsX*threadsY, sizeof (u64));

      char fenpositions[24][1024] =
      {
          "r2qkbnr/ppp2p1p/2n5/3P4/2BP1pb1/2N2p2/PPPQ2PP/R1B2RK1 b kq -  ",
          "r2qkbnr/ppp2p1p/8/nB1P4/3P1pb1/2N2p2/PPPQ2PP/R1B2RK1 b kq - ",
          "r2qkbnr/pp3p1p/2p5/nB1P4/3P1Qb1/2N2p2/PPP3PP/R1B2RK1 b kq - ",
          "r2qkb1r/pp3p1p/2p2n2/nB1P4/3P1Qb1/2N2p2/PPP3PP/R1B1R1K1 b kq - ",
          "r2q1b1r/pp1k1p1p/2P2n2/nB6/3P1Qb1/2N2p2/PPP3PP/R1B1R1K1 b - - ",
          "r2q1b1r/p2k1p1p/2p2n2/nB6/3PNQb1/5p2/PPP3PP/R1B1R1K1 b - - ",
          "r2q1b1r/p2k1p1p/2p5/nB6/3Pn1Q1/5p2/PPP3PP/R1B1R1K1 b - - ",
          "r2q1b1r/p1k2p1p/2p5/nB6/3PR1Q1/5p2/PPP3PP/R1B3K1 b - - ",
          "r2q1b1r/p1k2p1p/8/np6/3PR3/5Q2/PPP3PP/R1B3K1 b - - ",
          "r4b1r/p1kq1p1p/8/np6/3P1R2/5Q2/PPP3PP/R1B3K1 b - - ",
          "r6r/p1kqbR1p/8/np6/3P4/5Q2/PPP3PP/R1B3K1 b - - ",
          "5r1r/p1kqbR1p/8/np6/3P1B2/5Q2/PPP3PP/R5K1 b - - ",
          "5r1r/p2qbR1p/1k6/np2B3/3P4/5Q2/PPP3PP/R5K1 b - - ",
          "5rr1/p2qbR1p/1k6/np2B3/3P4/2P2Q2/PP4PP/R5K1 b - - ",
          "5rr1/p2qbR1p/1kn5/1p2B3/3P4/2P2Q2/PP4PP/4R1K1 b - - ",
          "4qRr1/p3b2p/1kn5/1p2B3/3P4/2P2Q2/PP4PP/4R1K1 b - - ",
          "5qr1/p3b2p/1kn5/1p1QB3/3P4/2P5/PP4PP/4R1K1 b - - ",
          "5q2/p3b2p/1kn5/1p1QB1r1/P2P4/2P5/1P4PP/4R1K1 b - - ",
          "5q2/p3b2p/1kn5/3QB1r1/p1PP4/8/1P4PP/4R1K1 b - - ",
          "5q2/p3b2p/1k6/3QR1r1/p1PP4/8/1P4PP/6K1 b - - ",
          "5q2/p3b2p/1k6/4Q3/p1PP4/8/1P4PP/6K1 b - - ",
          "3q4/p3b2p/1k6/2P1Q3/p2P4/8/1P4PP/6K1 b - - ",
          "3q4/p3b2p/8/1kP5/p2P4/8/1P2Q1PP/6K1 b - - ",
          "3q4/p3b2p/8/2P5/pk1P4/3Q4/1P4PP/6K1 b - - "
      };


      // for each pos
      for (posi=0;posi<24;posi++)
      {
        fprintf(stdout,"#\n");  
        fprintf(stdout,"#\n");  
        fprintf(stdout,"### setting up board %d: ###\n", posi+1);  
        if (LogFile)
        {
          fprintf(LogFile,"#\n");  
          fprintf(LogFile,"#\n");  
          fprintdate(LogFile);
          fprintf(LogFile,"### setting up board %d: ###\n", posi+1);  
        }

        setboard(BOARD,  fenpositions[posi]);

        printboard(BOARD);

        iter = 0;

        threadsX = 1;
        threadsY = 1;

        ABNODECOUNT = 0;
        MOVECOUNT = 0;

        // for threadsY
        while (true)
        {
          // for threadsX
          while(true)
          {

            fprintf(stdout,"### doing inits for benchhyatt24 depth %d: ###\n", SD);  
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile,"### doing inits for benchhyatt24 depth %d: ###\n", SD);  
            }

            totalWorkUnits = threadsX*threadsY;

            release_gameinits();
            state = gameinits();
            // something went wrong...
            if (!state)
            {
              quitengine(EXIT_FAILURE);
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


            fprintf(stdout,"### computing benchhyatt24 depth %d: ###\n", SD);  
            fprintf(stdout,"### work-groups: %" PRIu64 " ###\n", threadsX*threadsY);  
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile,"### computing benchhyatt24 depth %d: ###\n", SD);  
              fprintdate(LogFile);
              fprintf(LogFile,"### work-groups: %" PRIu64 " ###\n", threadsX*threadsY);  
            }

            start = get_time();
           
            rootsearch(BOARD, STM, SD);

            end = get_time();   
            elapsed = end-start;
            elapsed += 1;
            elapsed/=1000;

            // collect results
            timearrall[iter]+= timearr[iter] = elapsed;
            npsarr[iter] = (u64)((double)ABNODECOUNT/elapsed);
            workerssarr[iter] = threadsX*threadsY;

            iter++;

            if (threadsX>=x)
              break;
            if (threadsX*2>x)
              threadsX = x; 
            else
              threadsX*=2; 
          }
          if (threadsY>=y)
            break;
          if (threadsY*2>y)
            threadsY = y; 
          else
            threadsY*=2; 
        }
        // print results
        fprintf(stdout,"### workers\t#nps\t\t#nps speedup\t#time in s\t#ttd speedup \t#relative ttd speedup ###\n");
        fprintf(stdout,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[0], npsarr[0], (double)1, timearr[0], (double)1, (double)1);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"### workers\t#nps\t\t#nps speedup\t#time in s\t#ttd speedup\t#relative ttd speedup ###\n");
          fprintdate(LogFile);
          fprintf(LogFile,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[0], npsarr[0], (double)1, timearr[0], (double)1, (double)1);
        }
        for (int i=1;i<iter;i++)
        {
          fprintf(stdout,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[i], npsarr[i], (double)npsarr[i]/(double)npsarr[0], timearr[i], timearr[0]/timearr[i], timearr[i-1]/timearr[i]);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile,"### %"PRIu64"\t\t%"PRIu64"\t\t%lf\t%lf\t%lf\t%lf \n",workerssarr[i], npsarr[i], (double)npsarr[i]/(double)npsarr[0], timearr[i], timearr[0]/timearr[i], timearr[i-1]/timearr[i]);
          }
        }
      }
      // print overall results
      fprintf(stdout,"#\n");
      fprintf(stdout,"# overall results\n");
      fprintf(stdout,"### workers\t#ttd speedup\t#rel ttd speedup ###\n");
      fprintf(stdout,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[0], (double)1, (double)1);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#\n");
        fprintf(LogFile,"# overall results\n");
        fprintf(LogFile,"### workers\t#ttd speedup\t#rel ttd speedup ###\n");
        fprintf(LogFile,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[0], (double)1, (double)1);
      }
      for (int i=1;i<iter;i++)
      {
        fprintf(stdout,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[i], timearrall[0]/timearrall[i], timearrall[i-1]/timearrall[i]);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile,"### %"PRIu64"\t\t%lf\t%lf \n",workerssarr[i], timearrall[0]/timearrall[i], timearrall[i-1]/timearrall[i]);
        }
      }
      //reset 
      release_gameinits();
      state = read_and_init_config(configfile);
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
      }
      state = gameinits();
      // something went wrong...
      if (!state)
      {
        quitengine(EXIT_FAILURE);
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
          fprintf(stdout,"Error (opening logfile zeta.log): log\n");
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
}
