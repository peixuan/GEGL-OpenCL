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
 * Copyright 2005 Øyvind Kolås <pippin@gimp.org>,
 *           2007 Øyvind Kolås <oeyvindk@hig.no>
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"

#ifdef GEGL_CHANT_PROPERTIES


gegl_chant_double (blur_radius, _("Blur radius"), 0.0, 70.0, 4.0,
  _("Radius of square pixel region, (width and height will be radius*2+1)."))
gegl_chant_double (edge_preservation, _("Edge preservation"), 0.0, 70.0, 8.0,
  _("Amount of edge preservation"))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "bilateral-filter.c"

#include "gegl-chant.h"
#include <math.h>

static void
bilateral_filter (GeglBuffer          *src,
                  const GeglRectangle *src_rect,
                  GeglBuffer          *dst,
                  const GeglRectangle *dst_rect,
                  gdouble              radius,
                  gdouble              preserve);
static void
bilateral_filter_cl(GeglBuffer          *src,
                    const GeglRectangle *src_rect,
                    GeglBuffer          *dst,
                    const GeglRectangle *dst_rect,
                    gdouble              radius,
                    gdouble              preserve);

#include <stdio.h>

static void prepare (GeglOperation *operation)
{
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

  area->left = area->right = area->top = area->bottom = ceil (o->blur_radius);
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static void prepare_cl (GeglOperation *operation)
{
    GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
    GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

    area->left = area->right = area->top = area->bottom = ceil (o->blur_radius);

    gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
    GeglNode * self;
    GeglPad *pad;
    Babl * format=babl_format ("RGBA float");
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
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
  GeglChantO   *o = GEGL_CHANT_PROPERTIES (operation);
  GeglRectangle compute;

  compute = gegl_operation_get_required_for_output (operation, "input",result);

  if (o->blur_radius < 1.0)
    {
      output = g_object_ref (input);
    }
  else
    {
        if (gegl_cl_is_opencl_available())
            bilateral_filter_cl (input, &compute, output, result, o->blur_radius, o->edge_preservation);
        else
      bilateral_filter (input, &compute, output, result, o->blur_radius, o->edge_preservation);
    }

  return  TRUE;
}

static void
bilateral_filter (GeglBuffer          *src,
                  const GeglRectangle *src_rect,
                  GeglBuffer          *dst,
                  const GeglRectangle *dst_rect,
                  gdouble              radius,
                  gdouble              preserve)
{
  gfloat *gauss;
  gint x,y;
  gint offset;
  gfloat *src_buf;
  gfloat *dst_buf;
  gint width = (gint) radius * 2 + 1;
  gint iradius = radius;
  gint src_width = src_rect->width;
  gint src_height = src_rect->height;

  gauss = g_newa (gfloat, width * width);
  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RGBA float"), src_buf, GEGL_AUTO_ROWSTRIDE);

  offset = 0;

#define POW2(a) ((a)*(a))
  for (y=-iradius;y<=iradius;y++)
    for (x=-iradius;x<=iradius;x++)
      {
        gauss[x+(int)radius + (y+(int)radius)*width] = exp(- 0.5*(POW2(x)+POW2(y))/radius   );
      }

  for (y=0; y<dst_rect->height; y++)
    for (x=0; x<dst_rect->width; x++)
      {
        gint u,v;
        gfloat *center_pix = src_buf + ((x+iradius)+((y+iradius) * src_width)) * 4;
        gfloat  accumulated[4]={0,0,0,0};
        gfloat  count=0.0;

        for (v=-iradius;v<=iradius;v++)
          for (u=-iradius;u<=iradius;u++)
            {
              gint i,j;
              i = x + radius + u;
              j = y + radius + v;
              if (i >= 0 && i < src_width &&
                  j >= 0 && j < src_height)
                {
                  gint c;

                  gfloat *src_pix = src_buf + (i + j * src_width) * 4;

                  gfloat diff_map   = exp (- (POW2(center_pix[0] - src_pix[0])+
                                              POW2(center_pix[1] - src_pix[1])+
                                              POW2(center_pix[2] - src_pix[2])) * preserve
                                          );
                  gfloat gaussian_weight;
                  gfloat weight;

                  gaussian_weight = gauss[u+(int)radius+(v+(int)radius)*width];

                  weight = diff_map * gaussian_weight;

                  for (c=0;c<4;c++)
                    {
                      accumulated[c] += src_pix[c] * weight;
                    }
                  count += weight;
                }
            }

        for (u=0; u<4;u++)
          dst_buf[offset*4+u] = accumulated[u]/count;
        offset++;
      }
  gegl_buffer_set (dst, dst_rect, babl_format ("RGBA float"), dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"#define POW2(a) ((a)*(a))                                             \n"
"kernel void bilateral_filter(global float4 *in,                       \n"
"                             global float4 *out,                      \n"
"                             float radius,                            \n"
"                             float preserve)                          \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    int src_width  = get_global_size(0) + (int)ceil(radius) * 2;      \n"
"    int src_height = get_global_size(1) + (int)ceil(radius) * 2;      \n"
"                                                                      \n"
"                                                                      \n"
"    int u,v;                                                          \n"
"    float4 center_pix =                                               \n"
"        in[gidx + (int)ceil(radius)                                   \n"
"           + (gidy + (int)ceil(radius)) * src_width];                 \n"
"    float4 accumulated = 0.0f;                                        \n"
"    float count =0.0f;                                                \n"
"                                                                      \n"
"    for (v = -radius;v <= radius; ++v)                                \n"
"    {                                                                 \n"
"        for (u = -radius;u <= radius; ++u)                            \n"
"        {                                                             \n"
"            int i,j;                                                  \n"
"            i = gidx + radius + u;                                    \n"
"            j = gidy + radius + v;                                    \n"
"            if (i >= 0 && i < src_width &&                            \n"
"                j >= 0 && j < src_height)                             \n"
"            {                                                         \n"
"                int gid1d = i + j * src_width;                        \n"
"                                                                      \n"
"                float diff_map = exp (                                \n"
"                    - (   POW2(center_pix.x - in[gid1d].x)            \n"
"                        + POW2(center_pix.y - in[gid1d].y)            \n"
"                        + POW2(center_pix.z - in[gid1d].z))           \n"
"                    * preserve);                                      \n"
"                                                                      \n"
"                float gaussian_weight;                                \n"
"                float weight;                                         \n"
"                                                                      \n"
"                gaussian_weight =                                     \n"
"                    exp( - 0.5f * (POW2(u) + POW2(v)) / radius);      \n"
"                                                                      \n"
"                weight = diff_map * gaussian_weight;                  \n"
"                                                                      \n"
"                accumulated += in[gid1d] * weight;                    \n"
"                count += weight;                                      \n"
"            }                                                         \n"
"        }                                                             \n"
"    }                                                                 \n"
"    out[gidx + gidy * get_global_size(0)] = accumulated / count;      \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
bilateral_filter_cl(GeglBuffer  *src,
                    const GeglRectangle  *src_rect,
                    GeglBuffer  *dst,
                    const GeglRectangle  *dst_rect,
                    gdouble              radius,
                    gdouble              preserve)
{
    const Babl   * in_format = babl_format("RGBA float");
    const Babl   *out_format = babl_format("RGBA float");

    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
#include "gegl-cl-operation-area-filter-fw1.h"
    ///////////////////////////////////////////////////////////////////////////
    /* Algorithm specific processing flow.
       Build kernels, setting parameters, and running them. */

    if (!cl_data)
    {
        const char *kernel_name[] = {"bilateral_filter", NULL};
        cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;

    cl_float f_radius   = (cl_float)radius;
    cl_float f_preserve = (cl_float)preserve;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 0, sizeof(cl_mem) ,  (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 1, sizeof(cl_mem),   (void*)&dst_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 2, sizeof(cl_float), (void*)&f_radius));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 3, sizeof(cl_float), (void*)&f_preserve));

    CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
        gegl_cl_get_command_queue(), cl_data->kernel[0],
        2, NULL,
        gbl_size, NULL,
        0, NULL, NULL));

    errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
    if (CL_SUCCESS != errcode) CL_ERROR;

    ///////////////////////////////////////////////////////////////////////////
    /* AreaFilter general processing flow.
       Making the necessary color space conversion and Saving data. */
#include "gegl-cl-operation-area-filter-fw2.h"
}

static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class  = GEGL_OPERATION_CLASS (klass);
  filter_class     = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process   = process;
  if (gegl_cl_is_opencl_available())  
      operation_class->prepare = prepare_cl;
  else
      operation_class->prepare = prepare;

  operation_class->name        = "gegl:bilateral-filter";
  operation_class->opencl_support = TRUE;
  operation_class->categories  = "misc";
  operation_class->description =
        _("An edge preserving blur filter that can be used for noise reduction. "
          "It is a gaussian blur where the contribution of neighbourhood pixels "
          "are weighted by the color difference from the center pixel.");
}

#endif
