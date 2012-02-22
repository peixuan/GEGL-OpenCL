/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Mark Probst <mark.probst@gmail.com>
 */


#include "config.h"
#include <glib/gi18n-lib.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"
#include <string.h>


#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (red,   _("Red"),   -10.0, 10.0, 0.5,  _("Amount of red"))
gegl_chant_double (green, _("Green"), -10.0, 10.0, 0.25, _("Amount of green"))
gegl_chant_double (blue,  _("Blue"),  -10.0, 10.0, 0.25, _("Amount of blue"))

#else

#define GEGL_CHANT_TYPE_FILTER
#define GEGL_CHANT_C_FILE      "mono-mixer.c"

#include "gegl-chant.h"

static gboolean
cl_process ( GeglOperation       *operation,
			GeglBuffer          *src,
			GeglBuffer          *dst,
			const GeglRectangle *result);

 /* set the babl format this operation prefers to work on */
static void prepare (GeglOperation *operation)
{
	gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
	//Set the source pixel data format as the output format of current operation
	GeglNode * self;
	GeglPad *pad;
	//default format:RGBA float
	Babl * format=babl_format ("RGBA float");
	//get the source pixel data format
	self=gegl_operation_get_source_node(operation,"input");
	while(self){
		if(strcmp(gegl_node_get_operation(self),"gimp:tilemanager-source")==0){
			format=gegl_operation_get_format(self->operation,"output");
			break;
		}
		self=gegl_operation_get_source_node(self->operation,"input");
	}
	gegl_operation_set_format (operation, "output", format);
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
  if(cl_process(operation,input,output,result))
	  return TRUE;
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
  gfloat      red   = o->red;
  gfloat      green = o->green;
  gfloat      blue  = o->blue;
  gfloat     *in_buf;
  gfloat     *out_buf;

 if ((result->width > 0) && (result->height > 0))
 {
     gint num_pixels = result->width * result->height;
     gint i;
     gfloat *in_pixel, *out_pixel;

     in_buf = g_new (gfloat, 4 * num_pixels);
     out_buf = g_new (gfloat, 2 * num_pixels);

     gegl_buffer_get (input, 1.0, result, babl_format ("RGBA float"), in_buf, GEGL_AUTO_ROWSTRIDE);

     in_pixel = in_buf;
     out_pixel = out_buf;
     for (i = 0; i < num_pixels; ++i)
     {
         out_pixel[0] = in_pixel[0] * red + in_pixel[1] * green + in_pixel[2] * blue;
         out_pixel[1] = in_pixel[3];

         in_pixel += 4;
         out_pixel += 2;
     }

     gegl_buffer_set (output, result, babl_format ("YA float"), out_buf,
                      GEGL_AUTO_ROWSTRIDE);

     g_free (in_buf);
     g_free (out_buf);
 }

 return TRUE;
}

#include "opencl/gegl-cl.h"

#define CL_ERROR {return FALSE;}

static const char* kernel_source =
"__kernel void Mono_mixer_cl(__global const float4 *src_buf,							\n"
"							float4                 color,    /* red green bule 1 */		\n"
"							__global float2       *dst_buf)								\n"
"{																						\n"
"	int gid = get_global_id(0);															\n"
"	float4 tmp = src_buf[gid] * color;													\n"
"	dst_buf[gid].x = tmp.x + tmp.y + tmp.z;												\n"
"	dst_buf[gid].y = tmp.w;																\n"
"}																						\n";

static gegl_cl_run_data * cl_data = NULL;

static gboolean
cl_process ( GeglOperation       *operation,
			 GeglBuffer          *src,
			 GeglBuffer          *dst,
			 const GeglRectangle *result)
{
	//Initiate some necessary data
	const Babl   *src_format = gegl_buffer_get_format(src);
	const Babl   *dst_format = gegl_buffer_get_format(dst);
	const Babl   * in_format = babl_format("RGBA float");
	const Babl   * out_format= babl_format("YA float");

	g_printf("[OpenCL] BABL formats: (%s,%s:%d) (%s,%s:%d)\n", babl_get_name(src_format),  babl_get_name(in_format),
		gegl_cl_color_supported (src_format, in_format),
		babl_get_name(out_format),babl_get_name(dst_format),
		gegl_cl_color_supported (out_format,dst_format));

	const size_t bpp_src     = babl_format_get_bytes_per_pixel(src_format);
	const size_t bpp_dst     = babl_format_get_bytes_per_pixel(dst_format);
	const size_t bpp_in      = babl_format_get_bytes_per_pixel( in_format);
	const size_t bpp_out     = babl_format_get_bytes_per_pixel(out_format);

	const size_t size_src    = result->width * result->height * bpp_src;
	const size_t size_dst    = result->width * result->height * bpp_dst;
	const size_t size_in     = result->width * result->height * bpp_in ;
	const size_t size_out    = result->width * result->height * bpp_out;

	gegl_cl_color_op need_babl_in     =
		gegl_cl_color_supported(src_format,  in_format);
	gegl_cl_color_op need_babl_out    =
		gegl_cl_color_supported(out_format, dst_format);

	gfloat *src_buf = NULL;
	gfloat *dst_buf = NULL;

	cl_mem src_mem;
	cl_mem dst_mem;

	int errcode;

	//get the source data 
	src_mem = gegl_clCreateBuffer(gegl_cl_get_context(),
		CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
		MAX(MAX(size_src,size_dst),MAX(size_in,size_out)),
		NULL, &errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	dst_mem = gegl_clCreateBuffer(gegl_cl_get_context(),
		CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
		MAX(MAX(size_src,size_dst),MAX(size_in,size_out)),
		NULL, &errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	if (CL_COLOR_NOT_SUPPORTED == need_babl_in ||
		CL_COLOR_EQUAL         == need_babl_in)
	{
		src_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
					src_mem, CL_TRUE, CL_MAP_WRITE,
					0, size_in,
					NULL, NULL, NULL,
					&errcode);
		if (CL_SUCCESS != errcode) CL_ERROR;

		gegl_buffer_get(src, 1.0, result,  in_format, src_buf,
			GEGL_AUTO_ROWSTRIDE);

		errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
			src_mem, src_buf, 
			NULL, NULL, NULL);
		if (CL_SUCCESS != errcode) CL_ERROR;
	}
	else if (CL_COLOR_CONVERT == need_babl_in)
	{
		src_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
					src_mem, CL_TRUE, CL_MAP_WRITE,
					0, size_src,
					NULL, NULL, NULL,
					&errcode);
		if (CL_SUCCESS != errcode) CL_ERROR;

		gegl_buffer_get(src, 1.0, result, src_format, src_buf,
			GEGL_AUTO_ROWSTRIDE);

		errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
			src_mem, src_buf, 
			NULL, NULL, NULL);
		if (CL_SUCCESS != errcode) CL_ERROR;

		gegl_cl_color_conv(&src_mem, &dst_mem, 1,
			result->width * result->height,
			src_format, in_format);

		errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
		if (CL_SUCCESS != errcode) CL_ERROR;
	}

///////////////////////////Call the exact kernel to do mono-mixer//////////////////////////
		const size_t gbl_size[1] = {result->width*result->height};
		if (!cl_data)
		{
			const char *kernel_name[] = {"Mono_mixer_cl", NULL};
			cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
		}
		if (!cl_data) CL_ERROR;

		GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
		cl_float4  color={o->red,o->green,o->blue,1.0};
		
		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
			cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&src_mem));
		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
			cl_data->kernel[0], 1, sizeof(cl_float4), (void*)&color));
		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
			cl_data->kernel[0], 2, sizeof(cl_mem), (void*)&dst_mem));
		
		CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
			gegl_cl_get_command_queue(), cl_data->kernel[0],
			1, NULL,
			gbl_size, NULL,
			0, NULL, NULL));

		///////////////////////////////////////////////////////////////////////////
		errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
		if (CL_SUCCESS != errcode) CL_ERROR;
		//Return the pixel data

		if (CL_COLOR_NOT_SUPPORTED == need_babl_out ||
			CL_COLOR_EQUAL         == need_babl_out)
		{
			dst_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
				dst_mem, CL_TRUE, CL_MAP_READ,
				0, size_out,
				NULL, NULL, NULL,
				&errcode);
			if (CL_SUCCESS != errcode) CL_ERROR;

			gegl_buffer_set(dst, result, out_format, dst_buf,
				GEGL_AUTO_ROWSTRIDE);

			errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
				dst_mem, dst_buf, 
				NULL, NULL, NULL);
			if (CL_SUCCESS != errcode) CL_ERROR;
		}
		else if (CL_COLOR_CONVERT == need_babl_out)
		{
			gegl_cl_color_conv(&dst_mem, &src_mem, 1,
				result->width * result->height,
				out_format, dst_format);
			errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
			if (CL_SUCCESS != errcode) CL_ERROR;

			dst_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
				dst_mem, CL_TRUE, CL_MAP_READ,
				0, size_dst,
				NULL, NULL, NULL,
				&errcode);
			if (CL_SUCCESS != errcode) CL_ERROR;

			gegl_buffer_set(dst, result, dst_format, dst_buf,
				GEGL_AUTO_ROWSTRIDE);
			errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
				dst_mem, dst_buf, 
				NULL, NULL, NULL);
			if (CL_SUCCESS != errcode) CL_ERROR;
		}

		if(src_mem)    gegl_clReleaseMemObject(src_mem);
		if(dst_mem)    gegl_clReleaseMemObject(dst_mem);
		return TRUE;
}


static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process = process;
  operation_class->opencl_support = TRUE;
  operation_class->prepare = prepare;

  operation_class->name        = "gegl:mono-mixer";
  operation_class->categories  = "color";
  operation_class->description = _("Monochrome channel mixer");
}

#endif
