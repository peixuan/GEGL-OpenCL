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

gegl_chant_double (high_a_delta, _("High a delta"), -2.0, 2.0, 0.0, _(""))
gegl_chant_double (high_b_delta, _("High b delta"), -2.0, 2.0, 0.0, _(""))
gegl_chant_double (low_a_delta,  _("Low a delta"),  -2.0, 2.0, 0.0, _(""))
gegl_chant_double (low_b_delta,  _("Low b delta"),  -2.0, 2.0, 0.0, _(""))
gegl_chant_double (saturation,   _("Saturation"),   -3.0, 3.0, 1.0, _(""))

#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE       "whitebalance.c"

#include "gegl-chant.h"

static void prepare (GeglOperation *operation)
{

	gegl_operation_set_format (operation, "input", babl_format ("Y'CbCrA float"));

	Babl * format=babl_format ("Y'CbCrA float");	
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


/* GeglOperationPointFilter gives us a linear buffer to operate on
 * in our requested pixel format
 */
static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi)
{
  GeglChantO *o = GEGL_CHANT_PROPERTIES (op);
  gfloat     *in_pixel;
  gfloat     *out_pixel;
  gfloat      a_base;
  gfloat      a_scale;
  gfloat      b_base;
  gfloat      b_scale;
  glong       i;

  in_pixel = in_buf;
  out_pixel = out_buf;

  a_scale = (o->high_a_delta - o->low_a_delta);
  a_base = o->low_a_delta;
  b_scale = (o->high_b_delta - o->low_b_delta);
  b_base = o->low_b_delta;

  for (i=0; i<n_pixels; i++)
    {
      out_pixel[0] = in_pixel[0];
      out_pixel[1] = in_pixel[1];
      out_pixel[2] = in_pixel[2];
      out_pixel[3] = in_pixel[3];
      out_pixel[1] += in_pixel[0] * a_scale + a_base;
      out_pixel[2] += in_pixel[0] * b_scale + b_base;
      out_pixel[1] = out_pixel[1] * o->saturation;
      out_pixel[2] = out_pixel[2] * o->saturation;
      in_pixel += 4;
      out_pixel += 4;
    }
  return TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |   \n"
"                    CLK_ADDRESS_NONE                       |   \n"
"                    CLK_FILTER_NEAREST;                        \n"
"__kernel void kernel_white_balance(__global  const float4     *in,        \n"
"                        __global  float4     *out ,            \n"
"                         float a_base,                         \n"
"                         float a_scale,                        \n"
"                         float b_base,                         \n"
"                         float b_scale,                        \n"
"                         float saturation)                     \n"
"{                                                              \n"
"  int gid = get_global_id(0);                                  \n"
"  float4 in_v  = in[gid];                                     \n"
"  float4 out_v;                                                \n"
"  out_v = in_v;                                                \n"
"  out_v.y   =  (out_v.y+in_v.x*a_scale+a_base)*saturation;     \n"
"  out_v.z   =  (out_v.z+in_v.x*b_scale+b_base)*saturation;     \n"
"  out[gid]=out_v;                                              \n"
"}                                                              \n";

static gegl_cl_run_data *cl_data = NULL;

/* OpenCL processing function */
static gboolean
cl_process (GeglOperation       *op,
            cl_mem              in_tex,
            cl_mem              out_tex,
            const size_t global_worksize[1],
            const GeglRectangle *roi)
{
  /* Retrieve a pointer to GeglChantO structure which contains all the
   * chanted properties
   */

	GeglChantO *o = GEGL_CHANT_PROPERTIES (op);

	gfloat      a_base;
	gfloat      a_scale;
	gfloat      b_base;
	gfloat      b_scale;
	gfloat      sation;

	a_scale = (o->high_a_delta - o->low_a_delta);
	a_base = o->low_a_delta;
	b_scale = (o->high_b_delta - o->low_b_delta);
	b_base = o->low_b_delta;
	sation=o->saturation;


  cl_int errcode = 0;

  if (!cl_data)
    {
      const char *kernel_name[] = {"kernel_white_balance", NULL};
      cl_data = gegl_cl_compile_and_build (kernel_source, kernel_name);
    }

  if (!cl_data) return 1;

  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 0, sizeof(cl_mem),   (void*)&in_tex));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 1, sizeof(cl_mem),   (void*)&out_tex));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 2, sizeof(cl_float), (void*)&a_base));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 3, sizeof(cl_float), (void*)&a_scale));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 4, sizeof(cl_float), (void*)&b_base));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 5, sizeof(cl_float), (void*)&b_scale));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 6, sizeof(cl_float), (void*)&sation));

  CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(gegl_cl_get_command_queue (),
                                                     cl_data->kernel[0], 1,
                                                     NULL, global_worksize, NULL,
                                                     0, NULL, NULL) );

  if (errcode != CL_SUCCESS)
    {
      g_warning("[OpenCL] Error in White-Balance Kernel\n");
      return errcode;
    }

  //g_printf("[OpenCL] Running Brightness-Constrast Kernel in region (%d %d %d %d)\n", roi->x, roi->y, roi->width, roi->height);
  return errcode;
}

static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass            *operation_class;
  GeglOperationPointFilterClass *point_filter_class;

  operation_class    = GEGL_OPERATION_CLASS (klass);
  point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  
  operation_class->prepare = prepare;
  point_filter_class->process = process;

  point_filter_class->cl_process           = cl_process;
//  point_filter_class->cl_kernel_source     = kernel_source;

  operation_class->name        = "gegl:whitebalance";
  operation_class->opencl_support = TRUE;

  operation_class->categories  = "color";
  operation_class->description =
        _("Allows changing the whitepoint and blackpoint of an image.");
}

#endif
