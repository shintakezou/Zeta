/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2019
  License:      GPL >= v2

  Copyright (C) 2011-2019 Srdja Matovic

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
#include <stdlib.h>       // for malloc free
#include <getopt.h>       // for getopt_long

#include "bit.h"          // bit functions
#include "bitboard.h"     // bitboard related functions
#include "clconfig.h"     // configure OpenCL settings
#include "clquery.h"      // query OpenCL devices
#include "clrun.h"        // OpenCL run functions
#include "io.h"           // various IO and format functions
#include "search.h"       // rootsearch and perft
#include "test.h"         // selftest
#include "timer.h"        // timer functions
#include "types.h"        // types and defaults and macros 
#include "xboard.h"       // xboard protocol command loop

#include "zetacl.h"       // OpenCL source file zeta.cl as string
#include "zetaclperft.h"  // OpenCL source file zetaperft.cl as string

// global variables
// io
FILE *LogFile = NULL;         // logfile for debug
char *Line;                   // for fgetting the input on stdin
char *Command;                // for pasring the xboard command
char *Fen;                    // for storing the fen chess baord string
// counters
u64 ABNODECOUNT         = 0;
u64 TTHITS              = 0;
u64 TTSCOREHITS         = 0;
u64 IIDHITS             = 0;
u64 MOVECOUNT           = 0;
// config file
char configfile[256] = "config.txt";
u64 threadsX            =  1;
u64 threadsY            =  1;
const u64 threadsZ      = 64; // fix value, run z threads per work-group
u64 totalWorkUnits      =  1;
s64 nodes_per_second    =  0;
s64 nps_current         =  0;
u64 tt1_memory          =  0;
u64 tt2_memory          =  0;
//u64 tt3_memory          =  0;
s32 opencl_device_id    =  0;
s32 opencl_platform_id  =  0;
s32 opencl_user_device  = -1;
s32 opencl_user_platform= -1;
s32 opencl_gpugen       =  1;
// further config
s32 search_depth        =  0;
// timers
double start        = 0;
double end          = 0;
double elapsed      = 0;
bool TIMEOUT        = false;  // global value for time control*/
// time control in milli-seconds
s32 timemode    = 0;      // 0 = single move, 1 = conventional clock, 2 = ics
s32 MovesLeft   = 1;      // moves left unit nex time increase
s32 MaxMoves    = 1;      // moves to play in time frame
double TimeInc  = 0;      // time increase
double TimeBase = 5*1000; // time base for conventional inc, 5s default
double TimeLeft = 5*1000; // overall time on clock, 5s default
double MaxTime  = 5*1000; // max time per move
u64 MaxNodes    = 1;
// game state
bool STM        = WHITE;  // site to move
s32 SD          = MAXPLY; // max search depth*/
s32 GAMEPLY     = 0;      // total ply, considering depth via fen string
s32 PLY         = 0;      // engine specifix ply counter
// game history
Move *MoveHistory;
Hash *HashHistory;
Cr *CRHistory;
Bitboard *HMCHistory;
// Quad Bitboard
// based on http://chessprogramming.wikispaces.com/Quad-Bitboards
// by Gerd Isenberg
Bitboard BOARD[7];
/* quad bitboard array index definition
  0   pieces white
  1   piece type first bit
  2   piece type second bit
  3   piece type third bit
  4   piece moved flags, for castle rigths
  5   zobrist hash
  6   half move clock
*/
// for exchange with OpenCL device
Bitboard *GLOBAL_BOARD = NULL;
TTE *TT1ZEROED = NULL;
ABDADATTE *TT2ZEROED = NULL;
//TTE *TT3ZEROED = NULL;
u64 *COUNTERS = NULL;
u32 *RNUMBERS = NULL;
u64 *COUNTERSZEROED = NULL;
Move *PV = NULL;
Move *PVZEROED = NULL;
TTMove *KILLERZEROED = NULL;
TTMove *COUNTERZEROED = NULL;
Hash *GLOBAL_HASHHISTORY = NULL;
// OpenCL memory buffer objects
cl_mem   GLOBAL_BOARD_Buffer;
cl_mem   GLOBAL_globalbbMoves1_Buffer;
cl_mem   GLOBAL_globalbbMoves2_Buffer;
cl_mem	 GLOBAL_COUNTERS_Buffer;
cl_mem   GLOBAL_RNUMBERS_Buffer;
cl_mem	 GLOBAL_PV_Buffer;
cl_mem	 GLOBAL_HASHHISTORY_Buffer;
cl_mem	 GLOBAL_bbInBetween_Buffer;
cl_mem	 GLOBAL_bbLine_Buffer;
cl_mem   GLOBAL_TT1_Buffer;
cl_mem   GLOBAL_TT2_Buffer;
//cl_mem   GLOBAL_TT3_Buffer;
cl_mem   GLOBAL_Killer_Buffer;
cl_mem   GLOBAL_Counter_Buffer;
cl_mem   GLOBAL_RScore_Buffer;
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

// #############################
// ###        inits          ###
// #############################
// innitialize engine memory
bool engineinits(void)
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
bool gameinits(void)
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
  CRHistory = (Cr*)calloc(MAXGAMEPLY, sizeof(Cr));
  if (CRHistory==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): Cr CRHistory[%d]",
            MAXGAMEPLY);
    return false;
  }
  HMCHistory = (Bitboard*)calloc(MAXGAMEPLY, sizeof(Bitboard));
  if (HMCHistory==NULL) 
  {
    fprintf(stdout,"Error (memory allocation failed): Bitboard HMCHistory[%d]",
            MAXGAMEPLY);
    return false;
  }

  // allocate memory by config.txt
  GLOBAL_BOARD = (Bitboard*)malloc(7*sizeof(Bitboard));
  if (GLOBAL_BOARD==NULL)
  {
    fprintf(stdout, "memory alloc, GLOBAL_BOARD, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, GLOBAL_BOARD, failed\n");
    }
    return false;
  }
  RNUMBERS = (u32*)calloc(totalWorkUnits*threadsZ, sizeof(u32));
  if (RNUMBERS==NULL)
  {
    fprintf(stdout, "memory alloc, RNUMBERS, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, RNUMBERS, failed\n");
    }
    return false;
  }
  COUNTERS = (u64*)calloc(totalWorkUnits*threadsZ, sizeof(u64));
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
  COUNTERSZEROED = (u64*)calloc(totalWorkUnits*threadsZ, sizeof(u64));
  if (COUNTERSZEROED==NULL)
  {
    fprintf(stdout, "memory alloc, COUNTERSZEROED, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, COUNTERSZEROED, failed\n");
    }
    return false;
  }
  PV = (Move*)calloc(MAXPLY, sizeof(Move));
  if (PV==NULL)
  {
    fprintf(stdout, "memory alloc, PV, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, PV, failed\n");
    }
    return false;
  }
  PVZEROED = (Move*)calloc(MAXPLY, sizeof(Move));
  if (PVZEROED==NULL)
  {
    fprintf(stdout, "memory alloc, PVZEROED, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, PVZEROED, failed\n");
    }
    return false;
  }
  KILLERZEROED = (Move*)calloc(totalWorkUnits*MAXPLY, sizeof(Move));
  if (KILLERZEROED==NULL)
  {
    fprintf(stdout, "memory alloc, KILLERZEROED, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, KILLERZEROED, failed\n");
    }
    return false;
  }
  COUNTERZEROED = (Move*)calloc(totalWorkUnits*64*64, sizeof(Move));
  if (COUNTERZEROED==NULL)
  {
    fprintf(stdout, "memory alloc, COUNTERZEROED, failed\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "memory alloc, COUNTERZEROED, failed\n");
    }
    return false;
  }
  GLOBAL_HASHHISTORY = (Hash*)calloc(totalWorkUnits*MAXGAMEPLY , sizeof(Hash));
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
  // initialize transposition table, TT1
  u64 mem = (tt1_memory*1024*1024)/(sizeof(TTE));
  u64 ttbits = 0;
  if (tt1_memory>0)
  {
    while ( mem >>= 1)   // get msb
      ttbits++;
    mem = 1ULL<<ttbits;   // get number of tt entries
    ttbits=mem;
  }
  else
    mem = 1;
  TT1ZEROED = (TTE*)calloc(mem,sizeof(TTE));
  if (TT1ZEROED==NULL)
  {
    fprintf(stdout,"Error (tt1 hash table memory allocation on cpu, %" PRIu64 " mb, failed): memory\n", tt1_memory);
    return false;
  }
  // initialize abdada transposition table, TT2
  mem = (tt2_memory*1024*1024)/(sizeof(ABDADATTE));
  ttbits = 0;
  if (tt2_memory>0)
  {
    while ( mem >>= 1)   // get msb
      ttbits++;
    mem = 1ULL<<ttbits;   // get number of tt entries
    ttbits=mem;
  }
  else
    mem = 1;
  TT2ZEROED = (ABDADATTE*)calloc(mem,sizeof(ABDADATTE));
  if (TT2ZEROED==NULL)
  {
    fprintf(stdout,"Error (tt2 hash table memory allocation on cpu, %" PRIu64 " mb, failed): memory\n", tt2_memory);
    return false;
  }
/*
  // initialize transposition table, TT3
  mem = (tt3_memory*1024*1024)/(sizeof(TTE));
  ttbits = 0;
  if (tt3_memory>0)
  {
    while ( mem >>= 1)   // get msb
      ttbits++;
    mem = 1ULL<<ttbits;   // get number of tt entries
    ttbits=mem;
  }
  else
    mem = 1;
  TT3ZEROED = (TTE*)calloc(mem,sizeof(TTE));
  if (TT3ZEROED==NULL)
  {
    fprintf(stdout,"Error (tt3 hash table memory allocation on cpu, %" PRIu64 " mb, failed): memory\n", tt1_memory);
    return false;
  }
*/
  return true;
}
void release_gameinits()
{
  // release memory
  free(MoveHistory);
  free(HashHistory);
  free(CRHistory);
  free(HMCHistory);

  free(GLOBAL_BOARD);
  free(RNUMBERS);
  free(COUNTERS);
  free(COUNTERSZEROED);
  free(PV);
  free(PVZEROED);
  free(KILLERZEROED);
  free(COUNTERZEROED);
  free(GLOBAL_HASHHISTORY);
  free(TT1ZEROED);
  free(TT2ZEROED);
}
void release_configinits()
{
/*
  // opencl related
  free(GLOBAL_BOARD);
  free(RNUMBERS);
  free(COUNTERS);
  free(COUNTERSZEROED);
  free(PV);
  free(PVZEROED);
  free(KILLERZEROED);
  free(COUNTERZEROED);
  free(GLOBAL_HASHHISTORY);
  free(TT);
*/
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
// print engine info to console
static void print_version(void)
{
  fprintf(stdout,"Zeta version: %s\n",VERSION);
  fprintf(stdout,"Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"Copyright (C) 2011-2019 Srdja Matovic, Montenegro\n");
  fprintf(stdout,"This is free software, licensed under GPL >= v2\n");
}
// engine options and usage
void print_help(void)
{
  fprintf(stdout,"\n");
  fprintf(stdout,"Zeta, experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"### WARNING\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"GPUs may have an operating system and driver specific timeout for computation.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Nvidia GPUs may have an driver specific timeout of 5 seconds when display is\n");
  fprintf(stdout,"connected.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"AMD GPUs may have an driver specific timeout of about 360 to 3600 seconds.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Therefore it is recommended to run the engine on an discrete GPU, without\n");
  fprintf(stdout,"display connected.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"To increase the Windows OS GPU timeout from 2 to 20 seconds, double-click the\n");
  fprintf(stdout,".reg file \"SetWindowsGPUTimeoutTo20s.reg\" and reboot your OS.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"### Tested Platforms\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"Windows 7 64 bit (mingw compiler) and Linux 64 bit (gcc) OS on x86-64 host\n");
  fprintf(stdout,"with AMD CPU/GPU and Intel CPU/GPU and Nvidia GPU as OpenCL device.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"### Usage\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"First make sure you have an working OpenCL Runtime Environment,\n");
  fprintf(stdout,"start the zeta executable in command line with -dl option to list\n");
  fprintf(stdout,"all available OpenCL devices on host:\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"zeta -dl\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Second check the OpenCL device and create a config file for the engine:\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"zeta -p 0 -d 0 --guessconfigx\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Where p is the selected platform id and d is the selected device id.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Third rename the created config file to config.txt and start the engine.\n");
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
  fprintf(stdout,"The implemented Time Control is a bit shacky, tuned for 40 moves in 4 minutes\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Not supported Xboard commands:\n");
  fprintf(stdout,"analyze        // enter analyze mode\n");
  fprintf(stdout,"?              // move now\n");
  fprintf(stdout,"draw           // handle draw offers\n");
  fprintf(stdout,"hard/easy      // turn on/off pondering\n");
  fprintf(stdout,"hint           // give user a hint move\n");
  fprintf(stdout,"bk             // book lines\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"Non-Xboard commands:\n");
  fprintf(stdout,"perft          // perform a performance test, depth set by sd command\n");
  fprintf(stdout,"selftest       // run an internal test\n");
  fprintf(stdout,"help           // print usage info\n");
  fprintf(stdout,"log            // turn log on\n");
  fprintf(stdout,"benchsmp       // init with new and sd and st commands\n");
  fprintf(stdout,"               // runs an benchmark for parallel speedup\n");
  fprintf(stdout,"benchhyatt24   // init with sd and st commands\n");
  fprintf(stdout,"               // runs an smp benchmark on Hyatt24 positions\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout,"### Options\n");
  fprintf(stdout,"################################################################################\n");
  fprintf(stdout," -l, --log          Write output/debug to file zeta.log\n");
  fprintf(stdout," -v, --version      Print Zeta version info.\n");
  fprintf(stdout," -h, --help         Print Zeta program usage help.\n");
  fprintf(stdout," -s, --selftest     Run an internal test, usefull after compile.\n");
  fprintf(stdout," -pl                List all OpenCL Platforms on Host\n");
  fprintf(stdout," -dl                List all OpenCL Devices on Host\n");
  fprintf(stdout," -p 0               Set Platform ID to 0 for guessconfig \n");
  fprintf(stdout," -d 0               Set Device ID to 0 for guessconfig \n");
  fprintf(stdout," --guessconfig      Guess minimal config for OpenCL devices\n");
  fprintf(stdout," --guessconfigx     Guess optimal config for OpenCL devices\n");
  fprintf(stdout,"\n");
}
// Zeta, experimental chess engine written in OpenCL.
int main(int argc, char* argv[])
{
  // for get opt
  s32 c;
  static struct option long_options[] = 
  {
    {"help", 0, 0, 0},
    {"version", 0, 0, 0},
    {"selftest", 0, 0, 0},
    {"log", 0, 0, 0},
    {"guessconfig", 0, 0, 0},
    {"guessconfigx", 0, 0, 0},
    {"p", 1, 0, 0},
    {"d", 1, 0, 0},
    {"pl", 0, 0, 0},
    {"dl", 0, 0, 0},
    {NULL, 0, NULL, 0}
  };
  s32 option_index = 0;

  // no buffers
  setbuf (stdout, NULL);
  setbuf (stdin, NULL);
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stdin, NULL, _IONBF, 0);

  // turn log on
  for (c=1;c<argc;c++)
  {
    if (!strcmp(argv[c], "-l") || !strcmp(argv[c],"--log"))
    {
      // open/create log file
      LogFile = fopen("zeta.log", "a");
      if (LogFile==NULL) 
      {
        fprintf(stdout,"Error (opening logfile zeta.log): --log\n");
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
      case 0: // help
        print_help();
        exit(EXIT_SUCCESS);
        break;
      case 1: // version
        print_version();
        exit(EXIT_SUCCESS);
        break;
      case 2: // selftest
        if (!engineinits())
        {
          quitengine(EXIT_FAILURE);
        }
        if (!read_and_init_config(configfile))
        {
          quitengine(EXIT_FAILURE);
        }
        if (!gameinits())
        {
          quitengine(EXIT_FAILURE);
        }
        if (!cl_init_device("alphabeta_gpu"))
        {
          quitengine(EXIT_FAILURE);
        }
        selftest();
        quitengine(EXIT_SUCCESS);
        break;
      case 3: // log
        break;
      case 4: // guessconfig
        // init engine
        if (!engineinits())
            exit(EXIT_FAILURE);
        cl_guess_config(false);
        release_engineinits();
        exit(EXIT_SUCCESS);
        break;
      case 5: // guessconfigx
        // init engine
        if (!engineinits())
          exit(EXIT_FAILURE);
        cl_guess_config(true);
        release_engineinits();
        exit(EXIT_SUCCESS);
        break;
      case 6:
        // set user define platform
        if (optarg)
          opencl_user_platform = atoi(optarg);
       break;
      case 7:
        // set user define platform
        if (optarg)
          opencl_user_device = atoi(optarg);
       break;
      case 8: // list OpenCL platforms
        cl_platform_list();
        exit(EXIT_SUCCESS);
       break;
      case 9: // list OpenCL devices
        cl_device_list();
        exit(EXIT_SUCCESS);
       break;
      default: /* '?' */
        print_help();
        exit(EXIT_FAILURE);
    }
  }
  // print engine info to console
  fprintf(stdout,"#> Zeta %s\n",VERSION);
  fprintf(stdout,"#> Experimental chess engine written in OpenCL.\n");
  fprintf(stdout,"#> Copyright (C) 2011-2019 Srdja Matovic, Montenegro\n");
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
    fprintf(LogFile,"#> Copyright (C) 2011-2019 Srdja Matovic, Montenegro\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#> This is free software, licensed under GPL >= v2\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#> eninge is initialising...\n");  
    fprintdate(LogFile);
    fprintf(LogFile,"feature done=0\n");  
  }
  // init engine and game memory, read config ini file and init OpenCL device
  if (!engineinits())
  {
    quitengine(EXIT_FAILURE);
  }
  if (!read_and_init_config(configfile))
  {
    quitengine(EXIT_FAILURE);
  }
  if (!gameinits())
  {
    quitengine(EXIT_FAILURE);
  }
  if (!setboard(BOARD, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"))
  {
    quitengine(EXIT_FAILURE);
  }
  if (!cl_init_device("alphabeta_gpu"))
  {
    quitengine(EXIT_FAILURE);
  }
  if (!cl_run_alphabeta(STM, 0, 1))
  {
    quitengine(EXIT_FAILURE);
  }
  fprintf(stdout,"#> ...finished basic inits.\n");  
  if (LogFile) 
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#> ...finished basic inits.\n");  
  }
  // xboard protocol command loop
  xboard();
  // release memory, files and tables
  quitengine(EXIT_SUCCESS);
}

