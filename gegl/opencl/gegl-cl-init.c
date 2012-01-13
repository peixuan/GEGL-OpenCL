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

#include "gegl-cl-color.h"

////

const char *gegl_cl_errstring(cl_int err) {
	static const char* strings[] =
	{
		/* Error Codes */
		"success"                         /*  0  */
		, "device not found"                /* -1  */
		, "device not available"            /* -2  */
		, "compiler not available"          /* -3  */
		, "mem object allocation failure"   /* -4  */
		, "out of resources"                /* -5  */
		, "out of host memory"              /* -6  */
		, "profiling info not available"    /* -7  */
		, "mem copy overlap"                /* -8  */
		, "image format mismatch"           /* -9  */
		, "image format not supported"      /* -10 */
		, "build program failure"           /* -11 */
		, "map failure"                     /* -12 */
		, ""                                /* -13 */
		, ""                                /* -14 */
		, ""                                /* -15 */
		, ""                                /* -16 */
		, ""                                /* -17 */
		, ""                                /* -18 */
		, ""                                /* -19 */
		, ""                                /* -20 */
		, ""                                /* -21 */
		, ""                                /* -22 */
		, ""                                /* -23 */
		, ""                                /* -24 */
		, ""                                /* -25 */
		, ""                                /* -26 */
		, ""                                /* -27 */
		, ""                                /* -28 */
		, ""                                /* -29 */
		, "invalid value"                   /* -30 */
		, "invalid device type"             /* -31 */
		, "invalid platform"                /* -32 */
		, "invalid device"                  /* -33 */
		, "invalid context"                 /* -34 */
		, "invalid queue properties"        /* -35 */
		, "invalid command queue"           /* -36 */
		, "invalid host ptr"                /* -37 */
		, "invalid mem object"              /* -38 */
		, "invalid image format descriptor" /* -39 */
		, "invalid image size"              /* -40 */
		, "invalid sampler"                 /* -41 */
		, "invalid binary"                  /* -42 */
		, "invalid build options"           /* -43 */
		, "invalid program"                 /* -44 */
		, "invalid program executable"      /* -45 */
		, "invalid kernel name"             /* -46 */
		, "invalid kernel definition"       /* -47 */
		, "invalid kernel"                  /* -48 */
		, "invalid arg index"               /* -49 */
		, "invalid arg value"               /* -50 */
		, "invalid arg size"                /* -51 */
		, "invalid kernel args"             /* -52 */
		, "invalid work dimension"          /* -53 */
		, "invalid work group size"         /* -54 */
		, "invalid work item size"          /* -55 */
		, "invalid global offset"           /* -56 */
		, "invalid event wait list"         /* -57 */
		, "invalid event"                   /* -58 */
		, "invalid operation"               /* -59 */
		, "invalid gl object"               /* -60 */
		, "invalid buffer size"             /* -61 */
		, "invalid mip level"               /* -62 */
		, "invalid global work size"        /* -63 */
	};

	return strings[-err];
}


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

#ifdef G_OS_WIN32
#include <windows.h>

#define CL_LOAD_FUNCTION(func)                                              \
    if ((gegl_##func = (h_##func) GetProcAddress(module, #func)) == NULL)   \
{                                                                           \
    g_set_error (gError, 0, 0, "symbol gegl_##func is NULL");               \
    FreeLibrary(module);                                                    \
    return FALSE;                                                           \
}

#else
#define CL_LOAD_FUNCTION(func)                                                    \
	if (!g_module_symbol (module, #func, (gpointer *)& gegl_##func))                  \
{                                                                               \
	g_set_error (status, 0, 0,                                                     \
	"%s: %s", "libOpenCL.so", g_module_error ());                    \
	if (!g_module_close (module))                                                 \
	g_warning ("%s: %s", "libOpenCL.so", g_module_error ());                    \
	return FALSE;                                                                 \
}                                                                               \
	if (gegl_##func == NULL)                                                          \
{                                                                               \
	g_set_error (status, 0, 0, "symbol gegl_##func is NULL");                      \
	if (!g_module_close (module))                                                 \
	g_warning ("%s: %s", "libOpenCL.so", g_module_error ());                    \
	return FALSE;                                                                 \
}

#endif

gboolean
gegl_cl_init(GError **gError)
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

        /* Initialize OpenCL - Access to available platforms */
        cl_uint num_of_platforms;
        status = gegl_clGetPlatformIDs(0, NULL, &num_of_platforms);
        if (CL_SUCCESS == status && num_of_platforms >0)
        {
            cl_platform_id *platforms = (cl_platform_id*)
                malloc(num_of_platforms * sizeof(cl_platform_id));
            status = gegl_clGetPlatformIDs(num_of_platforms, platforms, NULL);
            if (CL_SUCCESS != status)
            {
                printf("[OpenCL]Error: Calling clGetPlatformsIDs\n");
                return FALSE;
            }
            unsigned int i;
            for (i = 0;i < num_of_platforms;++i)
            {
                status = gegl_clGetPlatformInfo(
                    platforms[i],
                    CL_PLATFORM_VENDOR,
                    sizeof(cl_status.platform_vendor),
                    cl_status.platform_vendor,
                    NULL);
                if (CL_SUCCESS != status)
                {
                    printf("[OpenCL]Error: Calling clGetPlatformInfo\n");
                    return FALSE;
                }
                cl_status.platform_id = platforms[i];
                if (!strcmp(cl_status.platform_vendor,
                    "Advanced Micro Devices, Inc."))
                {
                    break;
                }
            }
            status = gegl_clGetPlatformInfo(
                platforms[i],
                CL_PLATFORM_PROFILE,
                sizeof(cl_status.platform_profile),
                cl_status.platform_profile,
                NULL);
            if (CL_SUCCESS != status)
            {
                printf("[OpenCL]Error: Calling clGetPlatformInfo\n");
                return FALSE;
            }
            status = gegl_clGetPlatformInfo(
                platforms[i],
                CL_PLATFORM_VERSION,
                sizeof(cl_status.platform_version),
                cl_status.platform_version,
                NULL);
            if (CL_SUCCESS != status)
            {
                printf("[OpenCL]Error: Calling clGetPlatformInfo\n");
                return FALSE;
            }
            status = gegl_clGetPlatformInfo(
                platforms[i],
                CL_PLATFORM_NAME,
                sizeof(cl_status.platform_name),
                cl_status.platform_name,
                NULL);
            if (CL_SUCCESS != status)
            {
                printf("[OpenCL]Error: Calling clGetPlatformInfo\n");
                return FALSE;
            }
            status = gegl_clGetPlatformInfo(
                platforms[i],
                CL_PLATFORM_EXTENSIONS,
                sizeof(cl_status.platform_extensions),
                cl_status.platform_extensions,
                NULL);
            if (CL_SUCCESS != status)
            {
                printf("[OpenCL]Error: Calling clGetPlatformInfo\n");
                return FALSE;
            }
            free(platforms);
        }
        else
        {
            printf("[OpenCL]Error: Calling clGetPlatformsIDs\n");
            return FALSE;
        }

        /* Initialize OpenCL - Access to available device */
        cl_int num_of_devices;
        status = gegl_clGetDeviceIDs(
            cl_status.platform_id, CL_DEVICE_TYPE_GPU,
            0, NULL, &num_of_devices);
        if (CL_SUCCESS == status && num_of_devices > 0)
        {
            cl_device_id *devices = (cl_device_id*)
                malloc(num_of_devices * sizeof(cl_device_id));
            status = gegl_clGetDeviceIDs(
                cl_status.platform_id, CL_DEVICE_TYPE_GPU,
                num_of_devices, devices, NULL);
            if (CL_SUCCESS != status)
            {
                printf("[OpenCL]Error: Calling clGetDeviceIDs\n");
                return FALSE;
            }
            unsigned int i;
            for (i = 0;i < num_of_devices; ++i)
            {
                cl_status.device_id = devices[i];
            }
            free(devices);
        }
        else
        {
            printf("[OpenCL]Error: Calling clGetDeviceIDs\n");
            return FALSE;
        }

        /* Initialize OpenCL - Access to available context */
        cl_status.context = gegl_clCreateContext(0, 1,
            &cl_status.device_id, NULL, NULL, &status);
        if (CL_SUCCESS != status)
        {
            printf("[OpenCL]Error: Calling clCreateContext\n");
            return FALSE;
        }

        /* Initialize OpenCL - Access to available command queue */
        cl_status.command_queue = gegl_clCreateCommandQueue(
            cl_status.context, cl_status.device_id, 0, &status);
        if (CL_SUCCESS != status)
        {
            printf("[OpenCL]Error: Calling clCreateCommandQueue\n");
            return FALSE;
        }
    }

    cl_status.is_opencl_available = TRUE;

    if (cl_status.is_opencl_available)
        gegl_cl_color_compile_kernels();

    g_printf("[OpenCL] OK\n");


    return TRUE;
}

#undef CL_LOAD_FUNCTION

/* XXX: same program_source with different kernel_name[], context or device
 *      will retrieve the same key
 */
gegl_cl_run_data *
gegl_cl_compile_and_build (const char *program_source, const char *kernel_name[])
{
  gint errcode;
  gegl_cl_run_data *cl_data = NULL;

  if ((cl_data = (gegl_cl_run_data *)g_hash_table_lookup(cl_program_hash, program_source)) == NULL)
    {
      size_t length = strlen(program_source);

      gint i;
      guint kernel_n = 0;
      while (kernel_name[++kernel_n] != NULL);

      cl_data = (gegl_cl_run_data *) g_malloc(sizeof(gegl_cl_run_data)+sizeof(cl_kernel)*kernel_n);

      CL_SAFE_CALL( cl_data->program = gegl_clCreateProgramWithSource(gegl_cl_get_context(), 1, &program_source,
                                                                      &length, &errcode) );

      errcode = gegl_clBuildProgram(cl_data->program, 0, NULL, NULL, NULL, NULL);
      if (errcode != CL_SUCCESS)
        {
          char buffer[2000];
          CL_SAFE_CALL( errcode = gegl_clGetProgramBuildInfo(cl_data->program,
                                                             gegl_cl_get_device_id(),
                                                             CL_PROGRAM_BUILD_LOG,
                                                             sizeof(buffer), buffer, NULL) );
          g_warning("OpenCL Build Error:%s\n%s", gegl_cl_errstring(errcode), buffer);
          return NULL;
        }
      else
        {
          g_printf("[OpenCL] Compiling successful\n");
        }

      for (i=0; i<kernel_n; i++)
        CL_SAFE_CALL( cl_data->kernel[i] =
                      gegl_clCreateKernel(cl_data->program, kernel_name[i], &errcode) );

      g_hash_table_insert(cl_program_hash, g_strdup (program_source), (void*)cl_data);
    }

  return cl_data;
}