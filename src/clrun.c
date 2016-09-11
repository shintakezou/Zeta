/*
    Zeta CL, OpenCL Chess Engine
    Author: Srdja Matovic <srdja.matovic@googlemail.com>
    Created at: 20-Jan-2011
    Updated at:
    Description: A Chess Engine written in OpenCL, a language suited for GPUs.

    Copyright (C) 2011 Srdja Matovic
    This program is distributed under the GNU General Public License.
    See file COPYING or http://www.gnu.org/licenses/
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "zeta.h"

extern U64 *Zobrist;

cl_int status = 0;
cl_uint deviceListSize;

cl_uint maxDims = 3;
size_t globalThreads[3]; 
size_t localThreads[3];

int temp = 0;


int initializeCLDevice() {

    context = NULL;
    kernel  = NULL;
    program = NULL;

    status = clGetPlatformIDs(256, NULL, &numPlatforms);
    if(status != CL_SUCCESS)
    {
        print_debug((char *)"Error: Getting Platforms. (clGetPlatformsIDs)\n");
        return 1;
    }
    
    if(numPlatforms > 0)
    {

        cl_platform_id* platforms = (cl_platform_id *) malloc(numPlatforms);

        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if(status != CL_SUCCESS)
        {
            print_debug((char *)"Error: Getting Platform Ids. (clGetPlatformsIDs)\n");
            return 1;
        }

        platform = platforms[opencl_platform_id];
    }

    if(NULL == platform)
    {
        print_debug((char *)"NULL platform found so Exiting Application.");
        return 1;
    }

    /*
     * If we could find our platform, use it. Otherwise use just available platform.
     */


	/////////////////////////////////////////////////////////////////
	// Detect OpenCL devices
	/////////////////////////////////////////////////////////////////
    status = clGetDeviceIDs(platform, 
                              CL_DEVICE_TYPE_ALL, 
                              0, 
                              NULL, 
                              &deviceListSize);
    if(status != CL_SUCCESS) 
	{  
		print_debug((char *)"Error: Getting Device IDs (device list size, clGetDeviceIDs)\n");
		return 1;
	}

	if(deviceListSize == 0)
	{
		print_debug((char *)"Error: No devices found.\n");
		return 1;
	}

    devices = (cl_device_id *)malloc(deviceListSize * sizeof(cl_device_id));


    status = clGetDeviceIDs(platform, 
                              CL_DEVICE_TYPE_ALL, 
                              deviceListSize, 
                              devices, 
                              NULL);
    if(status != CL_SUCCESS) 
	{  
		print_debug((char *)"Error: Getting Device IDs (devices, clGetDeviceIDs)\n");
		return 1;
	}


    cps[0] = CL_CONTEXT_PLATFORM;
    cps[1] = (cl_context_properties)platform;
    cps[2] = 0;


	/////////////////////////////////////////////////////////////////
	// Create an OpenCL context
	/////////////////////////////////////////////////////////////////

    context = clCreateContext(cps, 
                                      1, 
                                      &devices[opencl_device_id], 
                                      NULL, 
                                      NULL, 
                                      &status);
    if(status != CL_SUCCESS) 
	{  
		print_debug((char *)"Error: Creating Context Info (cps, clCreateContext)\n");
		return 1;
	}

    return 0;
}

int initializeCL() {

	/////////////////////////////////////////////////////////////////
	// Create an OpenCL command queue
	/////////////////////////////////////////////////////////////////
    commandQueue = clCreateCommandQueue(
					   context, 
                       devices[opencl_device_id], 
                       0, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Creating Command Queue. (clCreateCommandQueue)\n");
		return 1;
	}

	/////////////////////////////////////////////////////////////////
	// Create OpenCL memory buffers
	/////////////////////////////////////////////////////////////////
    GLOBAL_INIT_BOARD_Buffer = clCreateBuffer(
				      context, 
                      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                      sizeof(cl_ulong) * 5,
                      GLOBAL_INIT_BOARD, 
                      &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_INIT_BOARD_Buffer)\n");
		return 1;
	}

    GLOBAL_BOARD_STACK_1_Buffer = clCreateBuffer(
		              context, 
                      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                      sizeof(NodeBlock) * max_nodes_to_expand,
                      NODES, 
                      &status);
    if(status != CL_SUCCESS) 
    { 
        print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STACK_1_Buffer)\n");
        return 1;
    }

    temp = (memory_slots >= 2)? max_nodes_to_expand : 1;
    GLOBAL_BOARD_STACK_2_Buffer = clCreateBuffer(
		              context, 
                      CL_MEM_READ_WRITE,
                      sizeof(NodeBlock) * temp,
                      NULL, 
                      &status);
    if(status != CL_SUCCESS) 
    { 
        print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STACK_2_Buffer)\n");
        return 1;
    }   

    temp = (memory_slots >= 3)? max_nodes_to_expand : 1;
    GLOBAL_BOARD_STACK_3_Buffer = clCreateBuffer(
		              context, 
                      CL_MEM_READ_WRITE,
                      sizeof(NodeBlock) * temp,
                      NULL, 
                      &status);
    if(status != CL_SUCCESS) 
    { 
        print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STACK_3_Buffer)\n");
        return 1;
    }   

    temp = 0;
    GLOBAL_RETURN_BESTMOVE_Buffer = clCreateBuffer(
				      context, 
                      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                      sizeof(S32) * 1,
                      &temp, 
                      &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (  GLOBAL_RETURN_BESTMOVE_Buffer)\n");
		return 1;
	}

    COUNTERS_Buffer = clCreateBuffer(
				      context, 
                      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                      sizeof(cl_ulong) * totalThreads * 10,
                      COUNTERS, 
                      &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (COUNTERS_Buffer)\n");
		return 1;
	}

    temp = BOARD_STACK_TOP;
    GLOBAL_BOARD_STAK_TOP_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       sizeof(cl_int) *  1,
                       &temp, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_BOARD_STAK_TOP_Buffer)\n");
		return 1;
	}

    temp = 0;
    GLOBAL_TOTAL_NODES_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       sizeof(cl_int) *  1,
                       &temp, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_TOTAL_NODES_Buffer)\n");
		return 1;
	}

    GLOBAL_PID_MOVECOUNTER_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE,
                       sizeof(cl_int) * totalThreads * max_depth,
                       NULL, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_MOVECOUNTER_Buffer)\n");
		return 1;
	}

    GLOBAL_PID_TODOINDEX_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE,
                       sizeof(cl_int) * totalThreads * max_depth,
                       NULL, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_TODOINDEX_Buffer)\n");
		return 1;
	}

    GLOBAL_PID_AB_SCORES_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE,
                       sizeof(cl_int) * totalThreads * max_depth * 2,
                       NULL, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_AB_SCORES_Buffer)\n");
		return 1;
	}


    GLOBAL_PID_DEPTHS_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE,
                       sizeof(cl_int) * totalThreads * max_depth * 2,
                       NULL, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_DEPTHS_Buffer)\n");
		return 1;
	}

    GLOBAL_PID_MOVES_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE,
                       sizeof(Move) * totalThreads * max_depth * MAXLEGALMOVES,
                       NULL, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL__PID_MOVES_Buffer)\n");
		return 1;
	}

    temp = 0;
    GLOBAL_FINISHED_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       sizeof(cl_int) * 1,
                       &temp, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_FINISHED_Buffer)\n");
		return 1;
	}

    temp = 0;
    GLOBAL_MOVECOUNT_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       sizeof(cl_int) * 1,
                       &temp, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_MOVECOUNT_Buffer)\n");
		return 1;
	}

    temp = 0;
    GLOBAL_PLYREACHED_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       sizeof(cl_int) * 1,
                       &temp, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_PLYREACHED_Buffer)\n");
		return 1;
	}

    GLOBAL_HASHHISTORY_Buffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                       sizeof(Hash) * totalThreads*1024,
                       GLOBAL_HASHHISTORY, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: clCreateBuffer (GLOBAL_HASHHISTORY_Buffer)\n");
		return 1;
	}

    ZobristBuffer = clCreateBuffer(
					   context, 
                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                       sizeof(cl_ulong) * 896,
                       &Zobrist, 
                       &status);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: ZobristBuffer (BAttacksBuffer)\n");
		return 1;
	}


	/////////////////////////////////////////////////////////////////
	// build CL program object, create CL kernel object
	/////////////////////////////////////////////////////////////////
    if (true) {
//    if (program == NULL ) {
        content = source;
        contentSize = &sourceSize;
        program = clCreateProgramWithSource(
			          context, 
                      1, 
                      &content,
				      contentSize,
                      &status);
	    if(status != CL_SUCCESS) 
	    { 
	      print_debug((char *)"Error: Loading Binary into cl_program (clCreateProgramWithBinary)\n");
	      return 1;
	    }   

        /* create a cl program executable for all the devices specified */
        status = clBuildProgram(program, 1, &devices[opencl_device_id], NULL, NULL, NULL);
        if(status != CL_SUCCESS) 
	    { 
            char* build_log=0;
            size_t log_size=0;
            FILE 	*temp=0;

		    print_debug((char *)"Error: Building Program (clBuildProgram)\n");


            // Shows the log
            // First call to know the proper size
            clGetProgramBuildInfo(program, devices[opencl_device_id], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            build_log = (char *) malloc(log_size+1);
            // Second call to get the log
            status = clGetProgramBuildInfo(program, devices[opencl_device_id], CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
            //build_log[log_size] = '\0';

            temp = fopen("zeta.debug", "ab+");
            fprintf(temp, "buildlog: %s \n", build_log);
            fclose(temp);

            if(status != CL_SUCCESS) 
            { 
              print_debug((char *)"Error: Building Log (clGetProgramBuildInfo)\n");
            }
         
           return 1;

	    }
    }

    /* get a kernel object handle for a kernel with the given name */
    kernel = clCreateKernel(program, "bestfirst_gpu", &status);
    if(status != CL_SUCCESS) 
    {  
	    print_debug((char *)"Error: Creating Kernel from program. (clCreateKernel)\n");
	    return 1;
    }


	return 0;
}




/*
 *        brief Run OpenCL program 
 *		  
 *        Bind host variables to kernel arguments 
 *		  Run the CL kernel
 */
int  runCLKernels(int som, int depth, Move lastmove) {

    int i = 0;

    /*** Set appropriate arguments to the kernel ***/
    /* the output array to the kernel */

    /* the input to the kernel */
    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_INIT_BOARD_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_INIT_BOARD_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_BOARD_STACK_1_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_1_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_BOARD_STACK_2_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_2_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_BOARD_STACK_3_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_BOARD_STACK_3_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_RETURN_BESTMOVE_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_RETURN_BESTMOVE_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&COUNTERS_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (COUNTERS_Buffer)\n");
		return 1;
	}
    i++;


    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_BOARD_STAK_TOP_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. ( GLOBAL_BOARD_STAK_TOP_Buffer)\n");
		return 1;
	}
    i++;


    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_TOTAL_NODES_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. ( GLOBAL_TOTAL_NODES_Buffer)\n");
		return 1;
	}
    i++;


    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_PID_MOVECOUNTER_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVECOUNTER_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_PID_TODOINDEX_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_TODOINDEX_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_PID_AB_SCORES_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_AB_SCORES_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_PID_DEPTHS_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_DEPTHS_Buffer)\n");
		return 1;
	}
    i++;


    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_PID_MOVES_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PID_MOVES_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_FINISHED_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_FINISHED_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_MOVECOUNT_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_MOVECOUNT_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_PLYREACHED_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_PLYREACHED_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&GLOBAL_HASHHISTORY_Buffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (GLOBAL_HASHHISTORY_Buffer)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_int), 
                    (void *)&som);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (som)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_int), 
                    (void *)&PLY);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (PLY)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_int), 
                    (void *)&depth);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (max_leaf_depth)\n");
		return 1;
	}
    i++;

    temp = max_nodes_to_expand*memory_slots;
    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_int), 
                    (void *)&temp);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (max_nodes_to_expand)\n");
		return 1;
	}
    i++;


    temp = max_nodes_to_expand;
    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_int), 
                    (void *)&temp);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. ( max_nodes_per_slot)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_long), 
                    (void *)&max_nodes);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (max_nodes)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_int), 
                    (void *)&max_depth);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (max_depth)\n");
		return 1;
	}
    i++;

    status = clSetKernelArg(
                    kernel, 
                    i, 
                    sizeof(cl_mem), 
                    (void *)&ZobristBuffer);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Setting kernel argument. (ZobristBuffer)\n");
		return 1;
	}
    i++;
    /* 
     * Enqueue a kernel run call.
     */
    globalThreads[0] = threadsX;
    globalThreads[1] = threadsY;
    globalThreads[2] = threadsZ;

    localThreads[0]  = 1;
    localThreads[1]  = threadsY;
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
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Enqueueing kernel onto command queue. (clEnqueueNDRangeKernel)\n");
		return 1;
	}


    status = clFlush(commandQueue);

    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: flushing the Kernel. (clFlush)\n");
		return 1;
	}



    status = clFinish(commandQueue);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Waiting for kernel run to finish. (clFinish)\n");
		return 1;
	}
  return 0;
}
int  clGetMemory()
{
    cl_event events[2];

    /* wait for the kernel call to finish execution */
/*
    status = clWaitForEvents(1, &events[0]);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Waiting for kernel run to finish. (clWaitForEvents)\n");
		return 1;
	}
    status = clReleaseEvent(events[0]);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Release event object. (clReleaseEvent)\n");
		return 1;
	}
*/

    /* Enqueue readBuffer*/
    status = clEnqueueReadBuffer(
                commandQueue,
                GLOBAL_RETURN_BESTMOVE_Buffer,
                CL_TRUE,
                0,
                1 * sizeof(S32),
                GLOBAL_RETURN_BESTMOVE,
                0,
                NULL,
                &events[1]);
    
    if(status != CL_SUCCESS) 
	{ 
        print_debug((char *)"Error: clEnqueueReadBuffer failed. (GLOBAL_RETURN_BESTMOVE)\n");

		return 1;
    }
    /* Wait for the read buffer to finish execution */
    status = clWaitForEvents(1, &events[1]);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Waiting for read buffer call to finish. (GLOBAL_RETURN_BESTMOVE)\n");
		return 1;
	}
    status = clReleaseEvent(events[1]);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Release event object.(GLOBAL_RETURN_BESTMOVE)\n");
		return 1;
	}



    // Enqueue readBuffer
    status = clEnqueueReadBuffer(
                commandQueue,
                COUNTERS_Buffer,
                CL_TRUE,
                0,
                10*totalThreads * sizeof(U64),
                COUNTERS,
                0,
                NULL,
                &events[1]);
    
    if(status != CL_SUCCESS) 
	{ 
        print_debug((char *)"Error: clEnqueueReadBuffer failed. (CountersBuffer)\n");

		return 1;
    }
    // Wait for the read buffer to finish execution
    status = clWaitForEvents(1, &events[1]);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Waiting for read buffer call to finish. (CountersBuffer)\n");
		return 1;
	}
    status = clReleaseEvent(events[1]);
    if(status != CL_SUCCESS) 
	{ 
		print_debug((char *)"Error: Release event object.(CountersBuffer)\n");
		return 1;
	}



//    if (reuse_node_tree == 1) {
        // Enqueue readBuffer
        status = clEnqueueReadBuffer(
                    commandQueue,
                    GLOBAL_BOARD_STACK_1_Buffer,
                    CL_TRUE,
                    0,
                    sizeof(NodeBlock) * max_nodes_to_expand,
                    NODES,
                    0,
                    NULL,
                    &events[1]);
        
        if(status != CL_SUCCESS) 
	    { 
            print_debug((char *)"Error: clEnqueueReadBuffer failed. (GLOBAL_BOARD_STACK_1_Buffer)\n");

		    return 1;
        }
        // Wait for the read buffer to finish execution
        status = clWaitForEvents(1, &events[1]);
        if(status != CL_SUCCESS) 
	    { 
		    print_debug((char *)"Error: Waiting for read buffer call to finish. (GLOBAL_BOARD_STACK_1_Buffer)\n");
		    return 1;
	    }
        status = clReleaseEvent(events[1]);
        if(status != CL_SUCCESS) 
	    { 
		    print_debug((char *)"Error: Release event object.(GLOBAL_BOARD_STACK_1_Buffer)\n");
		    return 1;
	    }
//    }

    /* release resources */

    status = clReleaseKernel(kernel);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseKernel \n");
		return 1; 
	}
/*
    status = clReleaseProgram(program);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseProgram\n");
		return 1; 
	}
*/

    status = clReleaseCommandQueue(commandQueue);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseCommandQueue\n");
		return 1;
	}

    status = clReleaseMemObject(GLOBAL_INIT_BOARD_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_INIT_BOARD_Buffer)\n");
		return 1; 
	}

    status = clReleaseMemObject(GLOBAL_BOARD_STACK_1_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STACK_1_Buffer)\n");
		return 1; 
	}

    status = clReleaseMemObject(GLOBAL_BOARD_STACK_2_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STACK_2_Buffer)\n");
		return 1; 
	}

    status = clReleaseMemObject(GLOBAL_BOARD_STACK_3_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STACK_3_Buffer)\n");
		return 1; 
	}

    status = clReleaseMemObject(GLOBAL_RETURN_BESTMOVE_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_RETURN_BESTMOVE_Buffer)\n");
		return 1; 
	}

    status = clReleaseMemObject(COUNTERS_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (COUNTERS_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_BOARD_STAK_TOP_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_BOARD_STAK_TOP_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_TOTAL_NODES_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TOTAL_NODES_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_PID_MOVECOUNTER_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_MOVECOUNTER_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_PID_TODOINDEX_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_TODOINDEX_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_PID_AB_SCORES_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_AB_SCORES_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_PID_DEPTHS_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_DEPTHS_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_PID_MOVES_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PID_MOVES_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_FINISHED_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_FINISHED_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_MOVECOUNT_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_MOVECOUNT_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_PLYREACHED_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_PLYREACHED_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(GLOBAL_HASHHISTORY_Buffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (GLOBAL_HASHHISTORY_Buffer)\n");
		return 1; 
	}

	status = clReleaseMemObject(ZobristBuffer);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseMemObject (ZobristBuffer)\n");
		return 1; 
	}

	return 0;
}


int  releaseCLDevice() {

/*
    status = clReleaseKernel(kernel);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseKernel \n");
		return 1; 
	}
*/
    status = clReleaseProgram(program);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseProgram\n");
		return 1; 
	}

    status = clReleaseContext(context);
    if(status != CL_SUCCESS)
	{
		print_debug((char *)"Error: In clReleaseContext\n");
		return 1;
	}

	return 0;
}



int load_file_to_string(const char *filename, char **result) 
{ 
	unsigned int size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL) 
	{ 
		*result = NULL;
		return -1;
	} 
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (char *)malloc(size+1);
	if (size != fread(*result, sizeof(char), size, f)) 
	{ 
		free(*result);
		return -2;
	} 
	fclose(f);
	(*result)[size] = 0;
	return size;
}

void print_debug(char *debug) {
    FILE 	*Stats;
    Stats = fopen("zeta.debug", "ab+");
    fprintf(Stats, "%s, status:%i", debug, status);
    if (status == CL_DEVICE_NOT_AVAILABLE)
        fprintf(Stats, "CL_DEVICE_NOT_AVAILABLE");
    fclose(Stats);
}


