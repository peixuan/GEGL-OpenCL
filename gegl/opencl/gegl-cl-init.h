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

#ifndef __DYNAMIC_LOADING_CL_H__
#define __DYNAMIC_LOADING_CL_H__

#include <glib-object.h>
#include <cl/opencl.h>

#define CL_SAFE_CALL(func)                                          \
	func;                                                               \
	if (errcode != CL_SUCCESS)                                          \
{                                                                   \
	g_warning("OpenCL error in %s, Line %u in file %s\nError:%s",     \
#func, __LINE__, __FILE__, gegl_cl_errstring(errcode)); \
}

//#define CL_ERROR {return;}
#define CL_ERROR {g_assert(0);}
//#define CL_ERROR {goto error;}

#if defined(_WIN32)
#define CL_API_ENTRY
#define CL_API_CALL __stdcall
#else
#define CL_API_ENTRY
#define CL_API_CALL
#endif

typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetPlatformIDs                )(cl_uint, cl_platform_id*, cl_uint*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetPlatformInfo               )(cl_platform_id, cl_platform_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetDeviceIDs                  )(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetDeviceInfo                 )(cl_device_id, cl_device_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_context       (CL_API_CALL* h_clCreateContext                 )(const cl_context_properties*, cl_uint, const cl_device_id*, void (CL_CALLBACK*)(const char*, const void*, size_t, void*), void*, cl_int*);
typedef CL_API_ENTRY cl_context       (CL_API_CALL* h_clCreateContextFromType         )(const cl_context_properties*, cl_device_type, void (CL_CALLBACK*)(const char*, const void*, size_t, void*), void*, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainContext                 )(cl_context);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseContext                )(cl_context);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetContextInfo                )(cl_context, cl_context_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_command_queue (CL_API_CALL* h_clCreateCommandQueue            )(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainCommandQueue            )(cl_command_queue);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseCommandQueue           )(cl_command_queue);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetCommandQueueInfo           )(cl_command_queue, cl_command_queue_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clSetCommandQueueProperty       )(cl_command_queue, cl_command_queue_properties, cl_bool, cl_command_queue_properties*);
typedef CL_API_ENTRY cl_mem           (CL_API_CALL* h_clCreateBuffer                  )(cl_context, cl_mem_flags, size_t, void*, cl_int*);
typedef CL_API_ENTRY cl_mem           (CL_API_CALL* h_clCreateSubBuffer               )(cl_mem, cl_mem_flags, cl_buffer_create_type, const void*, cl_int*);
typedef CL_API_ENTRY cl_mem           (CL_API_CALL* h_clCreateImage2D                 )(cl_context, cl_mem_flags, const cl_image_format*, size_t, size_t, size_t, void*, cl_int*);
typedef CL_API_ENTRY cl_mem           (CL_API_CALL* h_clCreateImage3D                 )(cl_context, cl_mem_flags, const cl_image_format*, size_t, size_t, size_t, size_t, size_t, void*, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainMemObject               )(cl_mem);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseMemObject              )(cl_mem);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetSupportedImageFormats      )(cl_context, cl_mem_flags, cl_mem_object_type, cl_uint, cl_image_format*, cl_uint*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetMemObjectInfo              )(cl_mem, cl_mem_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetImageInfo                  )(cl_mem, cl_image_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clSetMemObjectDestructorCallback)(cl_mem, void (CL_CALLBACK* ), void*);
typedef CL_API_ENTRY cl_sampler       (CL_API_CALL* h_clCreateSampler                 )(cl_context, cl_bool, cl_addressing_mode, cl_filter_mode, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainSampler                 )(cl_sampler);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseSampler                )(cl_sampler);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetSamplerInfo                )(cl_sampler, cl_sampler_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_program       (CL_API_CALL* h_clCreateProgramWithSource       )(cl_context, cl_uint, const char**, const size_t*, cl_int*);
typedef CL_API_ENTRY cl_program       (CL_API_CALL* h_clCreateProgramWithBinary       )(cl_context, cl_uint, const cl_device_id*, const size_t*, const unsigned char**, cl_int*, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainProgram                 )(cl_program);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseProgram                )(cl_program);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clBuildProgram                  )(cl_program,cl_uint, const cl_device_id*, const char*, void (CL_CALLBACK*), void*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clUnloadCompiler                )(void);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetProgramInfo                )(cl_program,cl_program_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetProgramBuildInfo           )(cl_program, cl_device_id, cl_program_build_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_kernel        (CL_API_CALL* h_clCreateKernel                  )(cl_program, const char*, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clCreateKernelsInProgram        )(cl_program, cl_uint, cl_kernel*, cl_uint*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainKernel                  )(cl_kernel);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseKernel                 )(cl_kernel);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clSetKernelArg                  )(cl_kernel, cl_uint, size_t, const void*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetKernelInfo                 )(cl_kernel, cl_kernel_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetKernelWorkGroupInfo        )(cl_kernel, cl_device_id, cl_kernel_work_group_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clWaitForEvents                 )(cl_uint, const cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetEventInfo                  )(cl_event, cl_event_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_event         (CL_API_CALL* h_clCreateUserEvent               )(cl_context, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clRetainEvent                   )(cl_event);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clReleaseEvent                  )(cl_event);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clSetUserEventStatus            )(cl_event, cl_int);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clSetEventCallback              )(cl_event, cl_int, void (CL_CALLBACK*)(cl_event, cl_int, void*), void*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clGetEventProfilingInfo         )(cl_event, cl_profiling_info, size_t, void*, size_t*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clFlush                         )(cl_command_queue);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clFinish                        )(cl_command_queue);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueReadBuffer             )(cl_command_queue, cl_mem, cl_bool, size_t, size_t, void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueReadBufferRect         )(cl_command_queue, cl_mem, cl_bool, const size_t*, const size_t*, const size_t*, size_t, size_t, size_t, size_t, void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueWriteBuffer            )(cl_command_queue, cl_mem, cl_bool, size_t, size_t, const void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueWriteBufferRect        )(cl_command_queue, cl_mem, cl_bool, const size_t*, const size_t*, const size_t*, size_t, size_t, size_t, size_t, const void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueCopyBuffer             )(cl_command_queue, cl_mem, cl_mem, size_t, size_t, size_t, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueCopyBufferRect         )(cl_command_queue, cl_mem, cl_mem, const size_t*, const size_t*, const size_t*, size_t, size_t, size_t, size_t, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueReadImage              )(cl_command_queue, cl_mem, cl_bool, const size_t*, const size_t*, size_t, size_t, void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueWriteImage             )(cl_command_queue, cl_mem, cl_bool, const size_t*, const size_t*, size_t, size_t, const void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueCopyImage              )(cl_command_queue, cl_mem, cl_mem, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueCopyImageToBuffer      )(cl_command_queue, cl_mem, cl_mem, const size_t*, const size_t*, size_t, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueCopyBufferToImage      )(cl_command_queue, cl_mem, cl_mem, size_t, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY void*            (CL_API_CALL* h_clEnqueueMapBuffer              )(cl_command_queue, cl_mem, cl_bool, cl_map_flags, size_t, size_t, cl_uint, const cl_event*, cl_event*, cl_int*);
typedef CL_API_ENTRY void*            (CL_API_CALL* h_clEnqueueMapImage               )(cl_command_queue, cl_mem, cl_bool, cl_map_flags, const size_t*, const size_t*, size_t*, size_t*, cl_uint, const cl_event*, cl_event*, cl_int*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueUnmapMemObject         )(cl_command_queue, cl_mem, void*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueNDRangeKernel          )(cl_command_queue, cl_kernel, cl_uint, const size_t*, const size_t*, const size_t*, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueTask                   )(cl_command_queue, cl_kernel, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueNativeKernel           )(cl_command_queue, void (*user_func)(void*), void*, size_t, cl_uint, const cl_mem*, const void**, cl_uint, const cl_event*, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueMarker                 )(cl_command_queue, cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueWaitForEvents          )(cl_command_queue, cl_uint, const cl_event*);
typedef CL_API_ENTRY cl_int           (CL_API_CALL* h_clEnqueueBarrier                )(cl_command_queue);
typedef CL_API_ENTRY void*            (CL_API_CALL* h_clGetExtensionFunctionAddress   )(const char*);

typedef struct
{
    gboolean         is_opencl_available;
    cl_platform_id   platform_id;
    cl_context       context;
    cl_device_id     device_id;
    cl_command_queue command_queue;
    cl_uint          max_height;
    cl_uint          max_width;
    char             platform_profile[300];
    char             platform_version[300];
    char             platform_name[300];
    char             platform_vendor[300];
    char             platform_extensions[300];
}
gegl_cl_status;

const char *gegl_cl_errstring(cl_int err);

gboolean gegl_cl_init (GError **error);

gboolean gegl_cl_is_accelerated (void);

cl_platform_id gegl_cl_get_platform (void);

cl_device_id gegl_cl_get_device (void);

cl_context gegl_cl_get_context (void);

cl_command_queue gegl_cl_get_command_queue (void);

typedef struct
{
    cl_program program;
    cl_kernel  kernel[];
} gegl_cl_run_data;


GHashTable *cl_program_hash;

#ifdef __DYNAMIC_LOADING_CL_MAIN_C__

gegl_cl_status                     cl_status = {FALSE,NULL,NULL,NULL,NULL,2048,2048,"","","","",""};
GHashTable*                        cl_program_hash;
h_clGetPlatformIDs                 gegl_clGetPlatformIDs                 = NULL;
h_clGetPlatformInfo                gegl_clGetPlatformInfo                = NULL;
h_clGetDeviceIDs                   gegl_clGetDeviceIDs                   = NULL;
h_clGetDeviceInfo                  gegl_clGetDeviceInfo                  = NULL;
h_clCreateContext                  gegl_clCreateContext                  = NULL;
h_clCreateContextFromType          gegl_clCreateContextFromType          = NULL;
h_clRetainContext                  gegl_clRetainContext                  = NULL;
h_clReleaseContext                 gegl_clReleaseContext                 = NULL;
h_clGetContextInfo                 gegl_clGetContextInfo                 = NULL;
h_clCreateCommandQueue             gegl_clCreateCommandQueue             = NULL;
h_clRetainCommandQueue             gegl_clRetainCommandQueue             = NULL;
h_clReleaseCommandQueue            gegl_clReleaseCommandQueue            = NULL;
h_clGetCommandQueueInfo            gegl_clGetCommandQueueInfo            = NULL;
h_clSetCommandQueueProperty        gegl_clSetCommandQueueProperty        = NULL;
h_clCreateBuffer                   gegl_clCreateBuffer                   = NULL;
h_clCreateSubBuffer                gegl_clCreateSubBuffer                = NULL;
h_clCreateImage2D                  gegl_clCreateImage2D                  = NULL;
h_clCreateImage3D                  gegl_clCreateImage3D                  = NULL;
h_clRetainMemObject                gegl_clRetainMemObject                = NULL;
h_clReleaseMemObject               gegl_clReleaseMemObject               = NULL;
h_clGetSupportedImageFormats       gegl_clGetSupportedImageFormats       = NULL;
h_clGetMemObjectInfo               gegl_clGetMemObjectInfo               = NULL;
h_clGetImageInfo                   gegl_clGetImageInfo                   = NULL;
h_clSetMemObjectDestructorCallback gegl_clSetMemObjectDestructorCallback = NULL;
h_clCreateSampler                  gegl_clCreateSampler                  = NULL;
h_clRetainSampler                  gegl_clRetainSampler                  = NULL;
h_clReleaseSampler                 gegl_clReleaseSampler                 = NULL;
h_clGetSamplerInfo                 gegl_clGetSamplerInfo                 = NULL;
h_clCreateProgramWithSource        gegl_clCreateProgramWithSource        = NULL;
h_clCreateProgramWithBinary        gegl_clCreateProgramWithBinary        = NULL;
h_clRetainProgram                  gegl_clRetainProgram                  = NULL;
h_clReleaseProgram                 gegl_clReleaseProgram                 = NULL;
h_clBuildProgram                   gegl_clBuildProgram                   = NULL;
h_clUnloadCompiler                 gegl_clUnloadCompiler                 = NULL;
h_clGetProgramInfo                 gegl_clGetProgramInfo                 = NULL;
h_clGetProgramBuildInfo            gegl_clGetProgramBuildInfo            = NULL;
h_clCreateKernel                   gegl_clCreateKernel                   = NULL;
h_clCreateKernelsInProgram         gegl_clCreateKernelsInProgram         = NULL;
h_clRetainKernel                   gegl_clRetainKernel                   = NULL;
h_clReleaseKernel                  gegl_clReleaseKernel                  = NULL;
h_clSetKernelArg                   gegl_clSetKernelArg                   = NULL;
h_clGetKernelInfo                  gegl_clGetKernelInfo                  = NULL;
h_clGetKernelWorkGroupInfo         gegl_clGetKernelWorkGroupInfo         = NULL;
h_clWaitForEvents                  gegl_clWaitForEvents                  = NULL;
h_clGetEventInfo                   gegl_clGetEventInfo                   = NULL;
h_clCreateUserEvent                gegl_clCreateUserEvent                = NULL;
h_clRetainEvent                    gegl_clRetainEvent                    = NULL;
h_clReleaseEvent                   gegl_clReleaseEvent                   = NULL;
h_clSetUserEventStatus             gegl_clSetUserEventStatus             = NULL;
h_clSetEventCallback               gegl_clSetEventCallback               = NULL;
h_clGetEventProfilingInfo          gegl_clGetEventProfilingInfo          = NULL;
h_clFlush                          gegl_clFlush                          = NULL;
h_clFinish                         gegl_clFinish                         = NULL;
h_clEnqueueReadBuffer              gegl_clEnqueueReadBuffer              = NULL;
h_clEnqueueReadBufferRect          gegl_clEnqueueReadBufferRect          = NULL;
h_clEnqueueWriteBuffer             gegl_clEnqueueWriteBuffer             = NULL;
h_clEnqueueWriteBufferRect         gegl_clEnqueueWriteBufferRect         = NULL;
h_clEnqueueCopyBuffer              gegl_clEnqueueCopyBuffer              = NULL;
h_clEnqueueCopyBufferRect          gegl_clEnqueueCopyBufferRect          = NULL;
h_clEnqueueReadImage               gegl_clEnqueueReadImage               = NULL;
h_clEnqueueWriteImage              gegl_clEnqueueWriteImage              = NULL;
h_clEnqueueCopyImage               gegl_clEnqueueCopyImage               = NULL;
h_clEnqueueCopyImageToBuffer       gegl_clEnqueueCopyImageToBuffer       = NULL;
h_clEnqueueCopyBufferToImage       gegl_clEnqueueCopyBufferToImage       = NULL;
h_clEnqueueMapBuffer               gegl_clEnqueueMapBuffer               = NULL;
h_clEnqueueMapImage                gegl_clEnqueueMapImage                = NULL;
h_clEnqueueUnmapMemObject          gegl_clEnqueueUnmapMemObject          = NULL;
h_clEnqueueNDRangeKernel           gegl_clEnqueueNDRangeKernel           = NULL;
h_clEnqueueTask                    gegl_clEnqueueTask                    = NULL;
h_clEnqueueNativeKernel            gegl_clEnqueueNativeKernel            = NULL;
h_clEnqueueMarker                  gegl_clEnqueueMarker                  = NULL;
h_clEnqueueWaitForEvents           gegl_clEnqueueWaitForEvents           = NULL;
h_clEnqueueBarrier                 gegl_clEnqueueBarrier                 = NULL;
h_clGetExtensionFunctionAddress    gegl_clGetExtensionFunctionAddress    = NULL;

#else

extern GHashTable *cl_program_hash;
extern gegl_cl_status                     cl_status                            ;
extern h_clGetPlatformIDs                 gegl_clGetPlatformIDs                ;
extern h_clGetPlatformInfo                gegl_clGetPlatformInfo               ;
extern h_clGetDeviceIDs                   gegl_clGetDeviceIDs                  ;
extern h_clGetDeviceInfo                  gegl_clGetDeviceInfo                 ;
extern h_clCreateContext                  gegl_clCreateContext                 ;
extern h_clCreateContextFromType          gegl_clCreateContextFromType         ;
extern h_clRetainContext                  gegl_clRetainContext                 ;
extern h_clReleaseContext                 gegl_clReleaseContext                ;
extern h_clGetContextInfo                 gegl_clGetContextInfo                ;
extern h_clCreateCommandQueue             gegl_clCreateCommandQueue            ;
extern h_clRetainCommandQueue             gegl_clRetainCommandQueue            ;
extern h_clReleaseCommandQueue            gegl_clReleaseCommandQueue           ;
extern h_clGetCommandQueueInfo            gegl_clGetCommandQueueInfo           ;
extern h_clSetCommandQueueProperty        gegl_clSetCommandQueueProperty       ;
extern h_clCreateBuffer                   gegl_clCreateBuffer                  ;
extern h_clCreateSubBuffer                gegl_clCreateSubBuffer               ;
extern h_clCreateImage2D                  gegl_clCreateImage2D                 ;
extern h_clCreateImage3D                  gegl_clCreateImage3D                 ;
extern h_clRetainMemObject                gegl_clRetainMemObject               ;
extern h_clReleaseMemObject               gegl_clReleaseMemObject              ;
extern h_clGetSupportedImageFormats       gegl_clGetSupportedImageFormats      ;
extern h_clGetMemObjectInfo               gegl_clGetMemObjectInfo              ;
extern h_clGetImageInfo                   gegl_clGetImageInfo                  ;
extern h_clSetMemObjectDestructorCallback gegl_clSetMemObjectDestructorCallback;
extern h_clCreateSampler                  gegl_clCreateSampler                 ;
extern h_clRetainSampler                  gegl_clRetainSampler                 ;
extern h_clReleaseSampler                 gegl_clReleaseSampler                ;
extern h_clGetSamplerInfo                 gegl_clGetSamplerInfo                ;
extern h_clCreateProgramWithSource        gegl_clCreateProgramWithSource       ;
extern h_clCreateProgramWithBinary        gegl_clCreateProgramWithBinary       ;
extern h_clRetainProgram                  gegl_clRetainProgram                 ;
extern h_clReleaseProgram                 gegl_clReleaseProgram                ;
extern h_clBuildProgram                   gegl_clBuildProgram                  ;
extern h_clUnloadCompiler                 gegl_clUnloadCompiler                ;
extern h_clGetProgramInfo                 gegl_clGetProgramInfo                ;
extern h_clGetProgramBuildInfo            gegl_clGetProgramBuildInfo           ;
extern h_clCreateKernel                   gegl_clCreateKernel                  ;
extern h_clCreateKernelsInProgram         gegl_clCreateKernelsInProgram        ;
extern h_clRetainKernel                   gegl_clRetainKernel                  ;
extern h_clReleaseKernel                  gegl_clReleaseKernel                 ;
extern h_clSetKernelArg                   gegl_clSetKernelArg                  ;
extern h_clGetKernelInfo                  gegl_clGetKernelInfo                 ;
extern h_clGetKernelWorkGroupInfo         gegl_clGetKernelWorkGroupInfo        ;
extern h_clWaitForEvents                  gegl_clWaitForEvents                 ;
extern h_clGetEventInfo                   gegl_clGetEventInfo                  ;
extern h_clCreateUserEvent                gegl_clCreateUserEvent               ;
extern h_clRetainKernel                   gegl_clRetainKernel                  ;
extern h_clReleaseEvent                   gegl_clReleaseEvent                  ;
extern h_clSetUserEventStatus             gegl_clSetUserEventStatus            ;
extern h_clSetEventCallback               gegl_clSetEventCallback              ;
extern h_clGetEventProfilingInfo          gegl_clGetEventProfilingInfo         ;
extern h_clFlush                          gegl_clFlush                         ;
extern h_clFinish                         gegl_clFinish                        ;
extern h_clEnqueueReadBuffer              gegl_clEnqueueReadBuffer             ;
extern h_clEnqueueReadBufferRect          gegl_clEnqueueReadBufferRect         ;
extern h_clEnqueueWriteBuffer             gegl_clEnqueueWriteBuffer            ;
extern h_clEnqueueWriteBufferRect         gegl_clEnqueueWriteBufferRect        ;
extern h_clEnqueueCopyBuffer              gegl_clEnqueueCopyBuffer             ;
extern h_clEnqueueCopyBufferRect          gegl_clEnqueueCopyBufferRect         ;
extern h_clEnqueueReadImage               gegl_clEnqueueReadImage              ;
extern h_clEnqueueWriteImage              gegl_clEnqueueWriteImage             ;
extern h_clEnqueueCopyImage               gegl_clEnqueueCopyImage              ;
extern h_clEnqueueCopyImageToBuffer       gegl_clEnqueueCopyImageToBuffer      ;
extern h_clEnqueueCopyBufferToImage       gegl_clEnqueueCopyBufferToImage      ;
extern h_clEnqueueMapBuffer               gegl_clEnqueueMapBuffer              ;
extern h_clEnqueueMapImage                gegl_clEnqueueMapImage               ;
extern h_clEnqueueUnmapMemObject          gegl_clEnqueueUnmapMemObject         ;
extern h_clEnqueueNDRangeKernel           gegl_clEnqueueNDRangeKernel          ;
extern h_clEnqueueTask                    gegl_clEnqueueTask                   ;
extern h_clEnqueueNativeKernel            gegl_clEnqueueNativeKernel           ;
extern h_clEnqueueMarker                  gegl_clEnqueueMarker                 ;
extern h_clEnqueueWaitForEvents           gegl_clEnqueueWaitForEvents          ;
extern h_clEnqueueBarrier                 gegl_clEnqueueBarrier                ;
extern h_clGetExtensionFunctionAddress    gegl_clGetExtensionFunctionAddress   ;

#endif /* __DYNAMIC_LOADING_CL_MAIN_C__*/

#endif /* __DYNAMIC_LOADING_CL_H__*/