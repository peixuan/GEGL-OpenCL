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
 * Copyright 2007 Mark Probst <mark.probst@gmail.com>
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

gegl_chant_int (sampling_points, _("Sample points"), 0, 65536, 0,
                _("Number of curve sampling points.  0 for exact calculation."))
gegl_chant_curve (curve, _("Curve"), _("The contrast curve."))

#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE  "contrast-curve.c"

#include "gegl-chant.h"

char* readSource(const char *sourceFilename) {

	FILE *fp;
	int err;
	int size;

	char *source;

	fp = fopen(sourceFilename, "rb");
	if(fp == NULL) {
		printf("Could not open kernel file: %s\n", sourceFilename);
		exit(-1);
	}

	err = fseek(fp, 0, SEEK_END);
	if(err != 0) {
		printf("Error seeking to end of file\n");
		exit(-1);
	}

	size = ftell(fp);
	if(size < 0) {
		printf("Error getting file position\n");
		exit(-1);
	}

	err = fseek(fp, 0, SEEK_SET);
	if(err != 0) {
		printf("Error seeking to start of file\n");
		exit(-1);
	}

	source = (char*)malloc(size+1);
	if(source == NULL) {
		printf("Error allocating %d bytes for the program source\n", size+1);
		exit(-1);
	}

	err = fread(source, 1, size, fp);
	if(err != size) {
		printf("only read %d bytes\n", err);
		exit(0);
	}

	source[size] = '\0';

	return source;
}


static void prepare (GeglOperation *operation)
{
  Babl *format = babl_format ("YA float");
  gegl_operation_set_format (operation, "input", format);

  //Set the source pixel data format as the output format of current operation
  GeglNode * self;
  GeglPad *pad;
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
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                samples,
         const GeglRectangle *roi)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (op);
  gint        num_sampling_points;
  GeglCurve  *curve;
  gint i;
  gfloat  *in  = in_buf;
  gfloat  *out = out_buf;
  gdouble *xs, *ys;

  num_sampling_points = o->sampling_points;
  curve = o->curve;

  if (num_sampling_points > 0)
  {
    xs = g_new(gdouble, num_sampling_points);
    ys = g_new(gdouble, num_sampling_points);

    gegl_curve_calc_values(o->curve, 0.0, 1.0, num_sampling_points, xs, ys);

    g_free(xs);

    for (i=0; i<samples; i++)
    {
      gint x = in[0] * num_sampling_points;
      gfloat y;

      if (x < 0)
       y = ys[0];
      else if (x >= num_sampling_points)
       y = ys[num_sampling_points - 1];
      else
       y = ys[x];

      out[0] = y;
      out[1]=in[1];

      in += 2;
      out+= 2;
    }

    g_free(ys);
  }
  else
    for (i=0; i<samples; i++)
    {
      gfloat u = in[0];

      out[0] = gegl_curve_calc_value(curve, u);
      out[1]=in[1];

      in += 2;
      out+= 2;
    }

  return TRUE;
}

#include "opencl/gegl-cl.h"
static gegl_cl_run_data *cl_data = NULL;

/* OpenCL processing function */
static gboolean
cl_process (GeglOperation       *op,
			cl_mem              in_tex,
			cl_mem              out_tex,
			const size_t global_worksize[1],
			const GeglRectangle *roi)
{
	GeglChantO *o = GEGL_CHANT_PROPERTIES (op);
	gint        num_sampling_points;
	GeglCurve  *curve;
	gint i;
	gdouble *xs, *ys;
	gfloat * lum;

	cl_int errcode = 0;

	num_sampling_points = o->sampling_points;
	curve = o->curve;

	if (num_sampling_points > 0)
	{
		//Call the contrast-curve kernel
		cl_mem ys_men;
		int k;
		xs = g_new(gdouble, num_sampling_points);
		ys = g_new(gdouble, num_sampling_points);

		gegl_curve_calc_values(o->curve, 0.0, 1.0, num_sampling_points, xs, ys);

		lum = (gfloat *)xs;
		for(k=0;k<num_sampling_points;k++)
			lum[k]=(gfloat)ys[k];
		g_free(ys);
		
		ys_men=gegl_clCreateBuffer(gegl_cl_get_context(),
				CL_MEM_USE_HOST_PTR|CL_MEM_READ_ONLY,
				sizeof(gfloat)*num_sampling_points,
				lum, &errcode);
		if (CL_SUCCESS != errcode) return errcode;;

		if (!cl_data)
		{
			const char *sourceFile = "ContrastCurve.cl";
			const char *kernel_name[] = {"contrast_curve", NULL};
			char* kernel_source = readSource(sourceFile);
			cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
		}
		if (!cl_data) return 1;

		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 0, sizeof(cl_mem),   (void*)&in_tex));
		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 1, sizeof(cl_mem),   (void*)&ys_men));
		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 2, sizeof(cl_mem),   (void*)&out_tex));
		CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 3, sizeof(cl_float), (void*)&num_sampling_points));

		CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(gegl_cl_get_command_queue (),
			cl_data->kernel[0], 1,
			NULL, global_worksize, NULL,
			0, NULL, NULL) );

		if (errcode != CL_SUCCESS)
		{
			g_warning("[OpenCL] Error in ContrastCurve Kernel\n");
			return errcode;
		}

		if(ys_men)  gegl_clReleaseMemObject(ys_men);
		g_free(xs);
	}
	else{
		gfloat * in_data, * out_data;
		gint   channel = 2;/* YA float*/
		size_t count=global_worksize[0] * babl_format_get_bytes_per_pixel(babl_format ("YA float"));

		in_data=gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(), in_tex, CL_TRUE,
			CL_MAP_READ,0, count, 0, NULL, NULL, &errcode);
		if (errcode != CL_SUCCESS) return errcode;;	

		out_data=gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(), out_tex, CL_TRUE,
			CL_MAP_WRITE,0, count, 0, NULL, NULL, &errcode);
		if (errcode != CL_SUCCESS) return errcode;;

		for (i=0; i<num_sampling_points; i++)
		{
			gint j=channel * i;
			gfloat u = in_data[j];

			out_data[j] = gegl_curve_calc_value(curve, u);
			out_data[j+1]=in_data[j+1];

		}

		errcode = gegl_clEnqueueUnmapMemObject (gegl_cl_get_command_queue(), out_tex, 
			out_data,0, NULL, NULL);
		if (errcode != CL_SUCCESS) return errcode;

		errcode = gegl_clEnqueueUnmapMemObject (gegl_cl_get_command_queue(), in_tex, 
			in_data,0, NULL, NULL);
		if (errcode != CL_SUCCESS) return errcode;
		
	}

		return errcode;
}



static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass            *operation_class;
  GeglOperationPointFilterClass *point_filter_class;

  operation_class    = GEGL_OPERATION_CLASS (klass);
  point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  point_filter_class->process = process;
  operation_class->prepare = prepare;
  point_filter_class->cl_process           = cl_process;

  operation_class->opencl_support = TRUE;
  operation_class->name        = "gegl:contrast-curve";
  operation_class->categories  = "color";
  operation_class->description =
        _("Adjusts the contrast of the image according to a curve.");
}

#endif
