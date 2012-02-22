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
 * Copyright 2010 Alexia Death
 *
 * Based on "Kaleidoscope" GIMP plugin
 * Copyright (C) 1999, 2002 Kelly Martin, updated 2005 by Matthew Plough
 * kelly@gimp.org
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

gegl_chant_double (m_angle, _("Mirror rotation"), 0.0, 180.0, 0.0, _("Rotation applied to the mirrors"))

gegl_chant_double (r_angle, _("Result rotation"), 0.0, 360.0, 0.0, _("Rotation applied to the result"))

gegl_chant_int    (n_segs, _("Mirrors"), 2, 24, 6, _("Number of mirrors to use"))

gegl_chant_double (c_x,  _("X offset"), 0.0, 1.0, 0.5, _("X offset of the result of mirroring"))

gegl_chant_double (c_y,  _("Y offset"), 0.0, 1.0, 0.5, _("Y offset of the result of mirroring"))

gegl_chant_double (o_x, _("Center X"), -1.0, 1.0, 0.0, _("X axis ratio for the center of mirroring"))

gegl_chant_double (o_y, _("Center Y"), -1.0, 1.0, 0.0, _("Y axis ratio for the center of mirroring"))

gegl_chant_double (trim_x, _("Trim X"), 0.0, 0.5, 0.0, _("X axis ratio for trimming mirror expanse"))

gegl_chant_double (trim_y, _("Trim Y"), 0.0, 0.5, 0.0, _("Y axis ratio for trimming mirror expanse"))

gegl_chant_double (input_scale, _("Zoom"), 0.1, 100.0, 100.0, _("Scale factor to make rendering size bigger"))

gegl_chant_double (output_scale, _("Expand"), 0.0, 100.0, 1.0, _("Scale factor to make rendering size bigger"))

gegl_chant_boolean (clip, _("Clip result"), TRUE, _("Clip result to input size"))

gegl_chant_boolean (warp, _("Wrap input"), TRUE, _("Fill full output area"))


#else

#define GEGL_CHANT_TYPE_FILTER
#define GEGL_CHANT_C_FILE       "mirrors.c"

#include "gegl-chant.h"
#include <math.h>

#if 0
#define TRACE       /* Define this to see basic tracing info. */
#endif

#if 0
#define DO_NOT_USE_BUFFER_SAMPLE       /* Define this to disable buffer sample.*/
#endif

static int
calc_undistorted_coords(double wx, double wy,
                        double angle1, double angle2, int nsegs,
                        double cen_x, double cen_y,
                        double off_x, double off_y,
                        double *x, double *y)
{
  double dx, dy;
  double r, ang;

  double awidth = G_PI/nsegs;
  double mult;

  dx = wx - cen_x;
  dy = wy - cen_y;

  r = sqrt(dx*dx+dy*dy);
  if (r == 0.0) {
    *x = wx + off_x;
    *y = wy + off_y;
    return TRUE;
  }

  ang = atan2(dy,dx) - angle1 - angle2;
  if (ang<0.0) ang = 2*G_PI - fmod (fabs (ang), 2*G_PI);

  mult = ceil(ang/awidth) - 1;
  ang = ang - mult*awidth;
  if (((int) mult) % 2 == 1) ang = awidth - ang;
  ang = ang + angle1;

  *x = r*cos(ang) + off_x;
  *y = r*sin(ang) + off_y;

  return TRUE;
} /* calc_undistorted_coords */


/* Apply the actual transform */
/* Apply the actual transform */

static void
apply_mirror (double               mirror_angle,
			  double               result_angle,
			  int                  nsegs,
			  double               cen_x,
			  double               cen_y,
			  double               off_x,
			  double               off_y,
			  double               input_scale,
			  gboolean             clip,
			  gboolean             warp,
			  Babl                *format,
			  GeglBuffer          *src,
			  GeglRectangle       *in_boundary,
			  GeglBuffer          *dst,
			  GeglRectangle       *boundary,
			  const GeglRectangle *roi)
{
	gfloat *dst_buf,*src_buf;
	gint    row, col,ix,iy,spx_pos;
	gdouble cx, cy;

	/* Get src pixels. */

#ifdef TRACE
	g_warning ("> mirror marker1, boundary x:%d, y:%d, w:%d, h:%d, center: (%f, %f) offset: (%f, %f)", boundary->x, boundary->y, boundary->width, boundary->height, cen_x, cen_y, off_x,off_y );
#endif

#ifdef DO_NOT_USE_BUFFER_SAMPLE
	src_buf = g_new0 (gfloat, boundary->width * boundary->height * 4);
	gegl_buffer_get (src, 1.0, boundary, format, src_buf, GEGL_AUTO_ROWSTRIDE);
#endif
	/* Get buffer in which to place dst pixels. */
	dst_buf = g_new0 (gfloat, roi->width * roi->height * 4);

	mirror_angle   = mirror_angle * G_PI / 180;
	result_angle   = result_angle * G_PI / 180;

	for (row = 0; row < roi->height; row++) {
		for (col = 0; col < roi->width; col++) {
			calc_undistorted_coords(roi->x + col + 0.01, roi->y + row - 0.01, mirror_angle, result_angle,
				nsegs,
				cen_x, cen_y,
				off_x * input_scale, off_y * input_scale,
				&cx, &cy);


			/* apply scale*/
			cx = in_boundary->x + (cx - in_boundary->x) / input_scale;
			cy = in_boundary->y + (cy - in_boundary->y) / input_scale;

			/*Warping*/
			if (warp)
			{
				double dx = cx - in_boundary->x;
				double dy = cy - in_boundary->y;

				double width_overrun = ceil ((dx) / (in_boundary->width)) ;
				double height_overrun = ceil ((dy) / (in_boundary->height));

				if (cx <= (in_boundary->x))
				{
					if ( fabs (fmod (width_overrun, 2)) < 1.0)
						cx = in_boundary->x - fmod (dx, in_boundary->width);
					else
						cx = in_boundary->x + in_boundary->width + fmod (dx, in_boundary->width);
				}

				if (cy <= (in_boundary->y))
				{
					if ( fabs (fmod (height_overrun, 2)) < 1.0)
						cy = in_boundary->y + fmod (dy, in_boundary->height);
					else
						cy = in_boundary->y + in_boundary->height - fmod (dy, in_boundary->height);
				}

				if (cx >= (in_boundary->x + in_boundary->width))
				{
					if ( fabs (fmod (width_overrun, 2)) < 1.0)
						cx = in_boundary->x + in_boundary->width - fmod (dx, in_boundary->width);
					else
						cx = in_boundary->x + fmod (dx, in_boundary->width);
				}

				if (cy >= (in_boundary->y + in_boundary->height))
				{
					if ( fabs (fmod (height_overrun, 2)) < 1.0)
						cy = in_boundary->y + in_boundary->height - fmod (dy, in_boundary->height);
					else
						cy = in_boundary->y + fmod (dy, in_boundary->height);
				}
			}
			else /* cliping */
			{
				if (cx < boundary->x)
					cx = 0;
				if (cy < boundary->y)
					cy = 0;

				if (cx >= boundary->width)
					cx = boundary->width - 1;
				if (cy >= boundary->height)
					cy = boundary->height -1;
			}


			/* Top */
#ifdef DO_NOT_USE_BUFFER_SAMPLE
			
			if (cx >= 0.0)
				ix = (int) cx;
			else
				ix = -((int) -cx + 1);

			if (cy >= 0.0)
				iy = (int) cy;
			else
				iy = -((int) -cy + 1);

			spx_pos = (iy * boundary->width + ix) * 4;
#endif



#ifndef DO_NOT_USE_BUFFER_SAMPLE
			gegl_buffer_sample (src, cx, cy, NULL, &dst_buf[(row * roi->width + col) * 4], format, GEGL_SAMPLER_LINEAR);
#endif

#ifdef DO_NOT_USE_BUFFER_SAMPLE
			int dpx_pos=(row*roi->width+col)*4;
			dst_buf[dpx_pos]     = src_buf[spx_pos];
			dst_buf[dpx_pos + 1] = src_buf[spx_pos + 1];
			dst_buf[dpx_pos + 2] = src_buf[spx_pos + 2];
			dst_buf[dpx_pos + 3] = src_buf[spx_pos + 3];
#endif

		} /* for */
	} /* for */

	gegl_buffer_sample_cleanup(src);

	/* Store dst pixels. */
	gegl_buffer_set (dst, roi, format, dst_buf, GEGL_AUTO_ROWSTRIDE);

	gegl_buffer_flush(dst);

	/* Free acquired storage. */
#ifdef DO_NOT_USE_BUFFER_SAMPLE
	g_free (src_buf);
#endif
	g_free (dst_buf);

}

#include "opencl/gegl-cl.h"
static const char* kernel_source = 
"#define G_PI 3.141593f														\n"                                                
"#define TRUE true															\n"
"int calc_undistorted_coords_CL(float  wx,float  wy,						\n"
"							   float  angle1,float  angle2,int    nsegs,	\n"
"							   float  cen_x, float  cen_y,					\n"
"							   float  off_x,float  off_y,					\n"
"							   float *x,float *y)							\n"
"{																			\n"
"	float dx,dy;															\n"
"	float r,ang;															\n"
"	float awidth = G_PI / nsegs;											\n"
"	float mult;																\n"
"	dx = wx - cen_x;														\n"
"	dy = wy - cen_y;														\n"
"	r = sqrt(dx * dx + dy * dy);											\n"
"	if(r == 0.0f)															\n"
"	{																		\n"
"		*x = wx + off_x;													\n"
"		*y = wy + off_y;													\n"
"		return TRUE;														\n"
"	}																		\n"
"	ang = atan2(dy, dx) - angle1 - angle2;									\n"
"	if(ang < 0.0f)															\n"
"		ang = 2*G_PI - fmod(fabs(ang), 2*G_PI);								\n"
"	mult = ceil(ang/awidth) - 1;											\n"
"	ang = ang - mult * awidth;												\n"
"	if(((int)mult) % 2 == 1)												\n"
"		ang = awidth - ang;													\n"
"	ang = ang + angle1;														\n"
"	*x = r*cos(ang) + off_x;												\n"
"	*y = r*sin(ang) + off_y;												\n"
"	return TRUE;															\n"
"}																			\n"
"__kernel void apply_mirror_warp_CL(          float   mirror_angle, float   result_angle,   \n"
"								   int     nsegs,											\n"
"								   float   cen_x, float   cen_y,							\n"
"								   float   off_x,float   off_y,								\n"
"								   float   input_scale,										\n"
"								   const __global float4 *src_buf,							\n"
"								   int4    in_boundary,										\n"
"								   __global float4 *dst_buf,								\n"
"								   int4    boundary,										\n"
"								   int4    roi)												\n"
"{																							\n"
"	int gidx = get_global_id(0);															\n"
"	int gidy = get_global_id(1);															\n"
"	float cx,cy;																			\n"
"	calc_undistorted_coords_CL(roi.x + gidx + 0.01f, roi.y + gidy - 0.01f,					\n"
"		mirror_angle, result_angle, nsegs, cen_x, cen_y,									\n"
"		off_x * input_scale, off_y * input_scale, &cx, &cy);								\n"
"	cx = in_boundary.x + (cx - in_boundary.x)/input_scale;									\n"
"	cy = in_boundary.y + (cy - in_boundary.y)/input_scale;									\n"
"	//start warp																			\n"
"	float dx = cx - in_boundary.x;															\n"
"	float dy = cy - in_boundary.y;															\n"
"	float width_overrun = ceil(dx / in_boundary.z);											\n"						
"	float height_overrun = ceil(dy / in_boundary.w);										\n"
"	float dx_mod = fmod(dx, in_boundary.z);													\n"
"	float dy_mod = fmod(dy, in_boundary.w);													\n"
"	if(cx <= in_boundary.x)																	\n"
"	{																						\n"
"		if(fabs(fmod(width_overrun, 2)) < 1.0f)												\n"
"			cx = in_boundary.x - dx_mod;													\n"
"		else																				\n"
"			cx = in_boundary.x + dx_mod + in_boundary.z;									\n"
"	}																						\n"
"	if(cy <= in_boundary.y)																	\n"
"	{																						\n"
"		if(fabs(fmod(height_overrun, 2)) < 1.0f)											\n"
"			cy = in_boundary.y + dy_mod;													\n"
"		else																				\n"
"			cy = in_boundary.y - dy_mod + in_boundary.w;									\n"
"	}																						\n"
"	if(cx >= (in_boundary.x + in_boundary.z))												\n"
"	{																						\n"
"		if(fabs(fmod(width_overrun, 2)) < 1.0f)												\n"
"			cx = in_boundary.x+in_boundary.z-dx_mod;										\n"
"		else																				\n"
"			cx = in_boundary.x + dx_mod;													\n"
"	}																						\n"
"	if(cy >= (in_boundary.y+ in_boundary.w))												\n"
"	{																						\n"
"		if(fabs(fmod(height_overrun, 2)) < 1.0f)											\n"
"			cy = in_boundary.y+in_boundary.w-dy_mod;										\n"
"		else																				\n"
"			cy = in_boundary.y+dy_mod;														\n"
"	}																						\n"
"	//end warp																				\n"
"	int ix ;																				\n"
"	int iy ;																				\n"
"	ix = (int)cx;																			\n"
"	iy = (int)cy;																			\n"
"	if(cx<0.0f)																				\n"
"		ix = -((int)(-cx) + 1);																\n"
"	if(cy < 0.0f)																			\n"
"		iy = -((int)-cy + 1);																\n"
"	int spx_pos = iy * boundary.z + ix;														\n"
"	int dpx_pos = gidy*roi.z+gidx;															\n"
"	dst_buf[dpx_pos] = src_buf[spx_pos];													\n"
"}																							\n"
"__kernel void apply_mirror_nowarp_CL(        float   mirror_angle, float   result_angle,	\n"
"									 int     nsegs,											\n"
"									 float   cen_x,float   cen_y,							\n"
"									 float   off_x,float   off_y,							\n"
"									 float   input_scale,									\n"
"									 const __global float4 *src_buf,						\n"
"									 int4    in_boundary,									\n"
"									 __global float4 *dst_buf,								\n"
"									 int4    boundary,										\n"
"									 int4    roi)											\n"
"{																							\n"
"	int gidx = get_global_id(0);															\n"
"	int gidy = get_global_id(1);															\n"
"	float cx;																				\n"
"	float cy;																				\n"
"	calc_undistorted_coords_CL(roi.x + gidx + 0.01f, roi.y + gidy - 0.01f,					\n"
"		mirror_angle, result_angle, nsegs, cen_x, cen_y,									\n"
"		off_x * input_scale, off_y * input_scale, &cx, &cy);								\n"
"	cx = in_boundary.x + (cx - in_boundary.x)/input_scale;									\n"
"	cy = in_boundary.y + (cy - in_boundary.y)/input_scale;									\n"
"	if(cx < boundary.x)																		\n"
"		cx = 0.0f;																			\n"
"	if(cy < boundary.y)																		\n"
"		cy = 0.0f;																			\n"
"	if(cx >= boundary.z)																	\n"
"		cx = boundary.z - 1;																\n"
"	if(cy >= boundary.w)																	\n"
"		cy = boundary.w - 1;																\n"
"	int ix ;																				\n"
"	int iy ;																				\n"
"	ix = (int)cx;																			\n"
"	iy = (int)cy;																			\n"
"	if(cx<0.0f)																				\n"
"		ix = -((int)(-cx) + 1);																\n"
"	if(cy < 0.0f)																			\n"
"		iy = -((int)-cy + 1);																\n"
"	int spx_pos = iy * boundary.z + ix;														\n"
"	int dpx_pos = gidy*roi.z+gidx;															\n"
"	dst_buf[dpx_pos] = src_buf[spx_pos];													\n"
"}																							\n";
#define CL_ERROR {return FALSE;}

static gegl_cl_run_data * cl_data = NULL;

static gboolean
apply_mirror_cl ( double               mirror_angle,
				  double               result_angle,
				  int                  nsegs,
				  double               cen_x,
				  double               cen_y,
				  double               off_x,
				  double               off_y,
				  double               input_scale,
				  gboolean             clip,
				  gboolean             warp,
				  Babl                *format,
				  GeglBuffer          *src,
				  GeglRectangle       *in_boundary,
				  GeglBuffer          *dst,
				  GeglRectangle       *boundary,
				  const GeglRectangle *roi)
{
	//Initiate some necessary data
	const Babl   *src_format = gegl_buffer_get_format(src);
	const Babl   *dst_format = gegl_buffer_get_format(dst);
	const Babl   * in_format = babl_format("RaGaBaA float");
	const Babl   * out_format= babl_format("RaGaBaA float");

	g_printf("[OpenCL] BABL formats: (%s,%s:%d) (%s,%s:%d)\n", babl_get_name(src_format),  babl_get_name(in_format),
		gegl_cl_color_supported (src_format, in_format),
		babl_get_name(out_format),babl_get_name(dst_format),
		gegl_cl_color_supported (out_format,dst_format));

	const size_t bpp_src     = babl_format_get_bytes_per_pixel(src_format);
	const size_t bpp_dst     = babl_format_get_bytes_per_pixel(dst_format);
	const size_t bpp_in      = babl_format_get_bytes_per_pixel( in_format);
	const size_t bpp_out     = babl_format_get_bytes_per_pixel(out_format);

	const size_t size_src    = boundary->width * boundary->height * bpp_src;
	const size_t size_dst    = roi->width * roi->height * bpp_dst;
	const size_t size_in     = boundary->width * boundary->height * bpp_in ;
	const size_t size_out    = roi->width * roi->height * bpp_out;

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

		gegl_buffer_get(src, 1.0, boundary,  in_format, src_buf,
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

		gegl_buffer_get(src, 1.0, boundary, src_format, src_buf,
			GEGL_AUTO_ROWSTRIDE);

		errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
			src_mem, src_buf, 
			NULL, NULL, NULL);
		if (CL_SUCCESS != errcode) CL_ERROR;

		gegl_cl_color_conv(&src_mem, &dst_mem, 1,
			boundary->width * boundary->height,
			src_format, in_format);

		errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
		if (CL_SUCCESS != errcode) CL_ERROR;
	}
    

	///////////////////////////Call the exact kernel to do mirrors//////////////////////////
	const size_t gbl_size[2] = {roi->width,roi->height};
	int kernel_flag=0;

	if (!cl_data)
	{
		const char *kernel_name[] = {"apply_mirror_warp_CL","apply_mirror_nowarp_CL", NULL};
		cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
	}
	if (!cl_data) CL_ERROR;

	mirror_angle               = mirror_angle * G_PI / 180;
	result_angle               = result_angle * G_PI / 180;
	cl_int4  param_in_boundary = {in_boundary->x,in_boundary->y,in_boundary->width,in_boundary->height};
	cl_int4  param_boundary    = {boundary->x,boundary->y,boundary->width,boundary->height};
	cl_int4  param_roi         = {roi->x,roi->y,roi->width,roi->height};
	cl_float f_mirror_angle = (cl_float)mirror_angle;
	cl_float f_result_angle = (cl_float)result_angle;
	cl_float f_cen_x = (cl_float)cen_x;
	cl_float f_cen_y = (cl_float)cen_y;
	cl_float f_off_x = (cl_float)off_x;
	cl_float f_off_y = (cl_float)off_y;
	cl_float f_input_scale = (cl_float)input_scale;

	if(warp){  //Call the kernel "apply_mirror_warp_CL"
		kernel_flag=0;
	}
	else{
		kernel_flag=1;
	}

	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 0, sizeof(cl_float), (void*)&f_mirror_angle));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 1, sizeof(cl_float), (void*)&f_result_angle));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 2, sizeof(cl_int), (void*)&nsegs));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 3, sizeof(cl_float), (void*)&f_cen_x));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 4, sizeof(cl_float), (void*)&f_cen_y));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 5, sizeof(cl_float), (void*)&f_off_x));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 6, sizeof(cl_float), (void*)&f_off_y));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 7, sizeof(cl_float), (void*)&f_input_scale));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 8, sizeof(cl_mem), (void*)&src_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 9, sizeof(cl_int4), (void*)&param_in_boundary));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 10, sizeof(cl_mem), (void*)&dst_mem));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 11, sizeof(cl_int4), (void*)&param_boundary));
	CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
		cl_data->kernel[kernel_flag], 12, sizeof(cl_int4), (void*)&param_roi));
	
	CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
		gegl_cl_get_command_queue(), cl_data->kernel[kernel_flag],
		2, NULL,
		gbl_size, NULL,
		0, NULL, NULL));

	errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
	if (CL_SUCCESS != errcode) CL_ERROR;

		////////////////////////////////////////////////////////////

    if (CL_COLOR_NOT_SUPPORTED == need_babl_out ||
	    CL_COLOR_EQUAL         == need_babl_out)
    {
	  dst_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
				  dst_mem, CL_TRUE, CL_MAP_READ,
				  0, size_out,
				  NULL, NULL, NULL,
				  &errcode);
	  if (CL_SUCCESS != errcode) CL_ERROR;

	  gegl_buffer_set(dst, roi, out_format, dst_buf,GEGL_AUTO_ROWSTRIDE);
	  

	  errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
				  dst_mem, dst_buf, 
				  NULL, NULL, NULL);
	  if (CL_SUCCESS != errcode) CL_ERROR;
   }
   else if (CL_COLOR_CONVERT == need_babl_out)
   {
	  gegl_cl_color_conv(&dst_mem, &src_mem, 1,
		  roi->width * roi->height,
		  out_format, dst_format);
	  errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
	  if (CL_SUCCESS != errcode) CL_ERROR;

	  dst_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
				  dst_mem, CL_TRUE, CL_MAP_READ,
				  0, size_dst,
				  NULL, NULL, NULL,
				  &errcode);
	  if (CL_SUCCESS != errcode) CL_ERROR;

	  gegl_buffer_set(dst, roi, dst_format, dst_buf,GEGL_AUTO_ROWSTRIDE);

	  errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
				  dst_mem, dst_buf, 
				  NULL, NULL, NULL);
	  if (CL_SUCCESS != errcode) CL_ERROR;
   }

   gegl_buffer_flush(dst);
   if(src_mem)    gegl_clReleaseMemObject(src_mem);
   if(dst_mem)    gegl_clReleaseMemObject(dst_mem);
   return TRUE;
}


/*****************************************************************************/

static GeglRectangle
get_effective_area (GeglOperation *operation)
{
  GeglRectangle  result = {0,0,0,0};
  GeglRectangle *in_rect = gegl_operation_source_get_bounding_box (operation, "input");
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
  gdouble xt = o->trim_x * in_rect->width;
  gdouble yt = o->trim_y * in_rect->height;

  gegl_rectangle_copy(&result, in_rect);

  /*Applying trims*/

  result.x = result.x + xt;
  result.y = result.y + yt;
  result.width = result.width - xt;
  result.height = result.height - yt;

  return result;
}

/* Compute the region for which this operation is defined.
 */
static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglRectangle  result = {0,0,0,0};
  GeglRectangle *in_rect = gegl_operation_source_get_bounding_box (operation, "input");
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);

  if (!in_rect){
        return result;
  }

  if (o->clip) {
    gegl_rectangle_copy(&result, in_rect);
  }
  else {
    result.x = in_rect->x;
    result.y = in_rect->y;
    result.width = result.height = sqrt (in_rect->width * in_rect->width + in_rect->height * in_rect->height) * MAX ((o->o_x + 1),  (o->o_y + 1)) * 2;
  }

  result.width = result.width * o->output_scale;
  result.height = result.height * o->output_scale;

  #ifdef TRACE
    g_warning ("< get_bounding_box result = %dx%d+%d+%d", result.width, result.height, result.x, result.y);
  #endif
  return result;
}

/* Compute the input rectangle required to compute the specified region of interest (roi).
 */
static GeglRectangle
get_required_for_output (GeglOperation       *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *roi)
{
  GeglRectangle  result = get_effective_area (operation);

  #ifdef TRACE
    g_warning ("> get_required_for_output src=%dx%d+%d+%d", result.width, result.height, result.x, result.y);
    if (roi)
      g_warning ("  ROI == %dx%d+%d+%d", roi->width, roi->height, roi->x, roi->y);
  #endif

  return result;
}

/* Specify the input and output buffer formats.
 */
// static void
// prepare (GeglOperation *operation)
// {
// 	gegl_operation_set_format (operation, "input", babl_format ("RaGaBaA float"));
// 	gegl_operation_set_format (operation, "output", babl_format ("RaGaBaA float"));
// }


static void
prepare (GeglOperation *operation)
{
  gegl_operation_set_format (operation, "input", babl_format ("RaGaBaA float"));
  // gegl_operation_set_format (operation, "output", babl_format ("RaGaBaA float"));
  //Set the source pixel data format as the output format of current operation
  GeglNode * self;
  GeglPad *pad;
  //default format:RGBA float
  Babl * format=babl_format ("RaGaBaA float");
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

/* Perform the specified operation.
 */
static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
  GeglRectangle boundary = gegl_operation_get_bounding_box (operation);
  GeglRectangle  eff_boundary = get_effective_area (operation);
  Babl *format = babl_format ("RaGaBaA float");

#ifdef DO_NOT_USE_BUFFER_SAMPLE
  //printf("define DO_NOT_USE_BUFFER_SAMPLE:\n");
  printf("NOT USING BUFFER SAMPLE!\n");
  g_warning ("NOT USING BUFFER SAMPLE!");
#endif
  if(apply_mirror_cl(   o->m_angle,
						o->r_angle,
						o->n_segs,
						o->c_x * boundary.width,
						o->c_y * boundary.height,
						o->o_x * (eff_boundary.width  - eff_boundary.x) + eff_boundary.x,
						o->o_y * (eff_boundary.height - eff_boundary.y) + eff_boundary.y,
						o->input_scale / 100,
						o->clip,
						o->warp,
						format,
						input,
						&eff_boundary,
						output,
						&boundary,
						result))
  return TRUE;

  apply_mirror(   o->m_angle,
				  o->r_angle,
				  o->n_segs,
				  o->c_x * boundary.width,
				  o->c_y * boundary.height,
				  o->o_x * (eff_boundary.width  - eff_boundary.x) + eff_boundary.x,
				  o->o_y * (eff_boundary.height - eff_boundary.y) + eff_boundary.y,
				  o->input_scale / 100,
				  o->clip,
				  o->warp,
				  format,
				  input,
				  &eff_boundary,
				  output,
				  &boundary,
				  result);
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
  operation_class->prepare = prepare;
  operation_class->get_bounding_box = get_bounding_box;
  operation_class->get_required_for_output = get_required_for_output;

  operation_class->name        = "gegl:mirrors";
  operation_class->categories  = "blur";
  operation_class->description =
        _("Applies mirroring effect on the image.");
}

#endif
