/*
    Zeta CL, OpenCL Chess Engine
    Author: Srdja Matovic <srdja.matovic@googlemail.com>
    Created at: 20-Jan-2011
    Updated at:
    Description: A Chess Engine written in OpenCL, a language suited for GPUs.

    Copyright (C) 2011 Srdja Matovic
    This program is distributed under the GNU General Public License.
    See file COPYING or http://www.gnu.org/licenses/*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "zeta.h"


extern signed int benchmarkNPS();
extern void read_config();
extern U64 max_mem_mb;
extern int load_file_to_string(const char *filename, char **result);
extern const char filename[];
extern int initializeCLDevice();
extern int initializeCL();
extern int opencl_device_id;

int GuessConfig( int extreme) {

    cl_int status = 0;
    size_t paramSize;
    char *paramValue;
    char *ExtensionsValue;
    cl_device_id *devices;
  	unsigned int i,j,k;
    cl_uint deviceunits = 0;
    cl_ulong devicememalloc = 0;
    int *warpVal;
    int warpsize = 1;
    int warpmulti = 1;
    signed int nps = 0;
    signed int npstmp = 0;
    signed int npsreal = 0;
    int devicecounter = 0;
    int benchsec = 10;
    


    printf("#> ### Query the OpenCL Platforms on Host...\n");

    status = clGetPlatformIDs(256, NULL, &numPlatforms);
    if(status != CL_SUCCESS)
    {
        printf("#> Error: No OpenCL Platforms detected\n");
        return 1;
    }
    
    if(numPlatforms > 0)
    {

        printf("#> Number of OpenCL Platforms found: %i \n", numPlatforms);

        cl_platform_id* platforms = (cl_platform_id *) malloc(numPlatforms);

        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if(status != CL_SUCCESS)
        {
            printf("#> Error: Getting Platform Ids. (clGetPlatformsIDs)\n");
            return 1;
        }
        for(i=0; i < numPlatforms; ++i)
        {
            char pbuff[256];
            status = clGetPlatformInfo(
                        platforms[i],
                        CL_PLATFORM_VENDOR,
                        sizeof(pbuff),
                        pbuff,
                        NULL);
            if(status != CL_SUCCESS)
            {
                printf("#> Error: Getting Platform Info.(clGetPlatformInfo)\n");
                return 1;
            }
            platform = platforms[i];

            printf("#> Platform: %i,  Vendor:  %s \n", i, pbuff);

            printf("#> ### Query the OpenCL Devices on Platform...\n");

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
		        printf("#> Error: Getting DeviceListSize (clGetDeviceIDs)\n");
		        continue;
	        }

	        if(deviceListSize == 0)
	        {
		        printf("#> Error: No devices found.\n");
		        continue;
	        }

            devices = (cl_device_id *)malloc(deviceListSize * sizeof(cl_device_id));


            status = clGetDeviceIDs(platform, 
                                      CL_DEVICE_TYPE_ALL, 
                                      deviceListSize, 
                                      devices, 
                                      NULL);
            if(status != CL_SUCCESS) 
	        {  
		        printf("#> Error: Getting DeviceIDs (clGetDeviceIDs)\n");
		        continue;
	        }

            
            printf("#> Number of OpenCL Devices found: %i \n", deviceListSize);


            for(j=0; j < deviceListSize; j++) {

                warpmulti = 1;

                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_NAME,
                                0,
                                NULL,
                                &paramSize
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting Device Name Size (clGetDeviceInfo)\n");
    		        continue;
	            }


                paramValue = (char *)malloc(1 * paramSize);

                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_NAME,
                                paramSize,
                                paramValue,
                                NULL
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting Device Name (clGetDeviceInfo)\n");
    		        continue;
	            }
    
                printf("#> ### Query and check the OpenCL Device...\n");
                printf("#> Device: %i, Device name: %s \n", j, paramValue);

                cl_bool endianlittle = CL_FALSE;
                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_ENDIAN_LITTLE,
                                sizeof(cl_bool),
                                &endianlittle,
                                NULL
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting Device Endianess (clGetDeviceInfo)\n");
    		        continue;
	            }
  
                if (endianlittle == CL_TRUE)
                    printf("#> OK, Device Endianness is little\n");
                else {
                    printf("#> Error: Device Endianness is not little\n");
    		        continue;
                }

                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_MAX_COMPUTE_UNITS,
                                sizeof(cl_uint),
                                &deviceunits,
                                NULL
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting CL_DEVICE_MAX_COMPUTE_UNITS (clGetDeviceInfo)\n");
    		        continue;
	            } 
                printf("#> OK, CL_DEVICE_MAX_COMPUTE_UNITS: %i \n",deviceunits);


                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_MAX_MEM_ALLOC_SIZE ,
                                sizeof(cl_ulong),
                                &devicememalloc,
                                NULL
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting CL_DEVICE_MAX_MEM_ALLOC_SIZE (clGetDeviceInfo)\n");
    		        continue;
	            } 

                if (devicememalloc < 67108864 ) {
                    printf("#> Error, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %lu < 64 MB\n", devicememalloc/1024/1024);
    		        continue;
                }
                else
                    printf("#> OK, CL_DEVICE_MAX_MEM_ALLOC_SIZE: %lu MB > 64 MB \n", devicememalloc/1024/1024 );

                // set memory to max
                if (devicememalloc > max_mem_mb*1024*1024 )
                    devicememalloc = max_mem_mb*1024*1024; 




                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_EXTENSIONS,
                                0,
                                NULL,
                                &paramSize
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting CL_DEVICE_EXTENSIONS size (clGetDeviceInfo)\n");
    		        continue;
	            } 

                ExtensionsValue = (char *)malloc(1 * paramSize);

                status = clGetDeviceInfo (devices[j],
                                CL_DEVICE_EXTENSIONS,
                                paramSize,
                                ExtensionsValue,
                                NULL
                                );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting CL_DEVICE_EXTENSIONS value (clGetDeviceInfo)\n");
    		        continue;
	            } 

                if ( (!strstr(ExtensionsValue, "cl_khr_global_int32_base_atomics")) ) {
		            printf("#> Error: Device extension cl_khr_global_int32_base_atomics not supported.\n");
    		        continue;
                }
                else
		            printf("#> OK, Device extension cl_khr_global_int32_base_atomics is supported.\n");
            

/*
                if ( (!strstr(ExtensionsValue, "cl_khr_local_int32_base_atomics")) ) {
		            printf("#> Error: Device extension cl_khr_local_int32_base_atomics not supported.\n");
    		        continue;
                }
                else
		            printf("#> OK, Device extension cl_khr_local_int32_base_atomics is supported.\n");
*/
                if ( (!strstr(ExtensionsValue, "cl_khr_global_int32_extended_atomics")) ) {
		            printf("#> Error: Device extension cl_khr_global_int32_extended_atomics not supported.\n");
    		        continue;
                }
                else
		            printf("#> OK, Device extension cl_khr_global_int32_extended_atomics is supported.\n");


                // getting prefered Warpsize, 
                 printf("#> #### Query Kernel params.\n");

	            /////////////////////////////////////////////////////////////////
	            // Create an OpenCL context
	            /////////////////////////////////////////////////////////////////
                cps[0] = CL_CONTEXT_PLATFORM;
                cps[1] = (cl_context_properties)platform;
                cps[2] = 0;

                context = clCreateContext(cps, 
                                                  1, 
                                                  &devices[j],
                                                  NULL, 
                                                  NULL, 
                                                  &status);
                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Creating Context Info (cps, clCreateContext)\n");
		            continue;
	            }
                else
		            printf("#> OK, Creating Context Info\n");


	            /////////////////////////////////////////////////////////////////
	            // Create an OpenCL command queue
	            /////////////////////////////////////////////////////////////////
                commandQueue = clCreateCommandQueue(
					               context, 
                                   devices[j], 
                                   0, 
                                   &status);
                if(status != CL_SUCCESS) 
	            { 
		            printf("#> Error: Creating Command Queue. (clCreateCommandQueue)\n");
		            continue;
	            }
                else
		            printf("#> OK, Creating Command Queue\n");

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
                    printf("#> Error: Loading Binary into cl_program (clCreateProgramWithBinary)\n");
                    continue;
                }   
                else
                    printf("#> OK, Loading Binary into cl_program\n");


                // create a cl program executable for all the devices specified
                status = clBuildProgram(program, 1, &devices[j], NULL, NULL, NULL);

                if(status != CL_SUCCESS) 
	            { 
                    char* build_log=0;
                    size_t log_size=0;
                    FILE 	*temp=0;

		            printf("#> Error: Building Program, see file zeta.debug for build log (clBuildProgram)\n");


                    // Shows the log
                    // First call to know the proper size
                    clGetProgramBuildInfo(program, devices[j], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
                    build_log = (char *) malloc(log_size+1);

                    if(status != CL_SUCCESS) 
                    { 
                      printf("#> Error: Building Log size (clGetProgramBuildInfo)\n");
                    }

                    // Second call to get the log
                    status = clGetProgramBuildInfo(program, devices[j], CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
                    //build_log[log_size] = '\0';

                    if(status != CL_SUCCESS) 
                    { 
                      printf("#> Error: Building Log (clGetProgramBuildInfo)\n");
                    }

                    temp = fopen("zeta.debug", "ab+");
                    fprintf(temp, "buildlog: %s \n", build_log);
                    fclose(temp);
                 

                    continue;
	            }
                else
		            printf("#> OK, Building Program\n");

                // get a kernel object handle for a kernel with the given name
                kernel = clCreateKernel(program, "bestfirst_gpu", &status);
                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Creating Kernel from program. (clCreateKernel)\n");
                    continue;
	            }
                else
		            printf("#> OK, Creating Kernel from program\n");



                status = clGetKernelWorkGroupInfo ( kernel,
                                                    devices[j],
                                                    CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                                    0,
                                                    NULL,
                                                    &paramSize
                                                  );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE size (clGetKernelWorkGroupInfo)\n");
		            continue;
	            }

                warpVal = (int *)malloc(1 * paramSize);

                status = clGetKernelWorkGroupInfo ( kernel,
                                                    devices[j],
                                                    CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                                                    paramSize,
                                                    warpVal,
                                                    NULL
                                                  );

                if(status != CL_SUCCESS) 
	            {  
		            printf("#> Error: Getting CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE (clGetKernelWorkGroupInfo)\n");
		            continue;
	            }

                warpsize = (int)*warpVal;
                printf("#> OK, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: %i.\n", warpsize);



                FILE 	*Cfg;
                remove("config.tmp");

                Cfg = fopen("config.tmp", "ab");

                fprintf(Cfg, "threadsX: %i; // Number of SIMD Units or CPU cores\n", deviceunits);
                fprintf(Cfg, "threadsY: %i; // Multiplier for threadsZ \n", warpmulti);
                fprintf(Cfg, "threadsZ: %i; // Number of threads per SIMD Unit or Core\n\n", warpsize);

                fprintf(Cfg, "nodes_per_second: 1; \n");
                fprintf(Cfg, "max_nodes_to_expand: %i; \n", (int)(devicememalloc/sizeof( NodeBlock)));
                fprintf(Cfg, "max_nodes: 0; \n");
                fprintf(Cfg,"memory_slots: %i; // max 3 \n", memory_slots);
                fprintf(Cfg, "max_leaf_depth: 0; \n");
                fprintf(Cfg, "max_depth: 32; \n");
                fprintf(Cfg, "reuse_node_tree: 0; // unstable\n\n");

                fprintf(Cfg, "opencl_platform_id: %i; \n",i);
                fprintf(Cfg, "opencl_device_id: %i; \n\n",j);

                fclose(Cfg);

                printf("#> ### Running NPS-Benchmark for minimal config on device, this can last %i seconds... \n", benchsec);

                npsreal = benchmarkNPS(benchsec);

                remove("config.tmp");
                
                if (npsreal <= 0) {
                    printf("#> ### Benchmark failed, see zeta.debug file for more info... \n");
                    continue;
                }

                if (extreme == 1) {

                    printf("\n\n");
                    printf("#> ### Running NPS-Benchmark for best config, this can last some minutes... \n\n");

                    nps = npsreal;
                    npstmp =  npsreal;

                    printf("\n\n");

 /*
                   // get threadsZ, warpsize, deprecated...
                    while (npstmp >= nps) {

                        nps = npstmp;

                        Cfg = fopen("config.tmp", "ab");

                        fprintf(Cfg, "threadsX: %i; // Number of SIMD Units or CPU cores\n", deviceunits);
                        fprintf(Cfg, "threadsY: %i; // Multiplier for threadsZ \n", warpmulti);
                        fprintf(Cfg, "threadsZ: %i; // Number of threads per SIMD Unit or Core\n\n", warpsize*2);

                        fprintf(Cfg, "nodes_per_second: 1; \n");
                        fprintf(Cfg, "max_nodes_to_expand: %i; \n", (int)(devicememalloc/sizeof( NodeBlock)));
                        fprintf(Cfg, "max_nodes: 0; \n");
                        fprintf(Cfg, "memory_slots: %i; // max 3 \n", memory_slots);
                        fprintf(Cfg, "max_leaf_depth: 0; \n");
                        fprintf(Cfg, "max_depth: 32; \n");
                        fprintf(Cfg, "reuse_node_tree: 0; // unstable\n\n");

                        fprintf(Cfg, "opencl_platform_id: %i; \n",i);
                        fprintf(Cfg, "opencl_device_id: %i; \n\n",j);

                        fclose(Cfg);

                        printf("#> ### Running NPS-Benchmark for threadsZ on device, this can last %i seconds... \n", benchsec);

                        npstmp = benchmarkNPS(benchsec);

                        remove("config.tmp");

                        // something went wrong
                        if (npstmp <= 0) {
                            npstmp = nps;
                            break;
                        }

                        // 10% speedup margin
                        if (npstmp <= nps*1.10) {
                            npstmp = nps;
                            break;
                        }

                        if (npstmp >= nps )
                            warpsize*=2;
                    }

                    printf("\n\n");
                    npstmp = nps;
*/


                    // get threadsY, multi for warpsize
                    while (npstmp >= nps ) {

                        nps = npstmp;

                        Cfg = fopen("config.tmp", "ab");

                        fprintf(Cfg, "threadsX: %i; // Number of SIMD Units or CPU cores\n", deviceunits);
                        fprintf(Cfg, "threadsY: %i; // Multiplier for threadsZ \n", warpmulti*2);
                        fprintf(Cfg, "threadsZ: %i; // Number of threads per SIMD Unit or Core\n\n", warpsize);

                        fprintf(Cfg, "nodes_per_second: 1; \n");
                        fprintf(Cfg, "max_nodes_to_expand: %i; \n", (int)(devicememalloc/sizeof( NodeBlock)));
                        fprintf(Cfg, "max_nodes: 0; \n");
                        fprintf(Cfg, "memory_slots: %i; // max 3 \n", memory_slots);
                        fprintf(Cfg, "max_leaf_depth: 0; \n");
                        fprintf(Cfg, "max_depth: 32; \n");
                        fprintf(Cfg, "reuse_node_tree: 0; // unstable\n\n");

                        fprintf(Cfg, "opencl_platform_id: %i; \n",i);
                        fprintf(Cfg, "opencl_device_id: %i; \n\n",j);

                        fclose(Cfg);

                        printf("#> ### Running NPS-Benchmark for threadsY on device, this can last %i seconds... \n", benchsec);

                        npstmp = benchmarkNPS(benchsec);

                        remove("config.tmp");

                        // something went wrong
                        if (npstmp <= 0) {
                            npstmp = nps;
                            break;
                        }

                        // 10% speedup margin
                        if (npstmp <= nps*1.10) {
                            npstmp = nps;
                            break;
                        }

                        if (npstmp >= nps )
                            warpmulti*=2;
                    }

                    remove("config.tmp");

                    printf("\n\n");

                }

                if (nps >= npsreal)
                    npsreal = nps;

                devicecounter++;
                char confignamefile[256] = "config.ini";
                k = strlen(confignamefile);
                sprintf(confignamefile + k , "_%i_%i", i,j );

                remove(confignamefile);

                Cfg = fopen(confignamefile, "ab");


                fprintf(Cfg,"// Zeta OpenCL Chess config file for %s \n\n", paramValue);

                fprintf(Cfg, "threadsX: %i; // Number of SIMD Units or CPU cores\n", deviceunits);
                fprintf(Cfg, "threadsY: %i; // Multiplier for threadsZ \n", warpmulti);
                fprintf(Cfg, "threadsZ: %i; // Number of threads per SIMD Unit or Core\n\n", warpsize);

                fprintf(Cfg,"nodes_per_second: %i; \n", npsreal);
                fprintf(Cfg,"max_nodes_to_expand: %li; // %i MB \n", (int)devicememalloc/sizeof( NodeBlock), (int)(devicememalloc/1024/1024));
                fprintf(Cfg,"max_nodes: 0; \n");
                fprintf(Cfg,"memory_slots: %i; // max 3 \n", memory_slots);
                fprintf(Cfg,"max_leaf_depth: 0; \n");
                fprintf(Cfg,"max_depth: 32; \n");
                fprintf(Cfg,"reuse_node_tree: 0; // unstable\n\n");

                fprintf(Cfg,"opencl_platform_id: %i; \n",i);
                fprintf(Cfg,"opencl_device_id: %i; \n\n",j);

                fclose(Cfg);

                printf("\n\n");                

                printf("// Zeta OpenCL Chess config file for %s \n\n", paramValue);

                printf("threadsX: %i; // Number of SIMD Units or CPU cores\n", deviceunits);
                printf("threadsY: %i; // Multiplier for threadsZ \n", warpmulti);
                printf("threadsZ: %i; // Number of threads per SIMD Unit or Core\n\n", warpsize);

                printf("nodes_per_second: %i; \n", npsreal);
                printf("max_nodes_to_expand: %li; // %i MB \n", (int)devicememalloc/sizeof( NodeBlock),(int)(devicememalloc/1024/1024));
                printf("max_nodes: 0; \n");
                printf("memory_slots: %i; // max 3 \n", memory_slots);
                printf("max_leaf_depth: 0; \n");
                printf("max_depth: 32; \n");
                printf("reuse_node_tree: 0; // unstable\n\n");

                printf("opencl_platform_id: %i; \n",i);
                printf("opencl_device_id: %i; \n\n",j);

                printf("\n\n");

                printf("##### Above output was saved in file %s \n\n", confignamefile);

            }
        }
    }

    if(NULL == platform)
    {
        printf("#> Error: No OpenCL Platforms detected\n");
        return 1;
    }



    return 0;
}

