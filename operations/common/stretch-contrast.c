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
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
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

#else

#define GEGL_CHANT_TYPE_FILTER
#define GEGL_CHANT_C_FILE       "stretch-contrast.c"

#include "gegl-chant.h"

static gboolean
cl_process (GeglOperation       *operation,
			GeglBuffer          *src,
			GeglBuffer          *dst,
			const GeglRectangle *result);

static gboolean
inner_process (gdouble  min,
               gdouble  max,
               gfloat  *buf,
               gint     n_pixels)
{
  gint o;

  for (o=0; o<n_pixels; o++)
    {
      buf[0] = (buf[0] - min) / (max-min);
      buf[1] = (buf[1] - min) / (max-min);
      buf[2] = (buf[2] - min) / (max-min);
      /* FIXME: really stretch the alpha channel?? */
      buf[3] = (buf[3] - min) / (max-min);

      buf += 4;
    }
  return TRUE;
}

static void
buffer_get_min_max (GeglBuffer *buffer,
                    gdouble    *min,
                    gdouble    *max)
{
  gfloat tmin = 9000000.0;
  gfloat tmax =-9000000.0;

  gfloat *buf = g_new0 (gfloat, 4 * gegl_buffer_get_pixel_count (buffer));
  gint i;
  gegl_buffer_get (buffer, 1.0, NULL, babl_format ("RGBA float"), buf, GEGL_AUTO_ROWSTRIDE);
  for (i=0;i< gegl_buffer_get_pixel_count (buffer);i++)
    {
      gint component;
      for (component=0; component<3; component++)
        {
          gfloat val = buf[i*4+component];

          if (val<tmin)
            tmin=val;
          if (val>tmax)
            tmax=val;
        }
    }
  g_free (buf);
  if (min)
    *min = tmin;
  if (max)
    *max = tmax;
}

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



static GeglRectangle
get_required_for_output (GeglOperation        *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  GeglRectangle result = *gegl_operation_source_get_bounding_box (operation, "input");
  return result;
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
   if(cl_process (operation,input,output, result))
		 return TRUE;
   gdouble  min, max;
   buffer_get_min_max (input, &min, &max);
   {
    gint row;
    gfloat *buf;
    gint chunk_size=128;
    gint consumed=0;

    buf = g_new0 (gfloat, 4 * result->width  * chunk_size);

    for (row = 0; row < result->height; row = consumed)
      {
        gint chunk = consumed+chunk_size<result->height?chunk_size:result->height-consumed;
        GeglRectangle line;

        line.x = result->x;
        line.y = result->y + row;
        line.width = result->width;
        line.height = chunk;

        gegl_buffer_get (input, 1.0, &line, babl_format ("RGBA float"), buf, GEGL_AUTO_ROWSTRIDE);
        inner_process (min, max, buf, result->width  * chunk);
        gegl_buffer_set (output, &line, babl_format ("RGBA float"), buf,
                         GEGL_AUTO_ROWSTRIDE);
        consumed+=chunk;
      }
     g_free (buf);
   }
  return TRUE;
}

#include "opencl/gegl-cl.h"
#define CL_ERROR {return FALSE;}

static const char* kernel_source =
"__kernel void kernel_StretchContrast(__global float4 * in,     \n"
"                                     __global float4 * out,    \n"
"									  float           min,      \n"
"							          float           max)      \n"     
"{																\n"
"	int gid = get_global_id(0);									\n"
"   float4 in_v = in[gid];										\n"
"	out[gid] = ( in_v - min ) / ( max - min );		            \n"
"}																\n";

static gegl_cl_run_data * cl_data = NULL;


static gboolean
cl_process (GeglOperation       *operation,
			GeglBuffer          *src,
			GeglBuffer          *dst,
			const GeglRectangle *result)
{
	//Initiate some necessary data
	const Babl   *src_format = gegl_buffer_get_format(src);
	const Babl   *dst_format = gegl_buffer_get_format(dst);
	const Babl   * in_format = babl_format("RGBA float");
	const Babl   * out_format= babl_format("RGBA float");

	g_printf("[OpenCL] BABL formats: (%s,%s:%d) (%s,%s:%d)\n", babl_get_name(src_format),  babl_get_name(in_format),
		gegl_cl_color_supported (src_format, in_format),
		babl_get_name(dst_format),babl_get_name(out_format),
		gegl_cl_color_supported (dst_format, out_format));

	const size_t bpp_src     = babl_format_get_bytes_per_pixel(src_format);
	const size_t bpp_dst     = babl_format_get_bytes_per_pixel(dst_format);
	const size_t bpp_in      = babl_format_get_bytes_per_pixel( in_format);
	const size_t bpp_out     = babl_format_get_bytes_per_pixel(out_format);

	const size_t size_src    = result->width * result->height * bpp_src;
	const size_t size_dst    = result->width * result->height * bpp_dst;
	const size_t size_in     = result->width * result->height * bpp_in ;
	const size_t size_out    = result->width * result->height * bpp_out;

	gdouble  min,max;

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
			NULL, size_in,
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
			NULL, size_src,
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

    //get the value of min and max after the data is converted into needed format.
	gfloat * buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
		src_mem, CL_TRUE, CL_MAP_READ,
		0, size_in,
		0, NULL, NULL,
		&errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	{
		gfloat fmin= 9000000.0;
		gfloat fmax=-9000000.0;
		gint i;
		for(i=0; i<result->width * result->height;i++)
		{
			gint component;
			for (component=0; component<3; component++)
			{
				gfloat val = buf[i*4+component];
				if (val<fmin)
					fmin=val;
				if (val>fmax)
					fmax=val;
			}
		}		
		min=fmin;
		max=fmax;
	}
	errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
		src_mem, buf, 
		0, NULL, NULL);
	if (CL_SUCCESS != errcode) CL_ERROR;

	//process the exact operation 

	if (!cl_data)
	{
		const char *kernel_name[] = {"kernel_StretchContrast", NULL};
		cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
	}
	if (!cl_data)  CL_ERROR;

	///////////////////////////////////////////////////////////////////////////
	
	const size_t gbl_size[1] = {result->width*result->height};
	cl_float cl_min = (cl_float)min;
	cl_float cl_max = (cl_float)max;

	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&src_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 1, sizeof(cl_mem), (void*)&dst_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 2, sizeof(cl_float), (void*)&cl_min));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 3, sizeof(cl_float), (void*)&cl_max));

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
			NULL, size_out,
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
			NULL, size_dst,
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

	gegl_clReleaseMemObject(src_mem);
	gegl_clReleaseMemObject(dst_mem);
	return TRUE;
}

/* This is called at the end of the gobject class_init function.
 *
 * Here we override the standard passthrough options for the rect
 * computations.
 */
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
 
  operation_class->get_required_for_output = get_required_for_output;

  operation_class->name        = "gegl:stretch-contrast";
  operation_class->categories  = "color:enhance";
  operation_class->description =
        _("Scales the components of the buffer to be in the 0.0-1.0 range. "
          "This improves images that make poor use of the available contrast "
          "(little contrast, very dark, or very bright images).");
}

#endif
