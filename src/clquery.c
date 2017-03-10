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

#include "timer.h"
#include "zeta.h"       // for global vars

extern s32 benchmarkWrapper(s32 benchsec);
extern void read_config();

// show opencl platforms
bool cl_platform_list()
{
  cl_int status = 0;
  u32 i;
    
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
      fprintf(stdout, "#> platform_id: %i,  Vendor:  %s \n", i, pbuff);
      fprintf(stdout,"#>\n");
      if (LogFile)
      {
        fprintdate(LogFile);
        fprintf(LogFile, "#> platform_id: %i,  Vendor:  %s \n", i, pbuff);
        fprintdate(LogFile);
        fprintf(LogFile,"#>\n");
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
// show opencl devices
bool cl_device_list()
{
  bool failed = false;
  cl_int status = 0;
  size_t paramSize;
  char *deviceName;
  cl_device_id *devices;
  u32 i,j;
    
  fprintf(stdout,"#> ### Query the OpenCL Devices on Host...\n");
  fprintf(stdout,"#>\n");
  if (LogFile)
  {
    fprintdate(LogFile);
    fprintf(LogFile,"#> ### Query the OpenCL Devices on Host...\n");
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
      // for each present OpenCL device do
      for(j=0; j < deviceListSize; j++)
      {
        failed = false;
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
          fprintf(stdout, "#> platform_id:%i, device_id: %i, Vendor: %s, Device name: %s \n", i, j, pbuff, deviceName);
          fprintf(stdout, "#>\n");
          if (LogFile)
          {
            fprintdate(LogFile);
            fprintf(LogFile, "#> platform_id:%i, device_id: %i, Vendor: %s, Device name: %s \n", i, j, pbuff, deviceName);
            fprintdate(LogFile);
            fprintf(LogFile, "#>\n");
          }
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

