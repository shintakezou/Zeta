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
#include <stdlib.h>       // for alloc
#include <string.h>       // for string compare 

#include "bit.h"          // bit functions
#include "bitboard.h"     // bitboard related functions
#include "clrun.h"        // OpenCL run functions
#include "search.h"       // rootsearch and perft
#include "timer.h"        // timer functions
#include "types.h"        // types and defaults and macros 
#include "zeta.h"         // for global vars and functions

void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply);

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

  fprintf(stdout,"#sqfrom:%" PRIu32 "\n",GETSQFROM(move));
  fprintf(stdout,"#sqto:%" PRIu32 "\n",GETSQTO(move));
  fprintf(stdout,"#sqcpt:%" PRIu32 "\n",GETSQCPT(move));
  fprintf(stdout,"#pfrom:%" PRIu32 "\n",GETPFROM(move));
  fprintf(stdout,"#pto:%" PRIu32 "\n",GETPTO(move));
  fprintf(stdout,"#pcpt:%" PRIu32 "\n",GETPCPT(move));
}
// move in algebraic notation, eg. e2e4, to internal packed move 
Move can2move(char *usermove, Bitboard *board, bool stm)
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

  // pack move
  move = MAKEMOVE((Move)sqfrom, (Move)sqto, (Move)sqcpt,(Move)pfrom, (Move)pto, (Move)pcpt);


  return move;
}
/* packed move to move in coordinate algebraic notation,
  e.g. 
  e2e4 
  e1c1 => when king, indicates castle queenside  
  e7e8q => indicates pawn promotion to queen
*/
void move2can(Move move, char * movec) 
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
  char movec[6];
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
void createfen(char *fenstring, Bitboard *board, bool stm, s32 gameply)
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
  Bitboard bbWork;

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
  // en passant flag
  bbWork = (~board[QBBPMVD]&0x000000FFFF000000);
  sq = (bbWork)?first1(bbWork):0x0;
  // file en passant
  if (sq)
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
  stringptr+=sprintf(stringptr, "%" PRIu64 "", board[QBBHMC]);
  stringptr+=sprintf(stringptr, " ");

  stringptr+=sprintf(stringptr, "%d", ((gameply+PLY)/2));
}
// set internal chess board presentation to fen string
bool setboard(Bitboard *board, char *fenstring)
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
  board[QBBHMC]    = hmc;

  // set en passant target square in piece moved flags
  tempchar = cep[0];
  file  = 0;
  rank  = 0;
  sq    = 0x0;
  if (tempchar != '-' && tempchar != '\0' && tempchar != '\n')
  {
    file  = cep[0] - 97;
    rank  = cep[1] - 49;
    sq    = MAKESQ(file, rank);
  }
  if (sq)
    board[QBBPMVD] &= CLRMASKBB(sq);

  // ply starts at zero
  PLY = 0;
  // game ply can be more
  GAMEPLY = fendepth*2+STM;

  // compute zobrist hash
  board[QBBHASH] = computehash(BOARD, STM);
  HashHistory[PLY] = board[QBBHASH];

  // store lastmove in history
  MoveHistory[PLY] = MOVENONE;

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
  char line[1024];

  fcfg = fopen(configfile, "r");
  if (fcfg == NULL)
  {
    fprintf(stdout,"Error (");
    fprintf(stdout, "%s file missing): ", configfile);
    fprintf(stdout, "try --guessconfig option to create a config.txt file ");
    fprintf(stdout, "or --help option for further options\n");

    fprintf(stdout,"tellusererror (");
    fprintf(stdout, "%s file missing): ", configfile);
    fprintf(stdout, "try --guessconfig option to create a config.txt file ");
    fprintf(stdout, "or --help option for further options\n");

    if (LogFile==NULL)
      LogFile = fopen("zeta.log", "a");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,"Error (");
      fprintf(LogFile, "%s file missing) ", configfile);
      fprintf(LogFile, "try --guessconfig option to create a config.txt file ");
      fprintf(LogFile, "or --help option for further options\n");
    }
    return false;
  }
  while (fgets(line, sizeof(line), fcfg))
  { 
    sscanf(line, "threadsX: %" PRIu64 ";", &threadsX);
    sscanf(line, "threadsY: %" PRIu64 ";", &threadsY);
    sscanf(line, "nodes_per_second: %" PRIi64 ";", &nodes_per_second);
    sscanf(line, "tt1_memory: %" PRIu64 ";", &tt1_memory);
    sscanf(line, "tt2_memory: %" PRIu64 ";", &tt2_memory);
//    sscanf(line, "tt3_memory: %" PRIu64 ";", &tt3_memory);
    sscanf(line, "opencl_platform_id: %d;", &opencl_platform_id);
    sscanf(line, "opencl_device_id: %d;", &opencl_device_id);
    sscanf(line, "opencl_gpugen: %d;", &opencl_gpugen);
  }
  fclose(fcfg);

  MaxNodes = (u64)nodes_per_second; 

  totalWorkUnits = threadsX*threadsY;

  return true;
}

