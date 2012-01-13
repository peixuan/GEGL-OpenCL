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

   /* no properties */

#else

#define GEGL_CHANT_TYPE_POINT_FILTER
#define GEGL_CHANT_C_FILE       "invert.c"
#define GEGLV4

#include "gegl-chant.h"

static void prepare (GeglOperation *operation)
{

	gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));

	GeglOperationClass            *operation_class;
	operation_class=GEGL_OPERATION_GET_CLASS(operation);
	Babl * format=babl_format ("RGBA float");
	if(operation_class->opencl_support){
		//Set the source pixel data format as the output format of current operation
		GeglNode * self;
		GeglPad *pad;
		//default format:RGBA float

		//get the source pixel data format
		self=gegl_operation_get_source_node(operation,"input");
		while(self){
			if(strcmp(gegl_node_get_operation(self),"gimp:tilemanager-source")==0){
				format=gegl_operation_get_format(self->operation,"output");
				break;
			}
			self=gegl_operation_get_source_node(self->operation,"input");
		}
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
  glong   i;
  gfloat *in  = in_buf;
  gfloat *out = out_buf;

  for (i=0; i<samples; i++)
    {
      int  j;
      for (j=0; j<3; j++)
        {
          gfloat c;
          c = in[j];
          c = 1.0 - c;
          out[j] = c;
        }
      out[3]=in[3];
      in += 4;
      out+= 4;
    }
  return TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |   \n"
"                    CLK_ADDRESS_NONE                       |   \n"
"                    CLK_FILTER_NEAREST;                        \n"
"__kernel void kernel_invert(__global  const float4     *in,    \n"
"                        __global  float4     *out )            \n"
"{                                                              \n"
"  int gid = get_global_id(0);                                  \n"
"  float4 in_v  =in[gid];                                     \n"
"  float4 out_v;                                                \n"
"  out_v.xyz = 1.0-in_v.xyz;                                    \n"
"  out_v.w   =  in_v.w;                                         \n"
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


  cl_int errcode = 0;

  if (!cl_data)
    {
      const char *kernel_name[] = {"kernel_invert", NULL};
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
      g_warning("[OpenCL] Error in Invert Kernel\n");
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

  operation_class->name        = "gegl:invert";

  operation_class->opencl_support = TRUE;

  operation_class->categories  = "color";
  operation_class->description =
     _("Inverts the components (except alpha), the result is the "
       "corresponding \"negative\" image.");
}

#endif
