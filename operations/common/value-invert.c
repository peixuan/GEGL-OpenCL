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
 * Copyright 2007 Mukund Sivaraman <muks@mukund.org>
 */

/*
 * The plug-in only does v = 1.0 - v; for each pixel in the image, or
 * each entry in the colormap depending upon the type of image, where 'v'
 * is the value in HSV color model.
 *
 * The plug-in code is optimized towards this, in that it is not a full
 * RGB->HSV->RGB transform, but shortcuts many of the calculations to
 * effectively only do v = 1.0 - v. In fact, hue is never calculated. The
 * shortcuts can be derived from running a set of r, g, b values through the
 * RGB->HSV transform and then from HSV->RGB and solving out the redundant
 * portions.
 *
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

   /* no properties */

#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE       "value-invert.c"

#include "gegl-chant.h"

static void prepare (GeglOperation *operation)
{

	gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
	GeglOperationClass            *operation_class;
	operation_class=GEGL_OPERATION_GET_CLASS(operation);

	Babl * format=babl_format ("RGBA float");	
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
  glong   j;
  gfloat *src  = in_buf;
  gfloat *dest = out_buf;
  gfloat  r, g, b;
  gfloat  value, min;
  gfloat  delta;

  for (j = 0; j < samples; j++)
    {
      r = *src++;
      g = *src++;
      b = *src++;

      if (r > g)
        {
          value = MAX (r, b);
          min = MIN (g, b);
        }
      else
        {
          value = MAX (g, b);
          min = MIN (r, b);
        }

      delta = value - min;
      if ((value == 0) || (delta == 0))
        {
          r = 1.0 - value;
          g = 1.0 - value;
          b = 1.0 - value;
        }
      else
        {
          if (r == value)
            {
              r = 1.0 - r;
              b = r * b / value;
              g = r * g / value;
            }
          else if (g == value)
            {
              g = 1.0 - g;
              r = g * r / value;
              b = g * b / value;
            }
          else
            {
              b = 1.0 - b;
              g = b * g / value;
              r = b * r / value;
            }
        }

      *dest++ = r;
      *dest++ = g;
      *dest++ = b;

      *dest++ = *src++;
    }
  return TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |   \n"
"                    CLK_ADDRESS_NONE                       |   \n"
"                    CLK_FILTER_NEAREST;                        \n"
"__kernel void kernel_bc(__global  const float4     *in,        \n"
"                        __global  float4     *out)             \n"
"{                                                              \n"
"  int gid = get_global_id(0);                                  \n"
"  float4 in_v  = in[gid];                                      \n"
"  float4 out_v;                                                \n"
"  float cmax,cmin;												\n"
"  cmax=max(in_v.x,max(in_v.y,in_v.z));	                        \n"							
"  cmin=min(in_v.x,min(in_v.y,in_v.z));                         \n"
"  if((cmax==0.0f)||((cmax-cmin)==0.0f))                        \n"
"	  out_v.xyz=1.0f-cmax;                                      \n"                                  
"  else{														\n"
"	  if(cmax==in_v.x)                                          \n"
"     {                                                         \n"
"         out_v.x=1.0f-cmax;                                    \n"
"		  out_v.y=out_v.x * in_v.y / cmax;                      \n"
"		  out_v.z=out_v.x * in_v.z / cmax;                      \n"
"      }                                                        \n"
"	  else if(cmax==in_v.y)                                     \n"
"     {                                                         \n"
"         out_v.y=1.0f-cmax;                                    \n"
"		  out_v.x=out_v.y * in_v.x / cmax;                      \n"
"		  out_v.z=out_v.y * in_v.z / cmax;                      \n"
"      }                                                        \n"
"	  else														\n"
"     {                                                         \n"
"         out_v.z=1.0f-cmax;                                    \n"
"		  out_v.y=out_v.z * in_v.y / cmax;                      \n"
"		  out_v.x=out_v.z * in_v.x / cmax;                      \n"
"      }                                                        \n"
"  }															\n"
"  out_v.w=in_v.w;                                              \n"
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

  cl_int errcode = 0;

  if (!cl_data)
    {
      const char *kernel_name[] = {"kernel_bc", NULL};
      cl_data = gegl_cl_compile_and_build (kernel_source, kernel_name);
    }

  if (!cl_data) return 1;

  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 0, sizeof(cl_mem),   (void*)&in_tex));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 1, sizeof(cl_mem),   (void*)&out_tex));

  CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(gegl_cl_get_command_queue (),
                                                     cl_data->kernel[0], 1,
                                                     NULL, global_worksize, NULL,
                                                     0, NULL, NULL) );

  if (errcode != CL_SUCCESS)
    {
      g_warning("[OpenCL] Error in Brightness-Constrast Kernel\n");
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
  operation_class->opencl_support = TRUE;

  operation_class->name        = "gegl:value-invert";
  operation_class->categories  = "color";
  operation_class->description =
        _("Inverts just the value component, the result is the corresponding "
          "`inverted' image.");
}

#endif

