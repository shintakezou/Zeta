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

#include <stdio.h>      // for file io
#include <string.h>     // for string comparing functions

#include "timer.h"
#include "zeta.h"       // for global vars

extern s32 benchmarkWrapper(s32 benchsec);
extern void read_config();
extern s32 load_file_to_string(const char *filename, char **result);
extern const char filename[];

// guess minimal and optimal setup for given cl device
bool cl_guess_config(bool extreme)
{
  cl_int status = 0;
  size_t paramSize;
  size_t paramValue;
  char *deviceName;
  char *ExtensionsValue;
  cl_device_id *devices;
  u32 i,j,k;
  u32 deviceunits = 0;
  u64 devicememalloc = 0;
  s32 warpsize = 1;
  s32 warpmulti = 1;
  s32 nps = 0;
  s32 npstmp = 0;
  s32 devicecounter = 0;
  s32 benchsec = 4;
    
  fprintf(stdout,"#> ### Query the OpenCL Platforms on Host...\n");
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#> ### Query the OpenCL Platforms on Host...\n");
  }

  status = clGetPlatformIDs(256, NULL, &numPlatforms);
  if(status!=CL_SUCCESS)
  {
    fprintf(stdout,": No OpenCL Platforms detected\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile,": No OpenCL Platforms detected\n");
    }
    return false;
  }
   
  if(numPlatforms > 0)
  {
    fprintf(stdout, "#> Number of OpenCL Platforms found: %i \n", numPlatforms);
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "#> Number of OpenCL Platforms found: %i \n", numPlatforms);
    }

    cl_platform_id* platforms = (cl_platform_id *) malloc(numPlatforms);

    status = clGetPlatformIDs(numPlatforms, platforms, NULL);
    if(status!=CL_SUCCESS)
    {
      fprintf(stdout, "#> Error: Getting Platform Ids. (clGetPlatformsIDs)\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#> Error: Getting Platform Ids. (clGetPlatformsIDs)\n");
      }
      return false;
    }
    // for each present OpenCL Platform do
    for(i=0; i < numPlatforms; ++i)
    {
      char pbuff[256];
      // get platforms
      status = clGetPlatformInfo(
                                platforms[i],
                                CL_PLATFORM_VENDOR,
                                sizeof(pbuff),
                                pbuff,
                                NULL);
      if(status!=CL_SUCCESS)
      {
        fprintf(stdout, "#> Error: Getting Platform Info.(clGetPlatformInfo)\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> Error: Getting Platform Info.(clGetPlatformInfo)\n");
        }
        return false;
      }
      // get current platform
      platform = platforms[i];
      fprintf(stdout, "#> Platform: %i,  Vendor:  %s \n", i, pbuff);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#> Platform: %i,  Vendor:  %s \n", i, pbuff);
      }
      fprintf(stdout, "#> ### Query the OpenCL Devices on Platform...\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#> ### Query the OpenCL Devices on Platform...\n");
      }
      // get device list size
      status = clGetDeviceIDs(platform, 
                              CL_DEVICE_TYPE_ALL, 
                              0, 
                              NULL, 
                              &deviceListSize);
      if(status!=CL_SUCCESS) 
      {  
        fprintf(stdout, "#> Error: Getting DeviceListSize (clGetDeviceIDs)\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> Error: Getting DeviceListSize (clGetDeviceIDs)\n");
        }
        continue;
      }
      if(deviceListSize == 0)
      {
        fprintf(stdout, "#> Error: No devices found.\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> Error: No devices found.\n");
        }
        continue;
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
        fprintf(stdout, "#> Error: Getting DeviceIDs (clGetDeviceIDs)\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> Error: Getting DeviceIDs (clGetDeviceIDs)\n");
        }
        continue;
      }
      fprintf(stdout, "#> Number of OpenCL Devices found: %i \n", deviceListSize);
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#> Number of OpenCL Devices found: %i \n", deviceListSize);
      }

      // for each present OpenCL device do
      for(j=0; j < deviceListSize; j++)
      {
        warpmulti = 1;
        // get device name size
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_NAME,
                                  0,
                                  NULL,
                                  &paramSize
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting Device Name Size (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting Device Name Size (clGetDeviceInfo)\n");
          }
          continue;
        }
        deviceName = (char *)malloc(1 * paramSize);
        // get device name
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_NAME,
                                  paramSize,
                                  deviceName,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting Device Name (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting Device Name (clGetDeviceInfo)\n");
          }
          continue;
        }
        fprintf(stdout, "#> ### Query and check the OpenCL Device...\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### Query and check the OpenCL Device...\n");
        }
        fprintf(stdout, "#> Device: %i, Device name: %s \n", j, deviceName);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> Device: %i, Device name: %s \n", j, deviceName);
        }
        cl_bool endianlittle = CL_FALSE;
        // get endianess, only little supported
        status = clGetDeviceInfo (devices[j],
                        CL_DEVICE_ENDIAN_LITTLE,
                        sizeof(cl_bool),
                        &endianlittle,
                        NULL
                        );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting Device Endianess (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting Device Endianess (clGetDeviceInfo)\n");
          }
          continue;
        }
        if (endianlittle == CL_TRUE)
        {
          fprintf(stdout, "#> OK, Device Endianness is little\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Device Endianness is little\n");
          }
        }
        else
        {
          fprintf(stdout, "#> Error: Device Endianness is not little\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device Endianness is not little\n");
          }
          continue;
        }
        // get compute units
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_MAX_COMPUTE_UNITS,
                                  sizeof(cl_uint),
                                  &deviceunits,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_MAX_COMPUTE_UNITS (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_MAX_COMPUTE_UNITS (clGetDeviceInfo)\n");
          }
          continue;
        } 
        fprintf(stdout, "#> OK, CL_DEVICE_MAX_COMPUTE_UNITS: %i \n",deviceunits);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> OK, CL_DEVICE_MAX_COMPUTE_UNITS: %i \n",deviceunits);
        }
        // get max memory allocation size
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_MAX_MEM_ALLOC_SIZE ,
                                  sizeof(cl_ulong),
                                  &devicememalloc,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_MAX_MEM_ALLOC_SIZE (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_MAX_MEM_ALLOC_SIZE (clGetDeviceInfo)\n");
          }
          continue;
        } 
        // check for min 64 mb memory
        if (devicememalloc < MINDEVICEMB*1024*1024 ) {
          fprintf(stdout, "#> Error, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " < %" PRIu64 " MB\n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " < %" PRIu64 " MB\n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
          }
          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " MB > %" PRIu64 " MB \n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " MB > %" PRIu64 " MB \n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
          }
        }
        // get max memory allocation size
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_GLOBAL_MEM_SIZE,
                                  sizeof(cl_ulong),
                                  &devicememalloc,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_GLOBAL_MEM_SIZE (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_GLOBAL_MEM_SIZE (clGetDeviceInfo)\n");
          }
          continue;
        } 
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_GLOBAL_MEM_SIZE: %" PRIu64 " MB\n", devicememalloc/1024/1024);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_GLOBAL_MEM_SIZE: %" PRIu64 " MB\n", devicememalloc/1024/1024);
          }
        }
        // use 1/4 of global memory
        devicememalloc/=4;
        // set memory to default max
        if (devicememalloc > MAXDEVICEMB*1024*1024 )
          devicememalloc = MAXDEVICEMB*1024*1024; 
        // check for needed device extensions
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_EXTENSIONS,
                                  0,
                                  NULL,
                                  &paramSize
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_EXTENSIONS size (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_EXTENSIONS size (clGetDeviceInfo)\n");
          }
          continue;
        } 
        ExtensionsValue = (char *)malloc(1 * paramSize);
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_EXTENSIONS,
                                  paramSize,
                                  ExtensionsValue,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_EXTENSIONS value (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_EXTENSIONS value (clGetDeviceInfo)\n");
          }
          continue;
        } 
        if ((!strstr(ExtensionsValue, "cl_khr_global_int32_base_atomics")))
        {
          fprintf(stdout, "#> Error: Device extension cl_khr_global_int32_base_atomics not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device extension cl_khr_global_int32_base_atomics not supported.\n");
          }
          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, Device extension cl_khr_global_int32_base_atomics is supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Device extension cl_khr_global_int32_base_atomics is supported.\n");
          }
        }
/*
        if ((!strstr(ExtensionsValue, "cl_khr_local_int32_base_atomics")))
        {
          fprintf(stdout, "#> Error: Device extension cl_khr_local_int32_base_atomics not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device extension cl_khr_local_int32_base_atomics not supported.\n");
          }
          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, Device extension cl_khr_local_int32_base_atomics is supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Device extension cl_khr_local_int32_base_atomics is supported.\n");
          }
        }
*/
        if ((!strstr(ExtensionsValue, "cl_khr_global_int32_extended_atomics")))
        {
          fprintf(stdout, "#> Error: Device extension cl_khr_global_int32_extended_atomics not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device extension cl_khr_global_int32_extended_atomics not supported.\n");
          }
        }
        else
        {
          fprintf(stdout, "#> OK, Device extension cl_khr_global_int32_extended_atomics is supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Device extension cl_khr_global_int32_extended_atomics is supported.\n");
          }
        }
        // getting prefered warpsize resp. wavefront size, 
        fprintf(stdout, "#> #### Query Kernel params.\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> #### Query Kernel params.\n");
        }
        // create context properties
        cps[0] = CL_CONTEXT_PLATFORM;
        cps[1] = (cl_context_properties)platform;
        cps[2] = 0;
        // create context
        context = clCreateContext(cps, 
                                  1, 
                                  &devices[j],
                                  NULL, 
                                  NULL, 
                                  &status);
        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Creating Context Info (cps, clCreateContext)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Creating Context Info (cps, clCreateContext)\n");
          }
          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, Creating Context Info\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Creating Context Info\n");
          }
        }
        // create command queue
        commandQueue = clCreateCommandQueue(
                  	                       context, 
                                           devices[j], 
                                           0, 
                                           &status);
        if(status!=CL_SUCCESS) 
        { 
          fprintf(stdout, "#> Error: Creating Command Queue. (clCreateCommandQueue)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Creating Command Queue. (clCreateCommandQueue)\n");
          }
          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, Creating Command Queue\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Creating Command Queue\n");
          }
        }
        // create program
        const char *content = zeta_cl;
        program = clCreateProgramWithSource(
                                	          context, 
                                            1, 
                                            &content,
                                  		      &zeta_cl_len,
                                            &status);
        if(status!=CL_SUCCESS) 
        { 
          fprintf(stdout, "#> Error: Loading Binary into cl_program (clCreateProgramWithBinary)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Loading Binary into cl_program (clCreateProgramWithBinary)\n");
          }
          continue;
        }   
        else
        {
          fprintf(stdout, "#> OK, Loading Binary into cl_program\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Loading Binary into cl_program\n");
          }
        }
        // build program for device
        status = clBuildProgram(program, 1, &devices[j], NULL, NULL, NULL);
        if(status!=CL_SUCCESS) 
        { 
          char* build_log=0;
          size_t log_size=0;
          FILE 	*temp=0;

          fprintf(stdout, "#> Error: Building Program, see file zeta.log for build log (clBuildProgram)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Building Program, see file zeta.log for build log (clBuildProgram)\n");
          }

          // shows the log
          // first call to know the proper size
          clGetProgramBuildInfo(program, devices[j], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
          build_log = (char *) malloc(log_size+1);

          if(status!=CL_SUCCESS) 
          { 
            fprintf(stdout, "#> Error: Building Log size (clGetProgramBuildInfo)\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#> Error: Building Log size (clGetProgramBuildInfo)\n");
            }
          }
          // second call to get the log
          status = clGetProgramBuildInfo(program, devices[j], CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
          //build_log[log_size] = '\0';

          if(status!=CL_SUCCESS) 
          { 
            fprintf(stdout, "#> Error: Building Log (clGetProgramBuildInfo)\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#> Error: Building Log (clGetProgramBuildInfo)\n");
            }
          }

          temp = fopen("zeta.log", "a");
          fprintdate(temp);
          fprintf(temp, "buildlog: %s \n", build_log);
          fclose(temp);

          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, Building Program\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Building Program\n");
          }
        }
        // get a kernel object handle for a kernel with the given name
        kernel = clCreateKernel(program, "bestfirst_gpu", &status);
        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Creating Kernel from program. (clCreateKernel)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Creating Kernel from program. (clCreateKernel)\n");
          }
          continue;
        }
        else
        {
          fprintf(stdout, "#> OK, Creating Kernel from program\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Creating Kernel from program\n");
          }
        }
        // query kernel for warp resp wavefront size
        status = clGetKernelWorkGroupInfo ( kernel,
                                            devices[j],
                                            CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                            0,
                                            NULL,
                                            &paramSize
                                          );
        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE size (clGetKernelWorkGroupInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE size (clGetKernelWorkGroupInfo)\n");
          }
          continue;
        }

        status = clGetKernelWorkGroupInfo ( kernel,
                                            devices[j],
                                            CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                            paramSize,
                                            &paramValue,
                                            NULL
                                          );
        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE (clGetKernelWorkGroupInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE (clGetKernelWorkGroupInfo)\n");
          }
          continue;
        }
        warpsize = (s32)paramValue;
        fprintf(stdout, "#> OK, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: %i.\n", warpsize);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#> OK, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: %i.\n", warpsize);
        }

        // print temp config file
        FILE 	*Cfg;
        remove("config.tmp");

        Cfg = fopen("config.tmp", "w");
        fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", deviceName);
        fprintf(Cfg, "threadsX: %i;\n", deviceunits);
        fprintf(Cfg, "threadsY: %i;\n", warpmulti);
        fprintf(Cfg, "threadsZ: %i;\n\n", warpsize);
        fprintf(Cfg, "nodes_per_second: %i;\n", nps);
        fprintf(Cfg, "max_nodes: 0;\n");
        fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)devicememalloc/1024/1024);
        fprintf(Cfg, "memory_slots: %i; // max 3 \n", memory_slots);
        fprintf(Cfg, "max_ab_depth: 1; // min 1\n");
        fprintf(Cfg, "max_depth: 32;\n");
        fprintf(Cfg, "opencl_platform_id: %i;\n",i);
        fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
        fprintf(Cfg,"\n");
        fprintf(Cfg,"config options explained:\n");
        fprintf(Cfg,"threadsX => number of SIMD units or CPU cores\n");
        fprintf(Cfg,"threadsY => multiplier for threadsZ\n");
        fprintf(Cfg,"threadsZ => mumber of threads per SIMD Unit or core\n");
        fprintf(Cfg,"nodes_per_second => nps of device, for time control\n");
        fprintf(Cfg,"max_nodes => search n nodes, 0 is inf\n");
        fprintf(Cfg,"max_memory => allocate n MB of memory on device for the node tree\n");
        fprintf(Cfg,"memory_slots => allocate n times max_memory on device, max is 3\n");
        fprintf(Cfg,"max_ab_depth => in evaluation phase perform an depth n alphabeta search\n");
        fprintf(Cfg,"max_depth => max alphabeta search depth\n");
        fprintf(Cfg,"opencl_platform_id => which OpenCL platform to use\n");
        fprintf(Cfg,"opencl_device_id => which OpenCL device to use\n\n");
        fclose(Cfg);

        fprintf(stdout, "#\n");
        fprintf(stdout, "#> ### Running NPS-Benchmark for minimal config on device, this can last %i seconds... \n", benchsec);
        fprintf(stdout, "#\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#\n");
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### Running NPS-Benchmark for minimal config on device, this can last %i seconds... \n", benchsec);
          fprintdate(LogFile);
          fprintf(LogFile, "#\n");
        }
        npstmp = benchmarkWrapper(benchsec);
        remove("config.tmp");
        
        // something went wrong
        if (npstmp<=0)
        {
          fprintf(stdout, "#\n");
          fprintf(stdout, "#> ### Benchmark FAILED, see zeta.log file for more info... \n");
          fprintf(stdout, "#\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#\n");
            fprintdate(LogFile);
            fprintf(LogFile, "#> ### Benchmark FAILED, see zeta.log file for more info... \n");
            fprintdate(LogFile);
            fprintf(LogFile, "#\n");
          }
          continue;
        }

        nps = npstmp;

        // iterate through threadsY, get best multi for warp resp. wavefront size
        if (extreme)
        {

          fprintf(stdout, "#\n");
          fprintf(stdout, "#> ### Running NPS-Benchmark for best config, this can last some minutes... \n");
          fprintf(stdout, "#\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#\n");
            fprintdate(LogFile);
            fprintf(LogFile, "#> ### Running NPS-Benchmark for best config, this can last some minutes... \n");
            fprintdate(LogFile);
            fprintf(LogFile, "#\n");
          }
/*
          // get threadsZ, warpsize, deprecated...
          while (true)
          {
            Cfg = fopen("config.tmp", "w");
            fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", deviceName);
            fprintf(Cfg, "threadsX: %i;\n", deviceunits);
            fprintf(Cfg, "threadsY: %i;\n", warpmulti);
            fprintf(Cfg, "threadsZ: %i;\n\n", warpsize*2);
            fprintf(Cfg, "nodes_per_second: %i;\n", npstmp);
            fprintf(Cfg, "max_nodes: 0;\n");
            fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)devicememalloc/1024/1024);
            fprintf(Cfg, "memory_slots: %i; // max 3 \n", memory_slots);
            fprintf(Cfg, "max_ab_depth: 1; // min 1\n");
            fprintf(Cfg, "max_depth: 32;\n");
            fprintf(Cfg, "opencl_platform_id: %i;\n",i);
            fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
            fprintf(Cfg,"\n");
            fprintf(Cfg,"config options explained:\n");
            fprintf(Cfg,"threadsX => number of SIMD units or CPU cores\n");
            fprintf(Cfg,"threadsY => multiplier for threadsZ\n");
            fprintf(Cfg,"threadsZ => mumber of threads per SIMD Unit or core\n");
            fprintf(Cfg,"nodes_per_second => nps of device, for time control\n");
            fprintf(Cfg,"max_nodes => search n nodes, 0 is inf\n");
            fprintf(Cfg,"max_memory => allocate n MB of memory on device for the node tree\n");
            fprintf(Cfg,"memory_slots => allocate n times max_memory on device, max is 3\n");
            fprintf(Cfg,"max_ab_depth => in evaluation phase perform an depth n alphabeta search\n");
            fprintf(Cfg,"max_depth => max alphabeta search depth\n");
            fprintf(Cfg,"opencl_platform_id => which OpenCL platform to use\n");
            fprintf(Cfg,"opencl_device_id => which OpenCL device to use\n\n");
            fclose(Cfg);

            fprintf(stdout, "#\n");
            fprintf(stdout, "#> ### Running NPS-Benchmark for threadsY on device, this can last %i seconds... \n", benchsec);
            fprintf(stdout, "#\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### Running NPS-Benchmark for threadsY on device, this can last %i seconds... \n", benchsec);
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
            }

            npstmp = benchmarkWrapper(benchsec);
            remove("config.tmp");

            // something went wrong
            if (npstmp<=0)
              break;
            // 10% speedup margin
            if (npstmp<=nps*1.10)
              break;
            // increase threadsZ
            if (npstmp>=nps)
              warpsize*=2;
              nps = npstmp;
            }
          }
*/
          // get threadsY, multi for warpsize
          while (true)
          {
            Cfg = fopen("config.tmp", "w");
            fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", deviceName);
            fprintf(Cfg, "threadsX: %i;\n", deviceunits);
            fprintf(Cfg, "threadsY: %i;\n", warpmulti*2);
            fprintf(Cfg, "threadsZ: %i;\n\n", warpsize);
            fprintf(Cfg, "nodes_per_second: %i;\n", npstmp);
            fprintf(Cfg, "max_nodes: 0;\n");
            fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)devicememalloc/1024/1024);
            fprintf(Cfg, "memory_slots: %i; // max 3 \n", memory_slots);
            fprintf(Cfg, "max_ab_depth: 1; // min 1\n");
            fprintf(Cfg, "max_depth: 32;\n");
            fprintf(Cfg, "opencl_platform_id: %i;\n",i);
            fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
            fprintf(Cfg,"\n");
            fprintf(Cfg,"config options explained:\n");
            fprintf(Cfg,"threadsX => number of SIMD units or CPU cores\n");
            fprintf(Cfg,"threadsY => multiplier for threadsZ\n");
            fprintf(Cfg,"threadsZ => mumber of threads per SIMD Unit or core\n");
            fprintf(Cfg,"nodes_per_second => nps of device, for time control\n");
            fprintf(Cfg,"max_nodes => search n nodes, 0 is inf\n");
            fprintf(Cfg,"max_memory => allocate n MB of memory on device for the node tree\n");
            fprintf(Cfg,"memory_slots => allocate n times max_memory on device, max is 3\n");
            fprintf(Cfg,"max_ab_depth => in evaluation phase perform an depth n alphabeta search\n");
            fprintf(Cfg,"max_depth => max alphabeta search depth\n");
            fprintf(Cfg,"opencl_platform_id => which OpenCL platform to use\n");
            fprintf(Cfg,"opencl_device_id => which OpenCL device to use\n\n");
            fclose(Cfg);

            fprintf(stdout, "#\n");
            fprintf(stdout, "#> ### Running NPS-Benchmark for threadsY on device, this can last %i seconds... \n", benchsec);
            fprintf(stdout, "#\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### Running NPS-Benchmark for threadsY on device, this can last %i seconds... \n", benchsec);
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
            }
            npstmp = benchmarkWrapper(benchsec);
            remove("config.tmp");

            // something went wrong
            if (npstmp<=0)
              break;
            // 10% speedup margin
            if (npstmp<=nps*1.10)
              break;
            // increase threadsY
            if (npstmp>=nps)
            {
              warpmulti*=2;
              nps = npstmp;
            }
          }
          remove("config.tmp");
        }

        devicecounter++;

        // print config to file
        char confignamefile[256] = "config";
        k = strlen(confignamefile);
        sprintf(confignamefile + k , "_%d_%d_", i,j );
        k = strlen(confignamefile);
        sprintf(confignamefile + k , ".ini");
        remove(confignamefile);

        Cfg = fopen(confignamefile, "w");
        fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", deviceName);
        fprintf(Cfg, "threadsX: %i;\n", deviceunits);
        fprintf(Cfg, "threadsY: %i;\n", warpmulti);
        fprintf(Cfg, "threadsZ: %i;\n\n", warpsize);
        fprintf(Cfg, "nodes_per_second: %i;\n", nps);
        fprintf(Cfg, "max_nodes: 0;\n");
        fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)devicememalloc/1024/1024);
        fprintf(Cfg, "memory_slots: %i; // max 3 \n", memory_slots);
        fprintf(Cfg, "max_ab_depth: 1; // min 1\n");
        fprintf(Cfg, "max_depth: 32;\n");
        fprintf(Cfg, "opencl_platform_id: %i;\n",i);
        fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
        fprintf(Cfg,"\n");
        fprintf(Cfg,"config options explained:\n");
        fprintf(Cfg,"threadsX => number of SIMD units or CPU cores\n");
        fprintf(Cfg,"threadsY => multiplier for threadsZ\n");
        fprintf(Cfg,"threadsZ => mumber of threads per SIMD Unit or core\n");
        fprintf(Cfg,"nodes_per_second => nps of device, for time control\n");
        fprintf(Cfg,"max_nodes => search n nodes, 0 is inf\n");
        fprintf(Cfg,"max_memory => allocate n MB of memory on device for the node tree\n");
        fprintf(Cfg,"memory_slots => allocate n times max_memory on device, max is 3\n");
        fprintf(Cfg,"max_ab_depth => in evaluation phase perform an depth n alphabeta search\n");
        fprintf(Cfg,"max_depth => max alphabeta search depth\n");
        fprintf(Cfg,"opencl_platform_id => which OpenCL platform to use\n");
        fprintf(Cfg,"opencl_device_id => which OpenCL device to use\n\n");
        fclose(Cfg);

        fprintf(stdout, "#\n");
        fprintf(stdout, "#\n");
        fprintf(stdout, "// Zeta OpenCL Chess config file for %s \n\n", deviceName);
        fprintf(stdout, "threadsX: %i;\n", deviceunits);
        fprintf(stdout, "threadsY: %i;\n", warpmulti);
        fprintf(stdout, "threadsZ: %i;\n", warpsize);
        fprintf(stdout, "nodes_per_second: %i;\n", nps);
        fprintf(stdout, "max_nodes: 0;\n");
        fprintf(stdout, "max_memory: %i; // in MB\n", (s32)devicememalloc/1024/1024);
        fprintf(stdout, "memory_slots: %i; // max 3\n", memory_slots);
        fprintf(stdout, "max_ab_depth: 1; // min 1\n");
        fprintf(stdout, "max_depth: 32;\n");
        fprintf(stdout, "opencl_platform_id: %i;\n",i);
        fprintf(stdout, "opencl_device_id: %i;\n\n",j);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#\n");
          fprintf(LogFile, "#\n");
          fprintf(LogFile, "// Zeta OpenCL Chess config file for %s \n\n", deviceName);
          fprintf(LogFile, "threadsX: %i;\n", deviceunits);
          fprintf(LogFile, "threadsY: %i;\n", warpmulti);
          fprintf(LogFile, "threadsZ: %i;\n", warpsize);
          fprintf(LogFile, "nodes_per_second: %i;\n", nps);
          fprintf(LogFile, "max_nodes: 0;\n");
          fprintf(LogFile, "max_memory: %i; // in MB\n", (s32)devicememalloc/1024/1024);
          fprintf(LogFile, "memory_slots: %i; // max 3\n", memory_slots);
          fprintf(LogFile, "max_ab_depth: 1; // min 1\n");
          fprintf(LogFile, "max_depth: 32;\n");
          fprintf(LogFile, "opencl_platform_id: %i;\n",i);
          fprintf(LogFile, "opencl_device_id: %i;\n\n",j);
        }

        fprintf(stdout, "##### Above output was saved in file %s \n", confignamefile);
        fprintf(stdout, "#\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "##### Above output was saved in file %s \n", confignamefile);
          fprintdate(LogFile);
          fprintf(LogFile, "#\n");
        }
      }
    }
  }
  if(platform==NULL)
  {
    fprintf(stdout, "#> Error: No OpenCL Platforms detected\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "#> Error: No OpenCL Platforms detected\n");
    }
    return false;
  }
  return true;
}

