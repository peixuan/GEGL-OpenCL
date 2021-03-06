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


/* Followed by this #if ... */
#ifdef GEGL_CHANT_PROPERTIES
/* ... are the properties of the filter, these are all scalar values
 * (doubles), the the parameters are:
 *       property name,   min,   max, default, "description of property"
 */

gegl_chant_double (contrast, _("Contrast"), -5.0, 5.0, 1.0,
                   _("Range scale factor"))
gegl_chant_double (brightness, _("Brightness"), -3.0, 3.0, 0.0,
                   _("Amount to increase brightness"))

/* this will create the instance structure for our use, and register the
 * property with the given ranges, default values and a comment for the
 * documentation/tooltip.
 */
#else

/* Specify the base class we're building our operation on, the base
 * class provides a lot of the legwork so we do not have to. For
 * brightness contrast the best base class is the POINT_FILTER base
 * class.
 */
#define GEGL_CHANT_TYPE_POINT_FILTER

/* We specify the file we're in, this is needed to make the code
 * generation for the properties work.
 */
#define GEGL_CHANT_C_FILE       "brightness-contrast.c"

/* Including gegl-chant.h creates most of the GObject boiler plate
 * needed, creating a GeglChant instance structure a GeglChantClass
 * structure for our operation, as well as the needed code to register
 * our new gobject with GEGL.
 */
#include "gegl-chant.h"

/* prepare() is called on each operation providing data to a node that
 * is requested to provide a rendered result. When prepare is called
 * all properties are known. For brightness contrast we use this
 * opportunity to dictate the formats of the input and output buffers.
 */
// static void prepare (GeglOperation *operation)
// {
//   gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
//   gegl_operation_set_format (operation, "output", babl_format ("RGBA u8"));
// }

/* For GeglOperationPointFilter subclasses, we operate on linear
 * buffers with a pixel count.
 */

static void prepare (GeglOperation *operation)
{

	gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
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
         glong                n_pixels,
         const GeglRectangle *roi)
{
  /* Retrieve a pointer to GeglChantO structure which contains all the
   * chanted properties
   */
  GeglChantO *o = GEGL_CHANT_PROPERTIES (op);
  gfloat     * GEGL_ALIGNED in_pixel;
  gfloat     * GEGL_ALIGNED out_pixel;
  gfloat      brightness, contrast;
  glong       i;

  in_pixel   = in_buf;
  out_pixel  = out_buf;

  brightness = o->brightness;
  contrast   = o->contrast;

  for (i=0; i<n_pixels; i++)
    {
      out_pixel[0] = (in_pixel[0] - 0.5f) * contrast + brightness + 0.5;
      out_pixel[1] = (in_pixel[1] - 0.5f) * contrast + brightness + 0.5;
      out_pixel[2] = (in_pixel[2] - 0.5f) * contrast + brightness + 0.5;
      out_pixel[3] = in_pixel[3]; /* copy the alpha */
      in_pixel  += 4;
      out_pixel += 4;
    }
  return TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |   \n"
"                    CLK_ADDRESS_NONE                       |   \n"
"                    CLK_FILTER_NEAREST;                        \n"
"__kernel void kernel_bc(__global  const float4     *in,        \n"
"                        __global  float4     *out ,            \n"
"                         float brightness,                     \n"
"                         float contrast)                       \n"
"{                                                              \n"
"  int gid = get_global_id(0);                                  \n"
"  float4 in_v  = in[gid];                                      \n"
"  float4 out_v;                                                \n"
"  out_v.xyz = (in_v.xyz - 0.5f) * contrast + brightness + 0.5f;\n"
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

  gfloat brightness = o->brightness;
  gfloat contrast   = o->contrast;

  cl_int errcode = 0;

  if (!cl_data)
    {
      const char *kernel_name[] = {"kernel_bc", NULL};
      cl_data = gegl_cl_compile_and_build (kernel_source, kernel_name);
    }

  if (!cl_data) return 1;

  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 0, sizeof(cl_mem),   (void*)&in_tex));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 1, sizeof(cl_mem),   (void*)&out_tex));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 2, sizeof(cl_float), (void*)&brightness));
  CL_SAFE_CALL(errcode = gegl_clSetKernelArg(cl_data->kernel[0], 3, sizeof(cl_float), (void*)&contrast));

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


/*
 * The class init function sets up information needed for this operations class
 * (template) in the GObject OO framework.
 */
static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass            *operation_class;
  GeglOperationPointFilterClass *point_filter_class;

  operation_class    = GEGL_OPERATION_CLASS (klass);
  point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

  /* override the prepare methods of the GeglOperation class */
 // operation_class->prepare = prepare;
  /* override the process method of the point filter class (the process methods
   * of our superclasses deal with the handling on their level of abstraction)
   */
  point_filter_class->process = process;
  operation_class->prepare = prepare;

  point_filter_class->cl_process           = cl_process;

  /* specify the name this operation is found under in the GUI/when
   * programming/in XML
   */
  operation_class->name        = "gegl:brightness-contrast";
  operation_class->opencl_support = TRUE;

  /* a colon separated list of categories/tags for this operations */
  operation_class->categories  = "color";

  /* a description of what this operations does */
  operation_class->description = _("Changes the light level and contrast.");
}

#endif /* closing #ifdef GEGL_CHANT_PROPERTIES ... else ... */
