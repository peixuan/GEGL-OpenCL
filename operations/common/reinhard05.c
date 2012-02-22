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
 * Copyright 2010 Danny Robson      <danny@blubinc.net>
 * (pfstmo)  2007 Grzegorz Krawczyk <krawczyk@mpi-sb.mpg.de>
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include <math.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"
#include <string.h>



#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (brightness, _("Brightness"),
                  -100.0, 100.0, 0.0,
                  _("Overall brightness of the image"))
gegl_chant_double (chromatic, _("Chromatic Adaptation"),
                  0.0, 1.0, 0.0,
                  _("Adapation to colour variation across the image"))
gegl_chant_double (light, _("Light Adaptation"),
                  0.0, 1.0, 1.0,
                  _("Adapation to light variation across the image"))


#else

#define GEGL_CHANT_TYPE_FILTER
#define GEGL_CHANT_C_FILE       "reinhard05.c"

#include "gegl-chant.h"

static gboolean
reinhard05_cl_process ( GeglOperation       *operation,
					   GeglBuffer          *src,
					   GeglBuffer          *dst,
					   const GeglRectangle *result);

typedef struct {
  gfloat min, max, avg, range;
  guint  num;
} stats;


static const gchar *OUTPUT_FORMAT = "RGBA float";


static void
reinhard05_prepare (GeglOperation *operation)
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
reinhard05_get_required_for_output (GeglOperation       *operation,
                                    const gchar         *input_pad,
                                    const GeglRectangle *roi)
{
  GeglRectangle result = *gegl_operation_source_get_bounding_box (operation,
                                                                  "input");
  return result;
}


static GeglRectangle
reinhard05_get_cached_region (GeglOperation       *operation,
                              const GeglRectangle *roi)
{
  return *gegl_operation_source_get_bounding_box (operation, "input");
}

static void
reinhard05_stats_start (stats *s)
{
  g_return_if_fail (s);

  s->min   = G_MAXFLOAT;
  s->max   = G_MINFLOAT;
  s->avg   = 0.0;
  s->range = NAN;
  s->num   = 0;
};


static void
reinhard05_stats_update (stats *s,
                         gfloat value)
{
  g_return_if_fail (s);
  g_return_if_fail (!isinf (value));
  g_return_if_fail (!isnan (value));

  s->min  = MIN (s->min, value);
  s->max  = MAX (s->max, value);
  s->avg += value;
  s->num += 1;
}


static void
reinhard05_stats_finish (stats *s)
{
  g_return_if_fail (s->num !=    0.0);
  g_return_if_fail (s->max >= s->min);

  s->avg   /= s->num;
  s->range  = s->max - s->min;
}


static gboolean
reinhard05_process (GeglOperation       *operation,
                    GeglBuffer          *input,
                    GeglBuffer          *output,
                    const GeglRectangle *result)
{
  
  if(reinhard05_cl_process(operation,input,output,result))
	  return TRUE;

  const GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);

  const gint  pix_stride = 4, /* RGBA */
              RGB        = 3;

  gfloat *lum,
         *pix;
  gfloat  key, contrast, intensity,
          chrom      =       o->chromatic,
          chrom_comp = 1.0 - o->chromatic,
          light      =       o->light,
          light_comp = 1.0 - o->light;

  stats   world_lin,
          world_log,
          channel [RGB],
          normalise;

  gint    i, c;

  g_return_val_if_fail (operation, FALSE);
  g_return_val_if_fail (input, FALSE);
  g_return_val_if_fail (output, FALSE);
  g_return_val_if_fail (result, FALSE);

  g_return_val_if_fail (babl_format_get_n_components (babl_format (OUTPUT_FORMAT)) == pix_stride, FALSE);

  g_return_val_if_fail (chrom      >= 0.0 && chrom      <= 1.0, FALSE);
  g_return_val_if_fail (chrom_comp >= 0.0 && chrom_comp <= 1.0, FALSE);
  g_return_val_if_fail (light      >= 0.0 && light      <= 1.0, FALSE);
  g_return_val_if_fail (light_comp >= 0.0 && light_comp <= 1.0, FALSE);


  /* Obtain the pixel data */
  lum = g_new (gfloat, result->width * result->height),
  gegl_buffer_get (input, 1.0, result, babl_format ("Y float"),
                   lum, GEGL_AUTO_ROWSTRIDE);

  pix = g_new (gfloat, result->width * result->height * pix_stride);
  gegl_buffer_get (input, 1.0, result, babl_format (OUTPUT_FORMAT),
                   pix, GEGL_AUTO_ROWSTRIDE);

  /* Collect the image stats, averages, etc */
  reinhard05_stats_start (&world_lin);
  reinhard05_stats_start (&world_log);
  reinhard05_stats_start (&normalise);
  for (i = 0; i < RGB; ++i)
    {
      reinhard05_stats_start (channel + i);
    }

  for (i = 0; i < result->width * result->height; ++i)
    {
      reinhard05_stats_update (&world_lin,                 lum[i] );
      reinhard05_stats_update (&world_log, logf (2.3e-5f + lum[i]));

      for (c = 0; c < RGB; ++c)
        {
          reinhard05_stats_update (channel + c, pix[i * pix_stride + c]);
        }
    }
  

  g_return_val_if_fail (world_lin.min >= 0.0, FALSE);

  reinhard05_stats_finish (&world_lin);
  reinhard05_stats_finish (&world_log);


  for (i = 0; i < RGB; ++i)
    {
      reinhard05_stats_finish (channel + i);

    }


  /* Calculate key parameters */
  key       = (logf (world_lin.max) -                 world_log.avg) /
              (logf (world_lin.max) - logf (2.3e-5f + world_lin.min));
  contrast  = 0.3 + 0.7 * powf (key, 1.4);
  intensity = expf (-o->brightness);

  g_return_val_if_fail (contrast >= 0.3 && contrast <= 1.0, FALSE);

  /* Apply the operator */
  for (i = 0; i < result->width * result->height; ++i)
    {
      gfloat local, global, adapt;

      if (lum[i] == 0.0)
        continue;

      for (c = 0; c < RGB; ++c)
        {
          gfloat *_p = pix + i * pix_stride + c,
                   p = *_p;

          local  = chrom      * p +
                   chrom_comp * lum[i];
          global = chrom      * channel[c].avg +
                   chrom_comp * world_lin.avg;
          adapt  = light      * local +
                   light_comp * global;

          p  /= p + powf (intensity * adapt, contrast);
          *_p = p;
          reinhard05_stats_update (&normalise, p);
        }
    }

  /* Normalise the pixel values */
  reinhard05_stats_finish (&normalise);


  for (i = 0; i < result->width * result->height; ++i)
    {
      for (c = 0; c < pix_stride; ++c)
        {
          gfloat *p = pix + i * pix_stride + c;
          *p        = (*p - normalise.min) / normalise.range;
        }
    }

  /* Cleanup and set the output */
  gegl_buffer_set (output, result, babl_format (OUTPUT_FORMAT), pix,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (pix);
  g_free (lum);

  return TRUE;
}

#include "opencl/gegl-cl.h"
#define CL_ERROR {return FALSE;}

static const char* kernel_source =
"__kernel void reinhard05_normalize(__global const float * lum,                 \n"
"								   __global float4 * pix,						\n"
"								   float chrom,float light,						\n"
"								   float4 channel_avg,float world_lin_avg,		\n"
"								   float intensity,float contrast)				\n"
"{																				\n"
"	int gid=get_global_id(0);													\n"
"	float chrom_comp=1.0f-chrom;												\n"
"	float light_comp=1.0f-light;												\n"
"	float4 llocal;																\n"
"	float4 gglobal;																\n"
"	float4 adapt;																\n"
"	float4 temp;																\n"
"	float4 dst=pix[gid];														\n"
"	if(lum[gid]==0.0f){															\n"
"		return;																	\n"
"	}																			\n"
"	llocal.xyz=chrom*dst.xyz+chrom_comp*lum[gid];								\n"
"	gglobal.xyz=chrom*channel_avg.xyz+chrom_comp*world_lin_avg;					\n"
"	adapt.xyz=light*llocal.xyz+light_comp*gglobal.xyz;							\n"
"	adapt.w=1.0f;																\n"
"	temp=(float4)pow(intensity*adapt,contrast);									\n"
"	dst.xyz/=dst.xyz+temp.xyz;													\n"
"	pix[gid]=dst;																\n"
"}																				\n"
"__kernel void reinhard05(__global float4 * src,								\n"
"						 __global float4 * dst,									\n"
"						 float min,float range)									\n"
"{																				\n"
"	int gid=get_global_id(0);													\n"
"	float4 temp=src[gid];														\n"
"	temp=(temp-min)/range;														\n"
"	dst[gid]=temp;																\n"
"}																				\n";

static gegl_cl_run_data * cl_data = NULL;

static gboolean
reinhard05_cl_process ( GeglOperation       *operation,
					   GeglBuffer          *src,
					   GeglBuffer          *dst,
					   const GeglRectangle *result)
{
	const GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);

	const gint  pix_stride = 4, /* RGBA */
		RGB        = 3;
	const gint  pixel_count=result->width*result->height;

	gfloat *lum;

	gfloat  key, contrast, intensity,
		chrom      =       o->chromatic,
		chrom_comp = 1.0 - o->chromatic,
		light      =       o->light,
		light_comp = 1.0 - o->light;

	stats   world_lin,
		world_log,
		channel [RGB],
		normalise;

	gint    i, c;

	g_return_val_if_fail (operation, FALSE);
	g_return_val_if_fail (src, FALSE);
	g_return_val_if_fail (dst, FALSE);
	g_return_val_if_fail (result, FALSE);

	g_return_val_if_fail (babl_format_get_n_components (babl_format (OUTPUT_FORMAT)) == pix_stride, FALSE);

	g_return_val_if_fail (chrom      >= 0.0 && chrom      <= 1.0, FALSE);
	g_return_val_if_fail (chrom_comp >= 0.0 && chrom_comp <= 1.0, FALSE);
	g_return_val_if_fail (light      >= 0.0 && light      <= 1.0, FALSE);
	g_return_val_if_fail (light_comp >= 0.0 && light_comp <= 1.0, FALSE);

	/* Collect the image stats, averages, etc */
	reinhard05_stats_start (&world_lin);
	reinhard05_stats_start (&world_log);
	reinhard05_stats_start (&normalise);
	for (i = 0; i < RGB; ++i)
	{
		reinhard05_stats_start (channel + i);
	}

	//Initiate some necessary data
	const Babl   *src_format = gegl_buffer_get_format(src);
	const Babl   *dst_format = gegl_buffer_get_format(dst);
	const Babl   * in_format = babl_format(OUTPUT_FORMAT);
	const Babl   * out_format= babl_format(OUTPUT_FORMAT);
	const Babl   * lum_format= babl_format("Y float");

	g_printf("[OpenCL] BABL formats: (%s,%s:%d) (%s,%s:%d)\n", babl_get_name(src_format),  babl_get_name(in_format),
		gegl_cl_color_supported (src_format, in_format),
		babl_get_name(out_format),babl_get_name(dst_format),
		gegl_cl_color_supported (out_format,dst_format));

	const size_t bpp_src     = babl_format_get_bytes_per_pixel(src_format);
	const size_t bpp_dst     = babl_format_get_bytes_per_pixel(dst_format);
	const size_t bpp_in      = babl_format_get_bytes_per_pixel( in_format);
	const size_t bpp_out     = babl_format_get_bytes_per_pixel(out_format);
	const size_t bpp_lum     = babl_format_get_bytes_per_pixel(lum_format);

	const size_t size_src    = result->width * result->height * bpp_src;
	const size_t size_dst    = result->width * result->height * bpp_dst;
	const size_t size_in     = result->width * result->height * bpp_in ;
	const size_t size_out    = result->width * result->height * bpp_out;
	const size_t size_lum    = result->width * result->height * bpp_lum;

	gegl_cl_color_op need_babl_in     =
		gegl_cl_color_supported(src_format,  in_format);
	gegl_cl_color_op need_babl_out    =
		gegl_cl_color_supported(out_format, dst_format);

	gfloat *src_buf = NULL;
	gfloat *dst_buf = NULL;
	gfloat *buf1    = NULL;
	gfloat *buf2    = NULL;

	cl_mem src_mem;
	cl_mem dst_mem;
	cl_mem lum_men;

	const size_t gbl_size[1] = {pixel_count};

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

	/************************ <src_format--> Y float> **********************************/
	gegl_cl_color_op need_babl_yfloat=
		gegl_cl_color_supported(src_format,  lum_format);

	if (CL_COLOR_NOT_SUPPORTED == need_babl_yfloat ||
		CL_COLOR_EQUAL         == need_babl_yfloat)
	{
		src_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
			src_mem, CL_TRUE, CL_MAP_WRITE,
			0, size_lum,
			NULL, NULL, NULL,
			&errcode);
		if (CL_SUCCESS != errcode) CL_ERROR;

		gegl_buffer_get(src, 1.0, result,  lum_format, src_buf,
			GEGL_AUTO_ROWSTRIDE);

		errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
			src_mem, src_buf, 
			NULL, NULL, NULL);
		if (CL_SUCCESS != errcode) CL_ERROR;
	}
	else if (CL_COLOR_CONVERT == need_babl_yfloat)
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
			pixel_count,src_format, lum_format);

		errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
		if (CL_SUCCESS != errcode) CL_ERROR;
	}
	//Write the Y float data back to lum_men
	lum_men=gegl_clCreateBuffer(gegl_cl_get_context(),
		CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
		size_lum,NULL, &errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	errcode=gegl_clEnqueueCopyBuffer(gegl_cl_get_command_queue(),
		src_mem,lum_men,0,0,size_lum,
		NULL, NULL, NULL);
	if (CL_SUCCESS != errcode) CL_ERROR;

	lum = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
		lum_men, CL_TRUE, CL_MAP_READ,
		0, size_lum,
		NULL, NULL, NULL,
		&errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	for (i = 0; i < pixel_count; ++i)
	{
		reinhard05_stats_update (&world_lin,                 lum[i] );
		reinhard05_stats_update (&world_log, logf (2.3e-5f + lum[i]));
	}

	errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
		lum_men, lum, 
		NULL, NULL, NULL);
	if (CL_SUCCESS != errcode) CL_ERROR;

	g_return_val_if_fail (world_lin.min >= 0.0, FALSE);
	reinhard05_stats_finish (&world_lin);
	reinhard05_stats_finish (&world_log);

	/**********************************************************/

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
			pixel_count,src_format, in_format);

		errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
		if (CL_SUCCESS != errcode) CL_ERROR;
	}

	buf1 = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
		src_mem, CL_TRUE, CL_MAP_READ,
		0, size_in,
		0, NULL, NULL,
		&errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	for (i = 0; i < pixel_count; ++i)
	{
		for (c = 0; c < RGB; ++c)
		{
			reinhard05_stats_update (channel + c, buf1[i * pix_stride + c]);
		}
	}

	errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
		src_mem, buf1, 
		0, NULL, NULL);
	if (CL_SUCCESS != errcode) CL_ERROR;

	for (i = 0; i < RGB; ++i)
	{
		reinhard05_stats_finish (channel+i);
	}

	/* Calculate key parameters */
	key       = (logf (world_lin.max) -                 world_log.avg) /
		(logf (world_lin.max) - logf (2.3e-5f + world_lin.min));
	contrast  = 0.3 + 0.7 * powf (key, 1.4);
	intensity = expf (-o->brightness);

	g_return_val_if_fail (contrast >= 0.3 && contrast <= 1.0, FALSE);

	////////////////////////////////////////////////////////////
	/*Execute the reinhard05_normalize kernel in order to compute the variable "normalize" */

	if (!cl_data)
	{
		const char *kernel_name[] = {"reinhard05_normalize","reinhard05", NULL};
		cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
	}
	if (!cl_data) CL_ERROR;

	cl_float4 channel_avg={channel[0].avg,channel[1].avg,channel[2].avg,1.0f};
	cl_float world_lin_avg=world_lin.avg;

	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&lum_men));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 1, sizeof(cl_mem), (void*)&src_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 2, sizeof(cl_float), (void*)&chrom));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 3, sizeof(cl_float), (void*)&light));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 4, sizeof(cl_float4), (void*)&channel_avg));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 5, sizeof(cl_float), (void*)&world_lin_avg));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 6, sizeof(cl_float), (void*)&intensity));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[0], 7, sizeof(cl_float), (void*)&contrast));

	CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
		gegl_cl_get_command_queue(), cl_data->kernel[0],
		1, NULL,
		gbl_size, NULL,
		0, NULL, NULL));

	errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
	if (CL_SUCCESS != errcode) CL_ERROR;


	////////////////////////////////////////////////////////////

	//Calculate the  normalise

	buf2 = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
		src_mem, CL_TRUE, CL_MAP_READ,
		0, size_in,
		0, NULL, NULL,
		&errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	lum = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
		lum_men, CL_TRUE, CL_MAP_READ,
		0, size_lum,
		NULL, NULL, NULL,
		&errcode);
	if (CL_SUCCESS != errcode) CL_ERROR;

	for (i = 0; i < pixel_count; ++i){
		if(lum[i]==0.0)
			continue;
		for(c=0;c<RGB;c++)			
			reinhard05_stats_update (&normalise, buf2[i*pix_stride + c]);		
	}

	errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
		lum_men, lum, 
		NULL, NULL, NULL);
	if (CL_SUCCESS != errcode) CL_ERROR;

	errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
		src_mem, buf2, 
		0, NULL, NULL);
	if (CL_SUCCESS != errcode) CL_ERROR;

	/* Normalise the pixel values */
	reinhard05_stats_finish (&normalise);

	////////////////////////////////////////////////////////////
	/*Execute the reinhard05 kernel */
	if (!cl_data)
	{
		const char *kernel_name[] = {"reinhard05_normalize","reinhard05", NULL};
		cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
	}
	if (!cl_data) CL_ERROR;

	cl_float normalise_min=normalise.min;
	cl_float normalise_range=normalise.range;

	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[1], 0, sizeof(cl_mem), (void*)&src_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[1], 1, sizeof(cl_mem), (void*)&dst_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[1], 2, sizeof(cl_float), (void*)&normalise_min));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[1], 3, sizeof(cl_float), (void*)&normalise_range));

	CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
		gegl_cl_get_command_queue(), cl_data->kernel[1],
		1, NULL,
		gbl_size, NULL,
		0, NULL, NULL));

	errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
	if (CL_SUCCESS != errcode) CL_ERROR;

	////////////////////////////////////////////////////////////

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

	if(src_mem)     gegl_clReleaseMemObject(src_mem);
	if(dst_mem)     gegl_clReleaseMemObject(dst_mem);	
	if(lum_men)     gegl_clReleaseMemObject(lum_men);	
	return TRUE;
}
/*
 */
static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process = reinhard05_process;
  //filter_class->process = reinhard05_process;
  
  operation_class->opencl_support = TRUE;
  operation_class->prepare                 = reinhard05_prepare;
  operation_class->get_required_for_output = reinhard05_get_required_for_output;
  operation_class->get_cached_region       = reinhard05_get_cached_region;

  operation_class->name        = "gegl:reinhard05";
  operation_class->categories  = "tonemapping";
  operation_class->description =
        _("Adapt an image, which may have a high dynamic range, for "
	  "presentation using a low dynamic range. This is an efficient "
          "global operator derived from simple physiological observations, "
          "producing luminance within the range 0.0-1.0");
}

#endif

