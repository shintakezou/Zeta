/*
  Name:         Zeta
  Description:  Experimental chess engine written in OpenCL.
  Author:       Srdja Matovic <s.matovic@app26.de>
  Created at:   2011-01-15
  Updated at:   2016
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

#include <stdio.h>      // for file io

#include "timer.h"
#include "zeta.h"       // for global vars

static cl_int status = 0;
cl_uint deviceListSize;

cl_uint maxDims = 3;
size_t globalThreads[3]; 
size_t localThreads[3];

s32 temp = 0;

extern s32 load_file_to_string(const char *filename, char **result);
void print_debug(char *debug);

// initialize OpenCL device, called once per game
bool cl_init_device()
{
  context = NULL;
  kernel  = NULL;
  program = NULL;

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
    print_debug((char *)"NULL platform found so Exiting Application.");
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
    status = clBuildProgram(program, 1, &devices[opencl_device_id], NULL, NULL, NULL);
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
  return true;
}
// initialize OpenCL objects, called every search run
bool cl_init_objects(char *kernelname) {

  cl_event events[2];

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
  GLOBAL_INIT_BOARD_Buffer = clCreateBuffer(
                          			      context, 
                                      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                      sizeof(Bitboard) * 7,
                                      GLOBAL_INIT_BOARD, 
                                      &status);
  if(status!=CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_INIT_BOARD_Buffer)\n");
		return false;
	}

  GLOBAL_BOARD_STACK_1_Buffer = clCreateBuffer(
                        	              context, 
                                        CL_MEM_READ_WRITE,
                                        sizeof(NodeBlock) * max_nodes_to_expand,
                                        NULL, 
                                        &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }

  // write first node to node tree buffer
  status = clEnqueueWriteBuffer(
                                commandQueue,
                                GLOBAL_BOARD_STACK_1_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(NodeBlock) * 1,
                                NODES,
                                0,
                                NULL,
                                &events[1]);
        
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clEnqueueWriteBuffer failed. (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }
  // wait for the write buffer to finish execution
  status = clWaitForEvents(1, &events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for write buffer call to finish. (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }
  status = clReleaseEvent(events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Release event object.(GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }

  temp = (memory_slots >= 2)? max_nodes_to_expand : 1;
  GLOBAL_BOARD_STACK_2_Buffer = clCreateBuffer(
                                                context, 
                                                CL_MEM_READ_WRITE,
                                                sizeof(NodeBlock) * temp,
                                                NULL, 
                                                &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STACK_2_Buffer)\n");
    return false;
  }   

  temp = (memory_slots >= 3)? max_nodes_to_expand : 1;
  GLOBAL_BOARD_STACK_3_Buffer = clCreateBuffer(
                                                context, 
                                                CL_MEM_READ_WRITE,
                                                sizeof(NodeBlock) * temp,
                                                NULL, 
                                                &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STACK_3_Buffer)\n");
    return false;
  }   

  COUNTERS_Buffer = clCreateBuffer(
                        		        context, 
                                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                    sizeof(u64) * 10,
                                    COUNTERS, 
                                    &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (COUNTERS_Buffer)\n");
    return false;
  }

  temp = BOARD_STACK_TOP;
  GLOBAL_BOARD_STAK_TOP_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       sizeof(cl_int) *  1,
                                       &temp, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STAK_TOP_Buffer)\n");
    return false;
  }

  temp = 0;
  GLOBAL_TOTAL_NODES_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       sizeof(cl_int) *  1,
                                       &temp, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_TOTAL_NODES_Buffer)\n");
    return false;
  }

  temp = 0;
  GLOBAL_FINISHED_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       sizeof(cl_int) * 1,
                                       &temp, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_FINISHED_Buffer)\n");
    return false;
  }


  GLOBAL_PID_MOVECOUNTER_Buffer = clCreateBuffer(
	                          		     context, 
                                     CL_MEM_READ_WRITE,
                                     sizeof(cl_int) * totalThreads * max_depth,
                                     NULL, 
                                     &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_MOVECOUNTER_Buffer)\n");
    return false;
  }

  GLOBAL_PID_TODOINDEX_Buffer = clCreateBuffer(
                        			       context, 
                                     CL_MEM_READ_WRITE,
                                     sizeof(cl_int) * totalThreads * max_depth,
                                     NULL, 
                                     &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_TODOINDEX_Buffer)\n");
    return false;
  }

  GLOBAL_PID_AB_SCORES_Buffer = clCreateBuffer(
                      			     context, 
                                 CL_MEM_READ_WRITE,
                                 sizeof(cl_int) * totalThreads * max_depth * 2,
                                 NULL, 
                                 &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_AB_SCORES_Buffer)\n");
    return false;
  }


  GLOBAL_PID_DEPTHS_Buffer = clCreateBuffer(
                        		     context, 
                                 CL_MEM_READ_WRITE,
                                 sizeof(cl_int) * totalThreads * max_depth,
                                 NULL, 
                                 &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_DEPTHS_Buffer)\n");
    return false;
  }

  GLOBAL_PID_MOVES_Buffer = clCreateBuffer(
                  			     context, 
                             CL_MEM_READ_WRITE,
                             sizeof(Move) * totalThreads * max_depth * MAXMOVES,
                             NULL, 
                             &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_MOVES_Buffer)\n");
    return false;
  }

  GLOBAL_PID_MOVE_HISTORY_Buffer = clCreateBuffer(
                              		     context, 
                                       CL_MEM_READ_WRITE,
                                       sizeof(Move) * totalThreads * max_depth,
                                       NULL, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_MOVE_HISTORY_Buffer)\n");
    return false;
  }

  GLOBAL_PID_CR_HISTORY_Buffer = clCreateBuffer(
                              		     context, 
                                       CL_MEM_READ_WRITE,
                                       sizeof(Move) * totalThreads * max_depth,
                                       NULL, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_CR_HISTORY_Buffer)\n");
    return false;
  }

  GLOBAL_HASHHISTORY_Buffer = clCreateBuffer(
                            			     context, 
                                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       sizeof(Hash) * totalThreads*1024,
                                       GLOBAL_HASHHISTORY, 
                                       &status);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clCreateBuffer (GLOBAL_HASHHISTORY_Buffer)\n");
    return false;
  }

  // create kernel
  kernel = clCreateKernel(program, kernelname, &status);
  if(status!=CL_SUCCESS) 
  {  
    print_debug((char *)"Error: Creating Kernel for bestfirst_gpu. (clCreateKernel)\n");
    return false;
  }
	return true;
}
// run OpenCL bestfirst kernel, every search
bool cl_run_search(bool stm, s32 depth)
{
  s32 i = 0;
  // set kernel arguments
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_INIT_BOARD_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_INIT_BOARD_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STACK_1_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STACK_2_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_2_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STACK_3_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_3_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&COUNTERS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (COUNTERS_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STAK_TOP_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. ( GLOBAL_BOARD_STAK_TOP_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_TOTAL_NODES_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. ( GLOBAL_TOTAL_NODES_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_FINISHED_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_FINISHED_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_MOVECOUNTER_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVECOUNTER_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_TODOINDEX_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_TODOINDEX_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_AB_SCORES_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_AB_SCORES_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_DEPTHS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_DEPTHS_Buffer)\n");
    return false;
  }
  i++;


  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_MOVES_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVES_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_MOVE_HISTORY_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVE_HISTORY_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_CR_HISTORY_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_CR_HISTORY_Buffer)\n");
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
    print_debug((char *)"Error: Setting kernel argument. (PLY)\n");
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
    print_debug((char *)"Error: Setting kernel argument. (max_ab_depth)\n");
    return false;
  }
  i++;

  temp = max_nodes_to_expand*memory_slots;
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&temp);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (max_nodes_to_expand)\n");
    return false;
  }
  i++;

  temp = max_nodes_to_expand;
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&temp);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. ( max_nodes_per_slot)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_long), 
                          (void *)&MaxNodes);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (MaxNodes)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&max_depth);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (max_depth)\n");
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

  // wair for kernel to finish execution
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
                          (void *)&GLOBAL_INIT_BOARD_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_INIT_BOARD_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STACK_1_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STACK_2_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_2_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STACK_3_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_3_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&COUNTERS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (COUNTERS_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_BOARD_STAK_TOP_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. ( GLOBAL_BOARD_STAK_TOP_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_TOTAL_NODES_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. ( GLOBAL_TOTAL_NODES_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_FINISHED_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_FINISHED_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_MOVECOUNTER_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVECOUNTER_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_TODOINDEX_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_TODOINDEX_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_AB_SCORES_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_AB_SCORES_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_DEPTHS_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_DEPTHS_Buffer)\n");
    return false;
  }
  i++;


  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_MOVES_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVES_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_MOVE_HISTORY_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVE_HISTORY_Buffer)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_mem), 
                          (void *)&GLOBAL_PID_CR_HISTORY_Buffer);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_CR_HISTORY_Buffer)\n");
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
    print_debug((char *)"Error: Setting kernel argument. (PLY)\n");
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
    print_debug((char *)"Error: Setting kernel argument. (max_ab_depth)\n");
    return false;
  }
  i++;

  temp = max_nodes_to_expand*memory_slots;
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&temp);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (max_nodes_to_expand)\n");
    return false;
  }
  i++;

  temp = max_nodes_to_expand;
  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&temp);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. ( max_nodes_per_slot)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_long), 
                          (void *)&MaxNodes);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (MaxNodes)\n");
    return false;
  }
  i++;

  status = clSetKernelArg(
                          kernel, 
                          i, 
                          sizeof(cl_int), 
                          (void *)&max_depth);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Setting kernel argument. (max_depth)\n");
    return false;
  }
  i++;

  // enqueue a kernel run call.
  globalThreads[0] = 1;
  globalThreads[1] = 1;
  globalThreads[2] = 1;

  localThreads[0]  = 1;
  localThreads[1]  = 1;
  localThreads[2]  = 1;

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

  // wair for kernel to finish execution
  status = clFinish(commandQueue);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for kernel run to finish. (clFinish)\n");
    return false;
  }

  return true;
}

// copy and release memory from device
bool cl_get_and_release_memory()
{
  cl_event events[2];

  // copy counters buffer
  status = clEnqueueReadBuffer(
                                commandQueue,
                                COUNTERS_Buffer,
                                CL_TRUE,
                                0,
                                10 * sizeof(u64),
                                COUNTERS,
                                0,
                                NULL,
                                &events[1]);

  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: clEnqueueReadBuffer failed. (CountersBuffer)\n");
    return false;
  }
  // wait for the read buffer to finish execution
  status = clWaitForEvents(1, &events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for read buffer call to finish. (CountersBuffer)\n");
    return false;
  }
  status = clReleaseEvent(events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Release event object.(CountersBuffer)\n");
    return false;
  }

  // copy first 256+1 nodes from tree, for bestmove selection
  status = clEnqueueReadBuffer(
                                commandQueue,
                                GLOBAL_BOARD_STACK_1_Buffer,
                                CL_TRUE,
                                0,
                                sizeof(NodeBlock) * MAXMOVES+1,
                                NODES,
                                0,
                                NULL,
                                &events[1]);

  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: clEnqueueReadBuffer failed. (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }
  // wait for the read buffer to finish execution
  status = clWaitForEvents(1, &events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Waiting for read buffer call to finish. (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }
  status = clReleaseEvent(events[1]);
  if(status!=CL_SUCCESS) 
  { 
    print_debug((char *)"Error: Release event object.(GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false;
  }

  // release cl objects
  status = clReleaseKernel(kernel);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseKernel \n");
    return false; 
  }
/* keep build program across game play

  status = clReleaseProgram(program);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseProgram\n");
    return false; 
  }
*/
  status = clReleaseCommandQueue(commandQueue);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseCommandQueue\n");
    return false;
  }

  status = clReleaseMemObject(GLOBAL_INIT_BOARD_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_INIT_BOARD_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_BOARD_STACK_1_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STACK_1_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_BOARD_STACK_2_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STACK_2_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(GLOBAL_BOARD_STACK_3_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STACK_3_Buffer)\n");
    return false; 
  }

  status = clReleaseMemObject(COUNTERS_Buffer);
  if(status!=CL_SUCCESS)
  {
    print_debug((char *)"Error: In clReleaseMemObject (COUNTERS_Buffer)\n");
    return false; 
  }

	status = clReleaseMemObject(GLOBAL_BOARD_STAK_TOP_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STAK_TOP_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_TOTAL_NODES_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TOTAL_NODES_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_FINISHED_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_FINISHED_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_MOVECOUNTER_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PID_MOVECOUNTER_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_TODOINDEX_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TODOINDEX_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_AB_SCORES_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_AB_SCORES_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_DEPTHS_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_DEPTHS_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_MOVES_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PID_MOVES_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_MOVE_HISTORY_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PID_MOVE_HISTORY_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_PID_CR_HISTORY_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PID_CR_HISTORY_Buffer)\n");
		return false; 
	}

	status = clReleaseMemObject(GLOBAL_HASHHISTORY_Buffer);
  if(status!=CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_HASHHISTORY_Buffer)\n");
		return false; 
	}

	return true;
}
// release OpenCL device, once by exit
bool cl_release_device() {

  if (program!=NULL)
  {
    status = clReleaseProgram(program);
    program = NULL;
    if(status!=CL_SUCCESS)
    {
      print_debug((char *)"Error: In clReleaseProgram\n");
      return false; 
    }
  }

  if (context!=NULL)
  {
    status = clReleaseContext(context);
    context = NULL;
    if(status!=CL_SUCCESS)
    {
      print_debug((char *)"Error: In clReleaseContext\n");
      return false;
    }
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
      fprintf(Stats, "CL_DEVICE_NOT_AVAILABLE");
  fclose(Stats);
}

