/* This file is part of GEGL
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
 * Copyright 2006 Øyvind Kolås
 */


#include "config.h"

#include <glib-object.h>
#include <math.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "gegl-operation-point-filter.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"
#include <string.h>

#include "gegl-buffer-private.h"
#include "gegl-tile-storage.h"

#include "opencl/gegl-cl.h"

static gboolean gegl_operation_point_filter_process
                              (GeglOperation       *operation,
                               GeglBuffer          *input,
                               GeglBuffer          *output,
                               const GeglRectangle *result);

static gboolean gegl_operation_point_filter_op_process
                              (GeglOperation       *operation,
                               GeglOperationContext *context,
                               const gchar          *output_pad,
                               const GeglRectangle  *roi);

G_DEFINE_TYPE (GeglOperationPointFilter, gegl_operation_point_filter, GEGL_TYPE_OPERATION_FILTER)

static void prepare (GeglOperation *operation)
{
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static void
gegl_operation_point_filter_class_init (GeglOperationPointFilterClass *klass)
{
  GeglOperationClass *operation_class = GEGL_OPERATION_CLASS (klass);

  operation_class->process = gegl_operation_point_filter_op_process;
  operation_class->prepare = prepare;
  operation_class->no_cache = TRUE;

  klass->process = NULL;
  klass->cl_process = NULL;
//  klass->cl_kernel_source     = NULL;
//  klass->cl_kernel_parameters = NULL;
}

static void
gegl_operation_point_filter_init (GeglOperationPointFilter *self)
{
}

struct buf_tex
{
  GeglBuffer *buf;
  GeglRectangle *region;
  cl_mem *tex;
};

#define CL_ERROR {g_assert(0);}
//#define CL_ERROR {goto error;}

#include "opencl/gegl-cl-color-kernel.h"
#include "opencl/gegl-cl-color.h"

//Three memory 
static gboolean
gegl_operation_point_filter_cl_process_full (GeglOperation       *operation,
                                             GeglBuffer          *input,
                                             GeglBuffer          *output,
                                             const GeglRectangle *result)
{

  const Babl *in_format  = gegl_operation_get_format (operation, "input");
  const Babl *out_format = gegl_operation_get_format (operation, "output");
  const Babl *input_format=gegl_buffer_get_format(input);
  const Babl *output_format=gegl_buffer_get_format(output);

  const size_t bpp_src     = babl_format_get_bytes_per_pixel(input_format);
  const size_t bpp_dst     = babl_format_get_bytes_per_pixel(output_format);
  const size_t bpp_in      = babl_format_get_bytes_per_pixel( in_format);
  const size_t bpp_out     = babl_format_get_bytes_per_pixel(out_format);
  const size_t bpp_rgbaf   = babl_format_get_bytes_per_pixel(babl_format ("RGBA float"));

  size_t size_src    ;
  size_t size_dst    ;
  size_t size_in     ;
  size_t size_out    ;
  size_t alloc_real_size   ;
  int transfer_twice = FALSE;  

  GeglOperationPointFilterClass *point_filter_class = GEGL_OPERATION_POINT_FILTER_GET_CLASS (operation);
  int y, x, i,j;
  int errcode;

  gfloat** in_data  = NULL;
  gfloat** out_data = NULL;

  int ntex = 0;
  const size_t interval=1;
  cl_mem * aux;

  struct buf_tex input_tex;
  struct buf_tex output_tex;

  gegl_cl_color_op need_babl_in= gegl_cl_color_supported(input_format,in_format);
  gegl_cl_color_op need_babl_out = gegl_cl_color_supported(out_format,output_format);
  gegl_cl_color_op need_in_out_convert=gegl_cl_color_supported(in_format,out_format);

  g_printf("[OpenCL] BABL formats: (%s,%s:%d) (%s,%s:%d)\n", babl_get_name(input_format),  babl_get_name(in_format),
                                                             gegl_cl_color_supported (input_format, in_format),
                                                             babl_get_name(out_format),babl_get_name(output_format),
                                                             gegl_cl_color_supported (out_format, output_format));


  for (y=0; y < result->height; y += cl_status.max_height)
	  for (x=0; x < result->width;  x += cl_status.max_width)
		  ntex++;

  input_tex.region  = (GeglRectangle *) gegl_malloc(interval * sizeof(GeglRectangle));
  output_tex.region = (GeglRectangle *) gegl_malloc(interval * sizeof(GeglRectangle));
  input_tex.tex     = (cl_mem *)        gegl_malloc(interval * sizeof(cl_mem));
  output_tex.tex    = (cl_mem *)        gegl_malloc(interval * sizeof(cl_mem));
  aux=                (cl_mem *)        gegl_malloc(interval * sizeof(cl_mem));

  if (input_tex.region == NULL || output_tex.region == NULL || input_tex.tex == NULL || output_tex.tex == NULL)
    CL_ERROR;

  in_data  = (gfloat**) gegl_malloc(interval * sizeof(gfloat *));
  out_data = (gfloat**) gegl_malloc(interval * sizeof(gfloat *));

  if (in_data == NULL || out_data == NULL) CL_ERROR;

/*
  Compare input_format with in_format.
  1. if input_format is equal to in_format,
     get the original data without changes directly;
  2. if input_format isn't equal to in_format and can be supported by gegl-cl-color-kernel:
     now the conditions will be considered:
     2.1.if the bytes per pixel is equal,we divide two cl_men as input_men and output_men for color conversion 
         and then change their contents , then do the operation by using the two cl_men.
     2.2.else,we divide three cl_men:aux[i]-->input_tex.tex[i]-->output_tex.tex[i]-->aux[i]
  3. if input_format isn't equal to in_format and can't also be supported by gegl-cl-color-kernel:
     we have to process color conversion by BABL.
*/

  i = 0;
  j = 0;

  for (y=0; y < result->height; y += cl_status.max_height)
	  for (x=0; x < result->width;  x += cl_status.max_width)
	  {

		j=i%interval;
		const size_t region[2] = {MIN(cl_status.max_width,  result->width -x),
								  MIN(cl_status.max_height, result->height-y)};        
		size_t bpp;
		size_t mem_size;
        GeglRectangle r = {x+result->x, y+result->y, region[0], region[1]};
        input_tex.region[j] = output_tex.region[j] = r;    

		size_src = region[0]*region[1]*bpp_src;
		size_in  = region[0]*region[1]*bpp_in;
		size_dst = region[0]*region[1]*bpp_dst;
		size_out = region[0]*region[1]*bpp_out;

		alloc_real_size = MAX(MAX(size_src,size_dst),MAX(size_in,size_out));
		//Enough space for RGBA float in the color conversion 
		if(need_babl_in==CL_COLOR_CONVERT || need_in_out_convert==CL_COLOR_CONVERT || need_babl_out==CL_COLOR_CONVERT)
			alloc_real_size = MAX(alloc_real_size,region[0]*region[1]*bpp_rgbaf);

		input_tex.tex[j] = gegl_clCreateBuffer(gegl_cl_get_context(),
			CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
			alloc_real_size,
			NULL, &errcode);
		if (CL_SUCCESS != errcode) CL_ERROR;

		if(input_format!=babl_format("RGBA float") && in_format!=babl_format("RGBA float"))
			transfer_twice = TRUE;

		if(need_babl_in==CL_COLOR_CONVERT && transfer_twice == FALSE){

			output_tex.tex[j] = gegl_clCreateBuffer(gegl_cl_get_context(),
				CL_MEM_READ_WRITE,alloc_real_size,
				NULL, &errcode);
			if (CL_SUCCESS != errcode) CL_ERROR;
		}
		else{
			output_tex.tex[j] = gegl_clCreateBuffer(gegl_cl_get_context(),
				CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
				alloc_real_size,
				NULL, &errcode);
			if (CL_SUCCESS != errcode) CL_ERROR;
		}
		
		if(need_babl_in==CL_COLOR_NOT_SUPPORTED||need_babl_in==CL_COLOR_EQUAL){			
			/* pre-pinned memory */
			in_data[j]=gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(), input_tex.tex[j], CL_TRUE,
				CL_MAP_WRITE,0, size_in, 0, NULL, NULL, &errcode);
			if (errcode != CL_SUCCESS) CL_ERROR;			
			/* un-tile */
            gegl_buffer_get (input, 1.0, &input_tex.region[j], in_format, 
	                         in_data[j], GEGL_AUTO_ROWSTRIDE);

			errcode = gegl_clEnqueueUnmapMemObject (gegl_cl_get_command_queue(), input_tex.tex[j], 
				in_data[j],0, NULL, NULL);
			if (errcode != CL_SUCCESS) CL_ERROR;				
		}	
		//call the gegl-cl-color-kernel
		else if(need_babl_in==CL_COLOR_CONVERT){
			/* pre-pinned memory */
			in_data[j]=gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(), input_tex.tex[j], CL_TRUE,
				CL_MAP_WRITE,0, size_src, 0, NULL, NULL, &errcode);
			if (errcode != CL_SUCCESS) CL_ERROR;			
			/* un-tile */
			gegl_buffer_get (input, 1.0, &input_tex.region[j], input_format, in_data[j], GEGL_AUTO_ROWSTRIDE);

			errcode = gegl_clEnqueueUnmapMemObject (gegl_cl_get_command_queue(), input_tex.tex[j], 
				in_data[j],0, NULL, NULL);
			if (errcode != CL_SUCCESS) CL_ERROR;

			aux[j]=input_tex.tex[j];

			input_tex.tex[j]= gegl_clCreateBuffer (gegl_cl_get_context(),
				CL_MEM_READ_WRITE, alloc_real_size,
				NULL, &errcode);
			if (errcode != CL_SUCCESS) CL_ERROR;		

			gegl_cl_color_conv(&aux[j],&input_tex.tex[j],0,region[0]*region[1],
				input_format,in_format);
		}
		//call the operation-kernel
		if(j==interval-1){
			/* Wait unfinished Processing */
			errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
			if (errcode != CL_SUCCESS) CL_ERROR;
			/* Process */
			for(j=0;j<interval;j++){
				const size_t region[2]= {input_tex.region[j].width, input_tex.region[j].height};
				const size_t global_worksize[1] = {region[0] * region[1]};
				errcode = point_filter_class->cl_process(operation, input_tex.tex[j], output_tex.tex[j], global_worksize, &input_tex.region[j]);
				if (errcode != CL_SUCCESS) CL_ERROR;
			}
			/* Wait Processing */
			errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
			if (errcode != CL_SUCCESS) CL_ERROR;

            /* GPU -> CPU */
			for(j=0;j<interval;j++){

				const size_t region[2] = {output_tex.region[j].width, output_tex.region[j].height};
				size_in  = region[0] * region[1] * bpp_in;
				size_out = region[0] * region[1] * bpp_out;
				size_dst = region[0] * region[1] * bpp_dst;

				if(need_in_out_convert==CL_COLOR_NOT_SUPPORTED||
					(need_in_out_convert==CL_COLOR_EQUAL && 
					(need_babl_out ==CL_COLOR_NOT_SUPPORTED|| need_babl_out==CL_COLOR_EQUAL))){
					//no color conversion in color-conversion-kernel from in_format to out_format
					out_data[j] = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(), output_tex.tex[j], CL_TRUE,
						CL_MAP_READ,0, size_in, 0, NULL, NULL, &errcode);
					if (errcode != CL_SUCCESS) CL_ERROR;

					gegl_buffer_set (output, &output_tex.region[j],in_format, out_data[j], GEGL_AUTO_ROWSTRIDE);

					errcode = gegl_clEnqueueUnmapMemObject (gegl_cl_get_command_queue(), output_tex.tex[j], 
						out_data[j],0, NULL, NULL);
					if (errcode != CL_SUCCESS) CL_ERROR;
				}
				else if(need_in_out_convert==CL_COLOR_CONVERT){
					Babl * final_output_format = out_format;
					size_t final_size_dst      = size_out;
					//We can merge the two color conversion into once call
					if(need_babl_out==CL_COLOR_CONVERT){
						final_output_format = output_format;
						final_size_dst      = size_dst;
					}	
					gegl_cl_color_conv(&output_tex.tex[j],&aux[j],1,region[0]*region[1],
						in_format,final_output_format);

					out_data[j] = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(), output_tex.tex[j], CL_TRUE,
						CL_MAP_READ,0, final_size_dst, 0, NULL, NULL, &errcode);
					if (errcode != CL_SUCCESS) CL_ERROR;

					gegl_buffer_set (output, &output_tex.region[j], 
						final_output_format, out_data[j], GEGL_AUTO_ROWSTRIDE);					
				}
			}
			
			/* Wait */
			errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
			if (errcode != CL_SUCCESS) CL_ERROR;
			/* Run! */
			errcode = gegl_clFinish(gegl_cl_get_command_queue());
			if (errcode != CL_SUCCESS) CL_ERROR;
			for (j=0; j<interval; j++)
			{			
				if(aux[j])                     gegl_clReleaseMemObject (aux[j]);
				if(input_tex.tex[j])           gegl_clReleaseMemObject (input_tex.tex[j]);
				if(output_tex.tex[j])          gegl_clReleaseMemObject (output_tex.tex[j]);
				
			}
			j=0;
		}       
        i++;
      }
  if (input_tex.tex)     gegl_free(input_tex.tex);
  if (output_tex.tex)    gegl_free(output_tex.tex);
  if (input_tex.region)  gegl_free(input_tex.region);
  if (output_tex.region) gegl_free(output_tex.region);
  if(aux)                gegl_free(aux);
  if(in_data)            gegl_free(in_data);
  if(out_data)           gegl_free(out_data);
  return TRUE;

error:
  g_warning("[OpenCL] Error: %s", gegl_cl_errstring(errcode));

  for (j=0; j < interval; j++)
  {
	if (aux[j])  gegl_clReleaseMemObject (aux[j]);
	if (input_tex.tex[j])  gegl_clReleaseMemObject (input_tex.tex[j]);
	if (output_tex.tex[j]) gegl_clReleaseMemObject (output_tex.tex[j]);
  }
  if (input_tex.tex)     gegl_free(input_tex.tex);
  if (output_tex.tex)    gegl_free(output_tex.tex);
  if (input_tex.region)  gegl_free(input_tex.region);
  if (output_tex.region) gegl_free(output_tex.region);
  if(aux)                gegl_free(aux);
  if(in_data)            gegl_free(in_data);
  if(out_data)           gegl_free(out_data);
  return FALSE;

}

#undef CL_ERROR

static gboolean
gegl_operation_point_filter_process (GeglOperation       *operation,
                                     GeglBuffer          *input,
                                     GeglBuffer          *output,
                                     const GeglRectangle *result)
{
  const Babl *in_format  = gegl_operation_get_format (operation, "input");
  const Babl *out_format = gegl_operation_get_format (operation, "output");
  GeglOperationPointFilterClass *point_filter_class;

  point_filter_class = GEGL_OPERATION_POINT_FILTER_GET_CLASS (operation);

  if ((result->width > 0) && (result->height > 0))
    {
      if (cl_status.is_opencl_available && point_filter_class->cl_process)
        {
          if (gegl_operation_point_filter_cl_process_full (operation, input, output, result))
            return TRUE;
        }

      {
        GeglBufferIterator *i = gegl_buffer_iterator_new (output, result, out_format, GEGL_BUFFER_WRITE);
        gint read = /*output == input ? 0 :*/ gegl_buffer_iterator_add (i, input,  result, in_format, GEGL_BUFFER_READ);
        /* using separate read and write iterators for in-place ideally a single
         * readwrite indice would be sufficient
         */
          while (gegl_buffer_iterator_next (i))
            point_filter_class->process (operation, i->data[read], i->data[0], i->length, &i->roi[0]);
      }
    }
  return TRUE;
}

gboolean gegl_can_do_inplace_processing (GeglOperation       *operation,
                                         GeglBuffer          *input,
                                         const GeglRectangle *result);

gboolean gegl_can_do_inplace_processing (GeglOperation       *operation,
                                         GeglBuffer          *input,
                                         const GeglRectangle *result)
{
  if (!input ||
      GEGL_IS_CACHE (input))
    return FALSE;
  if (gegl_object_get_has_forked (input))
    return FALSE;

  if (input->format == gegl_operation_get_format (operation, "output") &&
      gegl_rectangle_contains (gegl_buffer_get_extent (input), result))
    return TRUE;
  return FALSE;
}


static gboolean gegl_operation_point_filter_op_process
                              (GeglOperation       *operation,
                               GeglOperationContext *context,
                               const gchar          *output_pad,
                               const GeglRectangle  *roi)
{
  GeglBuffer               *input;
  GeglBuffer               *output;
  gboolean                  success = FALSE;

  input = gegl_operation_context_get_source (context, "input");

  if (gegl_can_do_inplace_processing (operation, input, roi))
    {
      output = g_object_ref (input);
      gegl_operation_context_take_object (context, "output", G_OBJECT (output));
    }
  else
    {
      output = gegl_operation_context_get_target (context, "output");
    }

  success = gegl_operation_point_filter_process (operation, input, output, roi);
  if (output == GEGL_BUFFER (operation->node->cache))
    gegl_cache_computed (operation->node->cache, roi);

  if (input != NULL)
    g_object_unref (input);
  return success;
}
