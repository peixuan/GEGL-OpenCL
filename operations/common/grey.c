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
#define GEGL_CHANT_C_FILE       "grey.c"

#include "gegl-chant.h"

static void prepare (GeglOperation *operation)
{
  Babl * op_format = babl_format ("YA float");
  Babl * format=op_format;  
  GeglNode * self;
  GeglPad *pad;
  gegl_operation_set_format (operation, "input", op_format);

  //Set the source pixel data format as the output format of current operation
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

/* XXX: could be sped up by special casing op-filter behavior */

static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                samples,
         const GeglRectangle *roi)
{
  float *in = in_buf;
  float *out = out_buf;
  while (samples--)
    {
      *out++ = *in++;
      *out++ = *in++;
    }

  return TRUE;
}

// static gboolean
// cl_process (GeglOperation       *op,
// 			cl_mem              in_tex,
// 			cl_mem              out_tex,
// 			const size_t global_worksize[1],
// 			const GeglRectangle *roi)
// {
// 	
// 	cl_mem  temp;
// 	temp   = in_tex;
// 	in_tex = out_tex;
// 	out_tex= temp; 
// 	return CL_SUCCESS;
// }

#include "opencl/gegl-cl.h"

static const char* kernel_source =

"__kernel void kernel_grey(__global  const float2     *in,      \n"
"                        __global  float2     *out )            \n"
"{                                                              \n"
"  int gid = get_global_id(0);                                  \n"
"  float2 in_v  = in[gid];                                      \n"
"  out[gid]=in_v;                                               \n"
"}                                                              \n";

static gegl_cl_run_data *cl_data = NULL;

static gboolean
cl_process (GeglOperation       *op,
			cl_mem              in_tex,
			cl_mem              out_tex,
			const size_t global_worksize[1],
			const GeglRectangle *roi)
{

	cl_int errcode = 0;

	if (!cl_data)
	{
		const char *kernel_name[] = {"kernel_grey", NULL};
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
		g_warning("[OpenCL] Error in Grey Kernel\n");
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

  operation_class = GEGL_OPERATION_CLASS (klass);
  point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  point_filter_class->process = process;
  operation_class->prepare = prepare;
  point_filter_class->cl_process           = cl_process;
  operation_class->opencl_support = TRUE;

  operation_class->name        = "gegl:grey";
  operation_class->categories  = "color";
  operation_class->description = _("Turns the image greyscale");
}

#endif
