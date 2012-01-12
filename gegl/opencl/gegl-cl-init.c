/******************************************************************************
 * This file is part of GEGL. It's the initialization of GEGL OpenCL module.
 *
 * GEGL is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * GEGL is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * The Idea of dynamic loading OpenCL is from DarkZeros (No contact).
 * Victor Oliveira (victormatheus@gmail.com) improved it and made it more
 * effective.
 * Peixuan Zhang (zhangpeixuancn@gmail.com) modified it based on actual
 * demand.
 *
 * Copyright 2012 GEGL Team
 *
******************************************************************************/

#define __DYNAMIC_LOADING_CL_MAIN_C__
#include "gegl-cl-init.h"
#undef  __DYNAMIC_LOADING_CL_MAIN_C__

#include <gmodule.h>
#include <string.h>
#include <stdio.h>

#include <windows.h>
#define CL_LOAD_FUNCTION(func)

////



///////
gboolean
gegl_cl_is_opencl_available(void)
{
    return cl_status.is_opencl_available;
}

cl_platform_id
gegl_cl_get_platform_id(void)
{
    return cl_status.platform_id;
}

cl_context
gegl_cl_get_context(void)
{
    return cl_status.context;
}

cl_device_id
gegl_cl_get_device_id(void)
{
    return cl_status.device_id;
}

cl_command_queue
gegl_cl_get_command_queue(void)
{
    return cl_status.command_queue;
}



gboolean
gegl_cl_init(void)
{
    cl_int status;

    if (!cl_status.is_opencl_available)
    {
#ifdef G_OS_WIN32
        HINSTANCE module;
        module = LoadLibrary("OpenCL.dll");
#else
        GModule *module;
        module = g_module_open("libOpenCL.so", G_MODULE_BIND_LAZY);
#endif
        if (!module)
        {
            g_warning ("Unable to load OpenCL library");
            return FALSE;
        }

        CL_LOAD_FUNCTION(clGetPlatformIDs                )
        CL_LOAD_FUNCTION(clGetPlatformInfo               )
        CL_LOAD_FUNCTION(clGetDeviceIDs                  )
        CL_LOAD_FUNCTION(clGetDeviceInfo                 )
        CL_LOAD_FUNCTION(clCreateContext                 )
        CL_LOAD_FUNCTION(clCreateContextFromType         )
        CL_LOAD_FUNCTION(clRetainContext                 )
        CL_LOAD_FUNCTION(clReleaseContext                )
        CL_LOAD_FUNCTION(clGetContextInfo                )
        CL_LOAD_FUNCTION(clCreateCommandQueue            )
        CL_LOAD_FUNCTION(clRetainCommandQueue            )
        CL_LOAD_FUNCTION(clReleaseCommandQueue           )
        CL_LOAD_FUNCTION(clGetCommandQueueInfo           )
        CL_LOAD_FUNCTION(clSetCommandQueueProperty       )
        CL_LOAD_FUNCTION(clCreateBuffer                  )
        CL_LOAD_FUNCTION(clCreateSubBuffer               )
        CL_LOAD_FUNCTION(clCreateImage2D                 )
        CL_LOAD_FUNCTION(clCreateImage3D                 )
        CL_LOAD_FUNCTION(clRetainMemObject               )
        CL_LOAD_FUNCTION(clReleaseMemObject              )
        CL_LOAD_FUNCTION(clGetSupportedImageFormats      )
        CL_LOAD_FUNCTION(clGetMemObjectInfo              )
        CL_LOAD_FUNCTION(clGetImageInfo                  )
        CL_LOAD_FUNCTION(clSetMemObjectDestructorCallback)
        CL_LOAD_FUNCTION(clCreateSampler                 )
        CL_LOAD_FUNCTION(clRetainSampler                 )
        CL_LOAD_FUNCTION(clReleaseSampler                )
        CL_LOAD_FUNCTION(clGetSamplerInfo                )
        CL_LOAD_FUNCTION(clCreateProgramWithSource       )
        CL_LOAD_FUNCTION(clCreateProgramWithBinary       )
        CL_LOAD_FUNCTION(clRetainProgram                 )
        CL_LOAD_FUNCTION(clReleaseProgram                )
        CL_LOAD_FUNCTION(clBuildProgram                  )
        CL_LOAD_FUNCTION(clUnloadCompiler                )
        CL_LOAD_FUNCTION(clGetProgramInfo                )
        CL_LOAD_FUNCTION(clGetProgramBuildInfo           )
        CL_LOAD_FUNCTION(clCreateKernel                  )
        CL_LOAD_FUNCTION(clCreateKernelsInProgram        )
        CL_LOAD_FUNCTION(clRetainKernel                  )
        CL_LOAD_FUNCTION(clReleaseKernel                 )
        CL_LOAD_FUNCTION(clSetKernelArg                  )
        CL_LOAD_FUNCTION(clGetKernelInfo                 )
        CL_LOAD_FUNCTION(clGetKernelWorkGroupInfo        )
        CL_LOAD_FUNCTION(clWaitForEvents                 )
        CL_LOAD_FUNCTION(clGetEventInfo                  )
        CL_LOAD_FUNCTION(clCreateUserEvent               )
        CL_LOAD_FUNCTION(clRetainEvent                   )
        CL_LOAD_FUNCTION(clReleaseEvent                  )
        CL_LOAD_FUNCTION(clSetUserEventStatus            )
        CL_LOAD_FUNCTION(clSetEventCallback              )
        CL_LOAD_FUNCTION(clGetEventProfilingInfo         )
        CL_LOAD_FUNCTION(clFlush                         )
        CL_LOAD_FUNCTION(clFinish                        )
        CL_LOAD_FUNCTION(clEnqueueReadBuffer             )
        CL_LOAD_FUNCTION(clEnqueueReadBufferRect         )
        CL_LOAD_FUNCTION(clEnqueueWriteBuffer            )
        CL_LOAD_FUNCTION(clEnqueueWriteBufferRect        )
        CL_LOAD_FUNCTION(clEnqueueCopyBuffer             )
        CL_LOAD_FUNCTION(clEnqueueCopyBufferRect         )
        CL_LOAD_FUNCTION(clEnqueueReadImage              )
        CL_LOAD_FUNCTION(clEnqueueWriteImage             )
        CL_LOAD_FUNCTION(clEnqueueCopyImage              )
        CL_LOAD_FUNCTION(clEnqueueCopyImageToBuffer      )
        CL_LOAD_FUNCTION(clEnqueueCopyBufferToImage      )
        CL_LOAD_FUNCTION(clEnqueueMapBuffer              )
        CL_LOAD_FUNCTION(clEnqueueMapImage               )
        CL_LOAD_FUNCTION(clEnqueueUnmapMemObject         )
        CL_LOAD_FUNCTION(clEnqueueNDRangeKernel          )
        CL_LOAD_FUNCTION(clEnqueueTask                   )
        CL_LOAD_FUNCTION(clEnqueueNativeKernel           )
        CL_LOAD_FUNCTION(clEnqueueMarker                 )
        CL_LOAD_FUNCTION(clEnqueueWaitForEvents          )
        CL_LOAD_FUNCTION(clEnqueueBarrier                )
        CL_LOAD_FUNCTION(clGetExtensionFunctionAddress   )

        // 初始化OpenCL各项指标
        size_t deviceListSize;
        cl_uint numPlatforms;
        cl_platform_id platform = NULL;

        status = clGetPlatformIDs(0,NULL,&numPlatforms);
        if (CL_SUCCESS != status)
        {
            printf("");
            return FALSE;
        }
        if (numPlatforms > 0)
        {
            cl_platform_id *platforms =(cl_platform_id*)
                malloc(numPlatforms * sizeof(cl_platform_id));
            status = clGetPlatformIDs(numPlatforms,platforms,NULL);
            if (CL_SUCCESS!=status)
            {
                printf("");
                return 0;
            }
            for (unsigned int i = 0;i<numPlatforms;++i)
            {
            }
            free(platforms);
        }
        cl_context_properties cps[3] = {
            CL_CONTEXT_PLATFORM,(cl_context_properties)platform,0};

        clGetPlatformIDs(1,&cl_status.platform_id, NULL);
    }

    cl_status.is_opencl_available = TRUE;

    return TRUE;
}

#undef CL_LOAD_FUNCTION