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

#include <stdio.h>      // for file io

#include "timer.h"
#include "zeta.h"       // for global vars

static cl_int status = 0;
cl_uint deviceListSize;

cl_uint maxDims = 3;
size_t globalThreads[3]; 
size_t localThreads[3];

s32 temp = 0;
u64 templong = 0;
u64 ttbits = 0;

char *coptions = "";
//char *coptions = "-cl-strict-aliasing";
//char *coptions = "-cl-opt-disable";

void print_debug(char *debug);

// initialize OpenCL device, called once per game
bool cl_init_device(char *kernelname)
{
  context = NULL;
  kernel  = NULL;
  program = NULL;
  commandQueue = NULL;

  status = clGetPlatformIDs(256, NULL, &numPlatforms);
  if(status!=CL_SUCCESS)
  {
      print_debug((char *)"Error: Getting Platforms. (clGetPlatformsIDs)\n");
      return false;
  }
  if(numPlatforms>0)
  {
    cl_platform_id* platforms = (cl_platform_id *) malloc(numPlatforms);
    // get platforms
    status = clGetPlatformIDs(numPlatforms, platforms, NULL);
    if(status!=CL_SUCCESS)
    {
        print_debug((char *)"Error: Getting Platform Ids. (clGetPlatformsIDs)\n");
        return false;
    }
    // get our OpenCL platform
    platform = platforms[opencl_platform_id];
  }
  if(platform==NULL)
  {
    print_debug((char *)"NULL platform found so Exiting Application.\n");
    return false;
  }
  status = clGetDeviceIDs(platform, 
                          CL_DEVICE_TYPE_ALL, 
                          0, 
                          NULL, 
                          &deviceListSize);
  if(status!=CL_SUCCESS) 
	{  
	  print_debug((char *)"Error: Getting Device IDs (device list size, clGetDeviceIDs)\n");
	  return false;
	}
	if(deviceListSize==0)
	{
		print_debug((char *)"Error: No devices found.\n");
		return false;
	}
  devices = (cl_device_id *)malloc(deviceListSize * sizeof(cl_device_id));
  // get devices
  status = clGetDeviceIDs(platform, 
                          CL_DEVICE_TYPE_ALL, 
                          deviceListSize, 
                          devices, 
                          NULL);
  if(status!=CL_SUCCESS) 
  {  
    print_debug((char *)"Error: Getting Device IDs (devices, clGetDeviceIDs)\n");
    return false;
  }
  // init context properties
  cps[0] = CL_CONTEXT_PLATFORM;
  cps[1] = (cl_context_properties)platform;
  cps[2] = 0;
  // create context for our device
  context = clCreateContext(cps, 
                            1, 
                            &devices[opencl_device_id], 
                            NULL, 
                            NULL, 
                            &status);
  if(status!=CL_SUCCESS) 
  {  
    print_debug((char *)"Error: Creating Context Info (cps, clCreateContext)\n");
    return false;
  }
  // build OpenCL program object
  if (program==NULL)
  {
    const char *content = zeta_cl;
    program = clCreateProgramWithSource(
                            	          context, 
                                        1, 
                                        &content,
                              		      &zeta_cl_len,
                                        &status);
    if(status!=CL_SUCCESS) 
    { 
      print_debug((char *)"Error: Loading Binary into cl_program (clCreateProgramWithBinary)\n");
      return false;
    }   
    // create program for all the devices specified */
    status = clBuildProgram(program, 1, &devices[opencl_device_id], coptions, NULL, NULL);
    // get build log and print
    if(status!=CL_SUCCESS) 
    { 
      char* build_log=0;
      size_t log_size=0;
      FILE 	*temp=0;

	    print_debug((char *)"Error: Building Program (clBuildProgram)\n");
      // shows the log
      // first call to know the proper size
      clGetProgramBuildInfo(program, devices[opencl_device_id], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
      build_log = (char *) malloc(log_size+1);
      // second call to get the log
      status = clGetProgramBuildInfo(program, devices[opencl_device_id], CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
      //build_log[log_size] = '\0';
      temp = fopen("zeta.log", "a");
      fprintdate(temp);
      fprintf(temp, "buildlog: %s \n", build_log);
      fclose(temp);
      if(status!=CL_SUCCESS) 
      { 
        print_debug((char *)"Error: Building Log (clGetProgramBuildInfo)\n");
      }
      return false;
    }
  }

  // create kernel
  kernel = clCreateKernel(program, kernelname, &status);
  if(status!=CL_SUCCESS) 
  {  
    print_debug((char *)"Error: Creating Kernel for gpu. (clCreateKernel)\n");
    return false;
  }

  // create command queue
  commandQueue = clCreateCommandQueue(
		                                   context, 
                                       devices[opencl_device_id], 
                                       0, 
                                       &status);
  if(status!=CL_SUCCESS) 
	{ 
	  print_debug((char *)"Creating Command Queue. (clCreateCommandQueue)\n");
	  return false;
	}

  // create memory buffers
  GLOBAL_BOARD_Buffer = clCreateBuffer(
                          			      context, 
                                      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                      sizeof(Bitboard) * 8,
                                      GLOBAL_BOARD, 
                                      &status);
  if(status!=CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_Buffer)\n");
		return false;
	}

  GLOBAL_COUNTERS_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(u64) * totalWorkUnits * 64,
                                    COUNTERSZEROED, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }

  GLOBAL_RNUMBERS_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                    sizeof(u32) * totalWorkUnits * 64,
                                    RNUMBERS, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_RNUMBERS_Buffer)\n");
    return false;
  }

  GLOBAL_PV_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(Move) * 1024,
                                    PVZEROED, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_PV_Buffer)\n");
    return false;
  }

  GLOBAL_globalbbMoves_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE,
                                    sizeof(Bitboard) * totalWorkUnits * MAXPLY * 64,
                                    NULL, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_globalbbMoves_Buffer)\n");
    return false;
  }

  GLOBAL_HASHHISTORY_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       sizeof(Hash) * totalWorkUnits * MAXGAMEPLY,
                                       GLOBAL_HASHHISTORY, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_HASHHISTORY_Buffer)\n");
    return false;
  }

  GLOBAL_bbInBetween_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       sizeof(Bitboard) * 64 * 64,
                                       bbInBetween, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_bbInBetween_Buffer)\n");
    return false;
  }

  GLOBAL_bbLine_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       sizeof(Bitboard) * 64 * 64,
                                       bbLine, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_bbLine_Buffer)\n");
    return false;
  }

  // initialize transposition table
  ttbits = 0;
  u64 mem = (max_memory*1024*1024)/(sizeof(TTE));
  if (max_memory>0&&memory_slots>0)
  {
    mem = (max_memory*1024*1024)/(sizeof(TTE));

    while ( mem >>= 1)   // get msb
      ttbits++;
    mem = 1ULL<<ttbits;   // get number of tt entries
    ttbits=mem;
  }
  else
  {
    mem = 1;
    ttbits = 0x2;
  }

  if (max_memory<1||memory_slots<1)
    mem = 1;

  GLOBAL_TT1_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(TTE) * mem,
                                    TT, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_TT1_Buffer)\n");
    return false;
  }

  if (max_memory<1||memory_slots<2)
    mem = 1;

  GLOBAL_TT2_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(TTE) * mem,
                                    TT, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_TT2_Buffer)\n");
    return false;
  }

  if (max_memory<1||memory_slots<3)
    mem = 1;

  GLOBAL_TT3_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(TTE) * mem,
                                    TT, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_TT3_Buffer)\n");
    return false;
  }

/*
  if (max_memory<1||memory_slots<4)
    mem = 1;

  GLOBAL_TT4_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(TTE) * mem,
                                    TT, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_TT4_Buffer)\n");
    return false;
  }
*/

  GLOBAL_Killer_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(TTMove) * totalWorkUnits * MAXPLY,
                                    KILLERZEROED, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_Killer_Buffer)\n");
    return false;
  }

  GLOBAL_Counter_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(TTMove) * totalWorkUnits * 64 * 64,
                                    COUNTERZEROED, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_Counter_Buffer)\n");
    return false;
  }

  u32 finito = 0x0;
  GLOBAL_finito_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(u32) * 1,
                                    &finito, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_finito_Buffer)\n");
    return false;
  }

  return true;
}
// write OpenCL memory buffers, called every search run
bool cl_init_objects() {

  // write buffers
  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_BOARD_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(Bitboard) * 8,
                                GLOBAL_BOARD, 
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_BOARD_Buffer)\n");
    return false;
  }

  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_COUNTERS_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(u64) * totalWorkUnits * 64,
                                COUNTERSZEROED, 
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }

  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_RNUMBERS_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(u32) * totalWorkUnits * 64,
                                RNUMBERS, 
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_RNUMBERS_Buffer)\n");
    return false;
  }

  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_PV_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(Move) * 1024,
                                PVZEROED, 
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_PV_Buffer)\n");
    return false;
  }

  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_HASHHISTORY_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(Hash) * totalWorkUnits * MAXGAMEPLY,
                                GLOBAL_HASHHISTORY, 
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_HASHHISTORY_Buffer)\n");
    return false;
  }

  u32 finito = 0x0;
  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_finito_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(u32) * 1,
                                &finito, 
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_finito_Buffer)\n");
    return false;
  }

  // flush command queue
  status = clFlush(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: flushing the memory writes. (clFlush)\n");
    return false;
  }

  // wait for queue to finish memory write ops
  status = clFinish(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for memory writes run to finish. (clFinish)\n");
    return false;
  }


	return true;
}
// run OpenCL bestfirst kernel, every search
bool cl_run_alphabeta(bool stm, s32 depth, u64 nodes)
{
  s32 i = 0;
  // set kernel arguments
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_COUNTERS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_RNUMBERS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_RNUMBERS_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PV_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PV_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_globalbbMoves_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_globalbbMoves_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_HASHHISTORY_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_HASHHISTORY_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_bbInBetween_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_bbInBetween_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_bbLine_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_bbLine_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_TT1_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_TT1_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_TT2_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_TT2_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_TT3_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_TT3_Buffer)\n");
    return false;
  }
  i++;

/*
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_TT4_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_TT4_Buffer)\n");
    return false;
  }
  i++;
*/

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_Killer_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_Killer_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_Counter_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_Counter_Buffer)\n");
    return false;
  }
  i++;

  temp = (s32)stm;
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&temp);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (stm)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&PLY);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (ply_init)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&depth);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (search_depth)\n");
    return false;
  }
  i++;


  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_ulong), 
                          (void *)&nodes);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (max_nodes)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_ulong), 
                          (void *)&ttbits);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (ttindex)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_ulong), 
                          (void *)&memory_slots);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (slots)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_finito_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_finito_Buffer)\n");
    return false;
  }
  i++;

  // enqueue a kernel run call.
  globalThreads[0] = threadsX;
  globalThreads[1] = threadsY;
  globalThreads[2] = threadsZ;

  localThreads[0]  = 1;
  localThreads[1]  = 1;
  localThreads[2]  = threadsZ;

  status = clEnqueueNDRangeKernel(
	                                 commandQueue,
                                   kernel,
                                   maxDims,
                                   NULL,
                                   globalThreads,
                                   localThreads,
                                   0,
                                   NULL,
                                   NULL);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Enqueueing kernel onto command queue. (clEnqueueNDRangeKernel)\n");
    return false;
  }

  // flush command queueu
  status = clFlush(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: flushing the Kernel. (clFlush)\n");
    return false;
  }

  // wait for kernel to finish execution
  status = clFinish(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for kernel run to finish. (clFinish)\n");
    return false;
  }

  return true;
}
// run OpenCL bestfirst kernel, every search
bool cl_run_perft(bool stm, s32 depth)
{
  s32 i = 0;
  // set kernel arguments
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_COUNTERS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_globalbbMoves_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_globalbbMoves_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_HASHHISTORY_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_HASHHISTORY_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_bbInBetween_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_bbInBetween_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_bbLine_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_bbLine_Buffer)\n");
    return false;
  }
  i++;

  temp = (s32)stm;
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&temp);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (stm)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&PLY);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (ply_init)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&depth);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (search_depth)\n");
    return false;
  }
  i++;


  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_ulong), 
                          (void *)&MaxNodes);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (max_nodes)\n");
    return false;
  }
  i++;

  // enqueue a kernel run call.
  globalThreads[0] = 1;
  globalThreads[1] = 1;
  globalThreads[2] = threadsZ;

  localThreads[0]  = 1;
  localThreads[1]  = 1;
  localThreads[2]  = threadsZ;

  status = clEnqueueNDRangeKernel(
	                                 commandQueue,
                                   kernel,
                                   maxDims,
                                   NULL,
                                   globalThreads,
                                   localThreads,
                                   0,
                                   NULL,
                                   NULL);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Enqueueing kernel onto command queue. (clEnqueueNDRangeKernel)\n");
    return false;
  }

  // flush command queueu
  status = clFlush(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: flushing the Kernel. (clFlush)\n");
    return false;
  }

  // wait for kernel to finish execution
  status = clFinish(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for kernel run to finish. (clFinish)\n");
    return false;
  }

  return true;
}

// copy and release memory from device
bool cl_read_memory()
{
//  cl_event events[2];

  // copy counters buffer
  status = clEnqueueReadBuffer(
                                commandQueue,
                                GLOBAL_COUNTERS_Buffer,
                                CL_TRUE,
                                0,
                                totalWorkUnits * 64 * sizeof(u64),
                                COUNTERS,
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueReadBuffer failed. (GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }

  // flush command queue
  status = clFlush(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: flushing the memory reads. (clFlush)\n");
    return false;
  }

  // wait for memory reads to finish execution
  status = clFinish(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for memory reads run to finish. (clFinish)\n");
    return false;
  }

/*
  // wait for the read buffer to finish execution
  status = clWaitForEvents(1, &events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for read buffer call to finish. (GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }
  status = clReleaseEvent(events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Release event object.(GLOBAL_COUNTERS_Buffer)\n");
    return false;
  }
*/
  // copy PV buffer
  status = clEnqueueReadBuffer(
                                commandQueue,
                                GLOBAL_PV_Buffer,
                                CL_TRUE,
                                0,
                                1024 * sizeof(Move),
                                PV,
                                0,
                                NULL,
                                NULL);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueReadBuffer failed. (GLOBAL_PV_Buffer)\n");
    return false;
  }

  // flush command queue
  status = clFlush(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: flushing the memory reads. (clFlush)\n");
    return false;
  }

  // wait for memory reads to finish execution
  status = clFinish(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for memory reads run to finish. (clFinish)\n");
    return false;
  }
/*
  // wait for the read buffer to finish execution
  status = clWaitForEvents(1, &events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for read buffer call to finish. (GLOBAL_PV_Buffer)\n");
    return false;
  }
  status = clReleaseEvent(events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Release event object.(GLOBAL_PV_Buffer)\n");
    return false;
  }
*/


/* keep build program across game play

  status = clReleaseProgram(program);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseProgram\n");
    return false; 
  }
*/

	return true;
}
// release OpenCL device
bool cl_release_device() {

  // release memory buffers
  status = clReleaseMemObject(GLOBAL_BOARD_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_COUNTERS_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_COUNTERS_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_RNUMBERS_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_RNUMBERS_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_PV_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PV_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_globalbbMoves_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_globalbbMoves_Buffer)\n");
    return false; 
  }

	status = clReleaseMemObject(GLOBAL_HASHHISTORY_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_HASHHISTORY_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_bbInBetween_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_bbInBetween_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_bbLine_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_bbLine_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_TT1_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TT1_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_TT2_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TT2_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_TT3_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TT3_Buffer)\n");
		return false; 
	}

/*
	status = clReleaseMemObject(GLOBAL_TT4_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TT4_Buffer)\n");
		return false; 
	}
*/

	status = clReleaseMemObject(GLOBAL_Killer_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_Killer_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_Counter_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_Counter_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_finito_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_finito_Buffer)\n");
		return false; 
	}

  // release cl objects
  status = clReleaseKernel(kernel);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseKernel \n");
    return false; 
  }

  status = clReleaseCommandQueue(commandQueue);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseCommandQueue\n");
    return false;
  }

  status = clReleaseProgram(program);
  program = NULL;
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseProgram\n");
    return false; 
  }

  status = clReleaseContext(context);
  context = NULL;
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseContext\n");
    return false;
  }

	return true;
}
// debug printing
void print_debug(char *debug)
{
  FILE 	*Stats;
  Stats = fopen("zeta.log", "a");
  fprintdate(Stats);
  fprintf(Stats, "%s", debug);
  fprintdate(Stats);
  fprintf(Stats, "status:%i\n",status);
  if (status == CL_DEVICE_NOT_AVAILABLE)
      fprintf(Stats, "CL_DEVICE_NOT_AVAILABLE\n");
  fclose(Stats);
}

