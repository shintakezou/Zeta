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
#include <string.h>     // for string comparing functions
#include <math.h>       // for ceil

#include "timer.h"
#include "zeta.h"       // for global vars

extern u64 benchmarkWrapper(s32 benchsec);
extern void read_config();

// TODO: check for work group dim == 3 and workgroup size[3] >= 64

// guess minimal and optimal setup for given cl device
bool cl_guess_config(bool extreme)
{
  bool failed = false;
  cl_int status = 0;
  size_t paramSize;
  char *deviceName;
  char *ExtensionsValue;
  size_t wgsize;
  cl_device_id *devices;
  u32 i,j,k;
  u32 deviceunits = 0;
  u64 devicememalloc = 0;
  u64 memalloc = 0;
  s32 warpmulti = 1;
  s32 bestwarpmulti = 1;
  s64 nps = 0;
  s64 npstmp = 0;
  s32 devicecounter = 0;
  s32 benchsec = 4;
  u64 ttbits = 0;
  u64 mem = 0;
  u64 slots = 0;
    
  fprintf(stdout,"#>\n");
  fprintf(stdout,"#> ### Query the OpenCL Platforms on Host...\n");
  fprintf(stdout,"#>\n");
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#>\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#> ### Query the OpenCL Platforms on Host...\n");
    fprintdate(LogFile);
    fprintf(LogFile,"#>\n");
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
    fprintf(stdout,"#>\n");
    if (LogFile)
    {
      fprintdate(LogFile);
      fprintf(LogFile, "#> Number of OpenCL Platforms found: %i \n", numPlatforms);
      fprintdate(LogFile);
      fprintf(LogFile,"#>\n");
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

      // switch to selected platform
      if (opencl_user_platform!=-1&&opencl_user_platform!=(s32)i)
        continue;

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
        continue;
      }
      // get current platform
      platform = platforms[i];
      fprintf(stdout, "#> Platform: %i,  Vendor:  %s \n", i, pbuff);
      fprintf(stdout,"#>\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#> Platform: %i,  Vendor:  %s \n", i, pbuff);
        fprintdate(LogFile);
        fprintf(LogFile,"#>\n");
      }
      fprintf(stdout, "#> ### Query the OpenCL Devices on Platform...\n");
      fprintf(stdout,"#>\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile,"#> ### Query the OpenCL Devices on Platform...\n");
        fprintdate(LogFile);
        fprintf(LogFile,"#>\n");
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
      fprintf(stdout,"#>\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#> Number of OpenCL Devices found: %i \n", deviceListSize);
        fprintdate(LogFile);
        fprintf(LogFile,"#>\n");
      }

      // for each present OpenCL device do
      for(j=0; j < deviceListSize; j++)
      {

        // switch to selected device
        if (opencl_user_device!=-1&&opencl_user_device!=(s32)j)
          continue;

        failed = false;
        bestwarpmulti = warpmulti = 1;
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
          failed |= true;
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
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> ### Query and check the OpenCL Device...\n");
          fprintf(stdout,"#>\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> ### Query and check the OpenCL Device...\n");
            fprintdate(LogFile);
            fprintf(LogFile,"#>\n");
          }
          fprintf(stdout, "#> Device: %i, Device name: %s \n", j, deviceName);
          fprintf(stdout,"#>\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Device: %i, Device name: %s \n", j, deviceName);
            fprintdate(LogFile);
            fprintf(LogFile,"#>\n");
          }
        }

        // get endianess, only little endian tested
        cl_bool endianlittle = CL_FALSE;
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
          failed |= true;
        }
        else
        {
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
            failed |= true;
          }
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
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_MAX_COMPUTE_UNITS: %i \n",deviceunits);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_MAX_COMPUTE_UNITS: %i \n",deviceunits);
          }
        }

        // get max memory allocation size, for hash table
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
          failed |= true;
        }
        else
        {
          // check for min 64 mb memory
          if (devicememalloc < MINDEVICEMB*1024*1024 ) {
            fprintf(stdout, "#> Error, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " < %" PRIu64 " MB\n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#> Error, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " < %" PRIu64 " MB\n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
            }
            failed |= true;
          }
          else
          {
            fprintf(stdout, "#> OK, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " MB >= %" PRIu64 " MB \n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#> OK, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %" PRIu64 " MB >= %" PRIu64 " MB \n", devicememalloc/1024/1024, (u64)MINDEVICEMB);
            }
          }
        }
        // get global memory size, for calculating slots
        slots = 1;
        u64 devicememglobal = 1;
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_GLOBAL_MEM_SIZE,
                                  sizeof(cl_ulong),
                                  &devicememglobal,
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
          failed |= true;
        } 
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_GLOBAL_MEM_SIZE: %" PRIu64 " MB\n", devicememglobal/1024/1024);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_GLOBAL_MEM_SIZE: %" PRIu64 " MB\n", devicememglobal/1024/1024);
          }
        }

        // set memory to default max
        memalloc = devicememglobal/4;
        if (memalloc>MAXDEVICEMB*1024*1024)
          memalloc =  MAXDEVICEMB*1024*1024;
        if (memalloc>devicememalloc)
          memalloc =  devicememalloc;
        // memory slots
        slots = devicememglobal/memalloc;
        slots = (slots>MAXSLOTS)?MAXSLOTS:slots;
        // initialize transposition table
        ttbits = 0;
        mem = memalloc/(sizeof(TTE));
        while ( mem >>= 1)   // get msb
          ttbits++;
        mem = 1UL<<ttbits;
        ttbits=mem;
        memalloc = mem*sizeof(TTE); // set correct hash size
        memalloc = (memalloc==0)?1:memalloc;


        // check for needed device extensions
// local and global 32 bit functions faster on newer device...
// chose portability vs speed...
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
          failed |= true;
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
          failed |= true;
        } 

        // global in32 atomics
        if ((!strstr(ExtensionsValue, "cl_khr_global_int32_base_atomics")))
        {
          fprintf(stdout, "#> Error: Device extension cl_khr_global_int32_base_atomics not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device extension cl_khr_global_int32_base_atomics not supported.\n");
          }
          failed |= true;
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
*/
        
        // local 32 bit atomics
/*
        if ((!strstr(ExtensionsValue, "cl_khr_local_int32_base_atomics")))
        {
          fprintf(stdout, "#> Error: Device extension cl_khr_local_int32_base_atomics not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device extension cl_khr_local_int32_base_atomics not supported.\n");
          }
          failed |= true;
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
        // local 32 bit atomics
        if ((!strstr(ExtensionsValue, "cl_khr_local_int32_extended_atomics")))
        {
          fprintf(stdout, "#> Error: Device extension cl_khr_local_int32_extended_atomics not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Device extension cl_khr_local_int32_extended_atomics not supported.\n");
          }
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, Device extension cl_khr_local_int32_extended_atomics is supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Device extension cl_khr_local_int32_extended_atomics is supported.\n");
          }
        }
*/
        // 64 bit atomics, removed, Nvidia >= sm35 does not report the support
/*
        if ((!strstr(ExtensionsValue, "cl_khr_int64_extended_atomics")))
        {
          fprintf(stdout, "#> Unknown: Device extension cl_khr_int64_extended_atomics maybe not supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Unknown: Device extension cl_khr_int64_extended_atomics maybe not supported.\n");
          }
//          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, Device extension cl_khr_int64_extended_atomics is supported.\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, Device extension cl_khr_int64_extended_atomics is supported.\n");
          }
        }
*/
        // get work group size
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                  sizeof(size_t),
                                  &wgsize,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_MAX_WORK_GROUP_SIZE (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_MAX_WORK_GROUP_SIZE (clGetDeviceInfo)\n");
          }
          failed |= true;
        }
        if ((s32)wgsize<64)
        {
          fprintf(stdout, "#> Error, CL_DEVICE_MAX_WORK_GROUP_SIZE: %i < 64\n", (s32)wgsize);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error, CL_DEVICE_MAX_WORK_GROUP_SIZE: %i < 64\n", (s32)wgsize);
          }
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_MAX_WORK_GROUP_SIZE: %i >= 64\n", (s32)wgsize);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_MAX_WORK_GROUP_SIZE: %i >= 64\n", (s32)wgsize);
          }
        }

        // get work group item dims
        cl_uint dims = 0;
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                                  sizeof(cl_uint),
                                  &dims,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS(clGetDeviceInfo)\n");
          }
          failed |= true;
        }
        if (dims<3)
        {
          fprintf(stdout, "#> Error,  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: %i < 3\n", dims);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error,  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: %i < 3\n", dims);
          }
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: %i >= 3\n", dims);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS: %i >= 3\n", dims);
          }
        }

        // get work group item sizes
        size_t items[dims];
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_MAX_WORK_ITEM_SIZES,
                                  dims * sizeof(size_t),
                                  &items,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: CL_DEVICE_MAX_WORK_ITEM_SIZES (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: CL_DEVICE_MAX_WORK_ITEM_SIZES (clGetDeviceInfo)\n");
          }
          failed |= true;
        }
        if ((s32)items[2]<64)
        {
          fprintf(stdout, "#> Error, CL_DEVICE_MAX_WORK_ITEM_SIZES [3]: %i < 64\n", (s32)items[2]);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error, CL_DEVICE_MAX_WORK_ITEM_SIZES [3]: %i < 64\n", (s32)items[2]);
          }
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_MAX_WORK_ITEM_SIZES [3]: %i >= 64\n", (s32)items[2]);
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_MAX_WORK_ITEM_SIZES [3]: %i >= 64\n", (s32)items[2]);
          }
        }
  
        // get device available info
        cl_bool available = CL_FALSE;
        status = clGetDeviceInfo (devices[j],
                                  CL_DEVICE_AVAILABLE,
                                  sizeof(cl_bool),
                                  &available,
                                  NULL
                                  );

        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: CL_DEVICE_AVAILABLE (clGetDeviceInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: CL_DEVICE_AVAILABLE (clGetDeviceInfo)\n");
          }
          failed |= true;
        }
        if (available == CL_FALSE)
        {
          fprintf(stdout, "#> Error, CL_DEVICE_AVAILABLE: CL_FALSE \n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error, CL_DEVICE_AVAILABLE: CL_FALSE \n");
          }
          failed |= true;
        }
        else
        {
          fprintf(stdout, "#> OK, CL_DEVICE_AVAILABLE: CL_TRUE \n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> OK, CL_DEVICE_AVAILABLE: CL_TRUE \n");
          }
        }
        if (failed)
          continue;
/*
        // deprecated:
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
        kernel = clCreateKernel(program, "alphabeta_gpu", &status);
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
                                            CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                            0,
                                            NULL,
                                            &paramSize
                                          );
        if(status!=CL_SUCCESS) 
        {  
          fprintf(stdout, "#> Error: Getting CL_DEVICE_MAX_WORK_GROUP_SIZE size (clGetKernelWorkGroupInfo)\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> Error: Getting CL_DEVICE_MAX_WORK_GROUP_SIZE size (clGetKernelWorkGroupInfo)\n");
          }
          continue;
        }

        paramValue = (size_t *)malloc(1 * paramSize);
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
            fprintf(LogFile, "#> Error: CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE (clGetKernelWorkGroupInfo)\n");
          }
          continue;
        }
*/
        // print temp config file
        FILE 	*Cfg;
        remove("config.tmp");

        Cfg = fopen("config.tmp", "w");
        fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", deviceName);
        fprintf(Cfg, "threadsX: %i;\n", 1);
        fprintf(Cfg, "threadsY: %i;\n", 1);
        fprintf(Cfg, "nodes_per_second: %" PRIu64 ";\n", nps);
        fprintf(Cfg, "max_nodes: 0;\n");
        fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)memalloc/1024/1024);
        fprintf(Cfg, "memory_slots: %i; // max %i \n", 1, (s32)slots);
        fprintf(Cfg, "opencl_platform_id: %i;\n",i);
        fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
        fprintf(Cfg,"\n");
        fprintf(Cfg,"threadsX\n");
        fprintf(Cfg,"Number of Compute Units resp. CPU cores\n");
        fprintf(Cfg,"Each of these threads runs 64 Work-Items in one Work-Group\n");
        fprintf(Cfg,"threadsY\n");
        fprintf(Cfg,"Multiplier for threadsX,\n");
        fprintf(Cfg,"run multiple Work-Groups per Compute Unit\n");
        fprintf(Cfg,"nodes_per_second\n");
        fprintf(Cfg,"nps of device, for initial time control\n");
        fprintf(Cfg,"max_nodes\n");
        fprintf(Cfg,"search n nodes only, 0 is inf\n");
        fprintf(Cfg,"max_memory\n");
        fprintf(Cfg,"Allocate n MB of memory on device for hash table\n");
        fprintf(Cfg,"memory_slots\n");
        fprintf(Cfg,"Allocate n times max_memory on device\n");
        fprintf(Cfg,"opencl_platform_id\n");
        fprintf(Cfg,"Which OpenCL platform to use\n");
        fprintf(Cfg,"opencl_device_id\n");
        fprintf(Cfg,"Which OpenCL device to use\n\n");
        fclose(Cfg);

        fprintf(stdout, "#\n");
        fprintf(stdout, "#> ### Running NPS-Benchmark for minimal config on device,\n");
        fprintf(stdout, "#> ### this can last about %i seconds... \n", benchsec);
        fprintf(stdout, "#> ### threadsX: %i \n", 1);
        fprintf(stdout, "#> ### threadsY: %i \n", 1);
        fprintf(stdout, "#> ### total work-groups: %i \n", 1*1);
        fprintf(stdout, "#> ### total threads: %i \n", 1*1*64);
        fprintf(stdout, "#\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#\n");
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### Running NPS-Benchmark for minimal config on device,\n");
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### this can last about %i seconds... \n", benchsec);
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### threadsX: %i \n", 1);
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### threadsY: %i \n", 1);
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### total work-groups: %i \n", 1*1);
          fprintdate(LogFile);
          fprintf(LogFile, "#> ### total threads: %i \n", 1*1*64);
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
          fprintf(stdout, "#> ### Running NPS-Benchmark for best config,\n");
          fprintf(stdout, "#> ### this can last about some minutes... \n");
          fprintf(stdout, "#\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#\n");
            fprintdate(LogFile);
            fprintf(LogFile, "#> ### Running NPS-Benchmark for best config,\n");
            fprintdate(LogFile);
            fprintf(LogFile, "#> ### this can last about some minutes... \n");
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
            fprintf(Cfg, "nodes_per_second: %" PRI64 ";\n", npstmp);
            fprintf(Cfg, "max_nodes: 0;\n");
            fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)memalloc/1024/1024);
            fprintf(Cfg, "memory_slots: %i; // max %i \n", (s32)slots,(s32)slots);
            fprintf(Cfg, "opencl_platform_id: %i;\n",i);
            fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
            fprintf(Cfg,"\n");
            fclose(Cfg);

            fprintf(stdout, "#\n");
            fprintf(stdout, "#> ### Running NPS-Benchmark for threadsY on device, this can last about %i seconds... \n", benchsec);
            fprintf(stdout, "#\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### Running NPS-Benchmark for threadsY on device, this can last about %i seconds... \n", benchsec);
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
//          nps = 0;
          npstmp = 0;
//          int iter = 0;
          while (true)
          {
            Cfg = fopen("config.tmp", "w");
            fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", deviceName);
            fprintf(Cfg, "threadsX: %i;\n", deviceunits);
            fprintf(Cfg, "threadsY: %i;\n", warpmulti);
            fprintf(Cfg, "nodes_per_second: %" PRIi64 ";\n", npstmp);
            fprintf(Cfg, "max_nodes: 0;\n");
            fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)memalloc/1024/1024);
            fprintf(Cfg, "memory_slots: %i; // max %i \n", (s32)slots, (s32)slots);
            fprintf(Cfg, "opencl_platform_id: %i;\n",i);
            fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
            fprintf(Cfg,"\n");
            fclose(Cfg);

            fprintf(stdout, "#\n");
            fprintf(stdout, "#> ### Running NPS-Benchmark for threadsY on device,\n");
            fprintf(stdout, "#> ### this can last about %i seconds... \n", benchsec);
            fprintf(stdout, "#> ### threadsX: %i \n", deviceunits);
            fprintf(stdout, "#> ### threadsY: %i \n", warpmulti);
            fprintf(stdout, "#> ### total work-groups: %i \n", deviceunits*warpmulti);
            fprintf(stdout, "#> ### total threads: %i \n", deviceunits*warpmulti*64);
            fprintf(stdout, "#\n");
            if (LogFile)
            {
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### Running NPS-Benchmark for threadsY on device,\n");
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### this can last about %i seconds... \n", benchsec);
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### threadsX: %i \n", deviceunits);
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### threadsY: %i \n", warpmulti);
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### total work-groups: %i \n", deviceunits*warpmulti);
              fprintdate(LogFile);
              fprintf(LogFile, "#> ### total threads: %i \n", deviceunits*warpmulti*64);
              fprintdate(LogFile);
              fprintf(LogFile, "#\n");
            }
            npstmp = benchmarkWrapper(benchsec);
            remove("config.tmp");

            // something went wrong
            if (npstmp<=0)
              break;
            // check for 10% speedup margin
            // increase threadsY
            if (npstmp/1.10>=nps)
            {
              bestwarpmulti = warpmulti;
              warpmulti*=2;
              nps = npstmp;
//              iter = 0;
            }
            else
            {
              break;
              // check non pow2 multis
/*
              iter++;
              warpmulti = bestwarpmulti;
              for(int i=0;i<iter&&warpmulti>=4;i++)
                warpmulti = (int)ceil((float)warpmulti/2+(float)warpmulti/4);

              if (iter>=3)
                break;
*/
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
        fprintf(Cfg, "threadsX: %i;\n", (!extreme)?1:deviceunits);
        fprintf(Cfg, "threadsY: %i;\n", (!extreme)?1:bestwarpmulti);
        fprintf(Cfg, "nodes_per_second: %" PRIu64 ";\n", nps);
        fprintf(Cfg, "max_nodes: 0;\n");
        fprintf(Cfg, "max_memory: %i; // in MB\n", (s32)memalloc/1024/1024);
        fprintf(Cfg, "memory_slots: %i; // max %i \n", (!extreme)?1:(s32)slots, (s32)slots);
        fprintf(Cfg, "opencl_platform_id: %i;\n",i);
        fprintf(Cfg, "opencl_device_id: %i;\n\n",j);
        fprintf(Cfg,"\n");
        fprintf(Cfg,"threadsX\n");
        fprintf(Cfg,"Number of Compute Units resp. CPU cores\n");
        fprintf(Cfg,"Each of these threads runs 64 Work-Items in one Work-Group\n");
        fprintf(Cfg,"threadsY\n");
        fprintf(Cfg,"Multiplier for threadsX,\n");
        fprintf(Cfg,"run multiple Work-Groups per Compute Unit\n");
        fprintf(Cfg,"nodes_per_second\n");
        fprintf(Cfg,"nps of device, for initial time control\n");
        fprintf(Cfg,"max_nodes\n");
        fprintf(Cfg,"search n nodes only, 0 is inf\n");
        fprintf(Cfg,"max_memory\n");
        fprintf(Cfg,"Allocate n MB of memory on device for hash table\n");
        fprintf(Cfg,"memory_slots\n");
        fprintf(Cfg,"Allocate n times max_memory on device\n");
        fprintf(Cfg,"opencl_platform_id\n");
        fprintf(Cfg,"Which OpenCL platform to use\n");
        fprintf(Cfg,"opencl_device_id\n");
        fprintf(Cfg,"Which OpenCL device to use\n\n");
        fclose(Cfg);

        fprintf(stdout, "#\n");
        fprintf(stdout, "#\n");
        fprintf(stdout, "// Zeta OpenCL Chess config file for %s \n\n", deviceName);
        fprintf(stdout, "threadsX: %i;\n", (!extreme)?1:deviceunits);
        fprintf(stdout, "threadsY: %i;\n", (!extreme)?1:bestwarpmulti);
        fprintf(stdout, "nodes_per_second: %" PRIu64 ";\n", nps);
        fprintf(stdout, "max_nodes: 0;\n");
        fprintf(stdout, "max_memory: %i; // in MB\n", (s32)memalloc/1024/1024);
        fprintf(stdout, "memory_slots: %i; // max %i\n", (!extreme)?1:(s32)slots, (s32)slots);
        fprintf(stdout, "opencl_platform_id: %i;\n",i);
        fprintf(stdout, "opencl_device_id: %i;\n\n",j);
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "#\n");
          fprintf(LogFile, "#\n");
          fprintf(LogFile, "// Zeta OpenCL Chess config file for %s \n\n", deviceName);
          fprintf(LogFile, "threadsX: %i;\n", (!extreme)?1:deviceunits);
          fprintf(LogFile, "threadsY: %i;\n", (!extreme)?1:bestwarpmulti);
          fprintf(LogFile, "nodes_per_second: %" PRIu64 ";\n", nps);
          fprintf(LogFile, "max_nodes: 0;\n");
          fprintf(LogFile, "max_memory: %i; // in MB\n", (s32)memalloc/1024/1024);
          fprintf(LogFile, "memory_slots: %i; // max %i\n", (!extreme)?1:(s32)slots, (s32)slots);
          fprintf(LogFile, "opencl_platform_id: %i;\n",i);
          fprintf(LogFile, "opencl_device_id: %i;\n\n",j);
        }

        fprintf(stdout, "##### Above output was saved in file %s \n", confignamefile);
        fprintf(stdout, "##### rename it to config.ini to let engine use it\n");
        fprintf(stdout, "#\n");
        if (LogFile)
        {
          fprintdate(LogFile);
          fprintf(LogFile, "##### Above output was saved in file %s \n", confignamefile);
          fprintdate(LogFile);
          fprintf(LogFile, "##### rename it to config.ini to let engine use it\n");
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

