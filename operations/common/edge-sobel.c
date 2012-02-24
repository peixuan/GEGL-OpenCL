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
 */

/*
 * Copyright 2011 Victor Oliveira <victormatheus@gmail.com>
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"

#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_boolean (horizontal,  _("Horizontal"),  TRUE,
                    _("Horizontal"))

gegl_chant_boolean (vertical,  _("Vertical"),  TRUE,
                    _("Vertical"))

gegl_chant_boolean (keep_signal,  _("Keep Signal"),  TRUE,
                    _("Keep Signal"))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "edge-sobel.c"

#include "gegl-chant.h"
#include <math.h>

#define SOBEL_RADIUS 1

static void
edge_sobel (GeglBuffer          *src,
            const GeglRectangle *src_rect,
            GeglBuffer          *dst,
            const GeglRectangle *dst_rect,
            gboolean            horizontal,
            gboolean            vertical,
            gboolean            keep_signal);

static void
edge_sobel_cl(GeglBuffer          *src,
              const GeglRectangle *src_rect,
              GeglBuffer          *dst,
              const GeglRectangle *dst_rect,
              gboolean            horizontal,
              gboolean            vertical,
              gboolean            keep_signal);

#include <stdio.h>

static void prepare (GeglOperation *operation)
{
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  //GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

  area->left = area->right = area->top = area->bottom = SOBEL_RADIUS;
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static void prepare_cl (GeglOperation *operation)
{
    GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
    //GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

    area->left = area->right = area->top = area->bottom = SOBEL_RADIUS;

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

  if (gegl_cl_is_opencl_available())
      edge_sobel_cl(input, &compute, output, result, o->horizontal, o->vertical, o->keep_signal);
  else
  edge_sobel (input, &compute, output, result, o->horizontal, o->vertical, o->keep_signal);

  return  TRUE;
}

inline static gfloat
RMS(gfloat a, gfloat b)
{
  return sqrtf(a*a+b*b);
}

static void
edge_sobel (GeglBuffer          *src,
            const GeglRectangle *src_rect,
            GeglBuffer          *dst,
            const GeglRectangle *dst_rect,
            gboolean            horizontal,
            gboolean            vertical,
            gboolean            keep_signal)
{

  gint x,y;
  gint offset;
  gfloat *src_buf;
  gfloat *dst_buf;

  gint src_width = src_rect->width;

  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RGBA float"), src_buf, GEGL_AUTO_ROWSTRIDE);

  offset = 0;

  for (y=0; y<dst_rect->height; y++)
    for (x=0; x<dst_rect->width; x++)
      {

        gfloat hor_grad[3] = {0.0f, 0.0f, 0.0f};
        gfloat ver_grad[3] = {0.0f, 0.0f, 0.0f};
        gfloat gradient[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        gfloat *center_pix = src_buf + ((x+SOBEL_RADIUS)+((y+SOBEL_RADIUS) * src_width)) * 4;

        gint c;

        if (horizontal)
          {
            gint i=x+SOBEL_RADIUS, j=y+SOBEL_RADIUS;
            gfloat *src_pix = src_buf + (i + j * src_width) * 4;

            for (c=0;c<3;c++)
                hor_grad[c] +=
                    -1.0f*src_pix[c-4-src_width*4]+ src_pix[c+4-src_width*4] +
                    -2.0f*src_pix[c-4] + 2.0f*src_pix[c+4] +
                    -1.0f*src_pix[c-4+src_width*4]+ src_pix[c+4+src_width*4];
          }

        if (vertical)
          {
            gint i=x+SOBEL_RADIUS, j=y+SOBEL_RADIUS;
            gfloat *src_pix = src_buf + (i + j * src_width) * 4;

            for (c=0;c<3;c++)
                ver_grad[c] +=
                  -1.0f*src_pix[c-4-src_width*4]-2.0f*src_pix[c-src_width*4]-1.0f*src_pix[c+4-src_width*4] +
                  src_pix[c-4+src_width*4]+2.0f*src_pix[c+src_width*4]+     src_pix[c+4+src_width*4];
        }

        if (horizontal && vertical)
          {
            for (c=0;c<3;c++)
              // normalization to [0, 1]
              gradient[c] = RMS(hor_grad[c],ver_grad[c])/1.41f;
          }
        else
          {
            if (keep_signal)
              {
                for (c=0;c<3;c++)
                  gradient[c] = hor_grad[c]+ver_grad[c];
              }
            else
              {
                for (c=0;c<3;c++)
                  gradient[c] = fabsf(hor_grad[c]+ver_grad[c]);
              }
          }

        //alpha
        gradient[3] = center_pix[3];

        for (c=0; c<4;c++)
          dst_buf[offset*4+c] = gradient[c];

        offset++;
      }

  gegl_buffer_set (dst, dst_rect, babl_format ("RGBA float"), dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"#define SOBEL_RADIUS 1                                                \n"
"kernel void kernel_edgesobel(global float4 *in,                       \n"
"                             global float4 *out,                      \n"
"                             const int horizontal,                    \n"
"                             const int vertical,                      \n"
"                             const int keep_signal)                   \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    float4 hor_grad = 0.0f;                                           \n"
"    float4 ver_grad = 0.0f;                                           \n"
"    float4 gradient = 0.0f;                                           \n"
"                                                                      \n"
"    int dst_width = get_global_size(0);                               \n"
"    int src_width = dst_width + SOBEL_RADIUS * 2;                     \n"
"                                                                      \n"
"    int i = gidx + SOBEL_RADIUS, j = gidy + SOBEL_RADIUS;             \n"
"    int gid1d = i + j * src_width;                                    \n"
"                                                                      \n"
"    float4 pix_fl = in[gid1d - 1 - src_width];                        \n"
"    float4 pix_fm = in[gid1d     - src_width];                        \n"
"    float4 pix_fr = in[gid1d + 1 - src_width];                        \n"
"    float4 pix_ml = in[gid1d - 1            ];                        \n"
"    float4 pix_mm = in[gid1d                ];                        \n"
"    float4 pix_mr = in[gid1d + 1            ];                        \n"
"    float4 pix_bl = in[gid1d - 1 + src_width];                        \n"
"    float4 pix_bm = in[gid1d     + src_width];                        \n"
"    float4 pix_br = in[gid1d + 1 + src_width];                        \n"
"                                                                      \n"
"    if (horizontal)                                                   \n"
"    {                                                                 \n"
"        hor_grad +=                                                   \n"
"            - 1.0f * pix_fl + 1.0f * pix_fr                           \n"
"            - 2.0f * pix_ml + 2.0f * pix_mr                           \n"
"            - 1.0f * pix_bl + 1.0f * pix_br;                          \n"
"    }                                                                 \n"
"    if (vertical)                                                     \n"
"    {                                                                 \n"
"        ver_grad +=                                                   \n"
"            - 1.0f * pix_fl - 2.0f * pix_fm                           \n"
"            - 1.0f * pix_fr + 1.0f * pix_bl                           \n"
"            + 2.0f * pix_bm + 1.0f * pix_br;                          \n"
"    }                                                                 \n"
"                                                                      \n"
"    if (horizontal && vertical)                                       \n"
"    {                                                                 \n"
"        gradient = sqrt(                                              \n"
"            hor_grad * hor_grad +                                     \n"
"            ver_grad * ver_grad) / 1.41f;                             \n"
"    }                                                                 \n"
"    else                                                              \n"
"    {                                                                 \n"
"        if (keep_signal)                                              \n"
"            gradient = hor_grad + ver_grad;                           \n"
"        else                                                          \n"
"            gradient = fabs(hor_grad + ver_grad);                     \n"
"    }                                                                 \n"
"                                                                      \n"
"    gradient.w = pix_mm.w;                                            \n"
"                                                                      \n"
"    out[gidx + gidy * dst_width] = gradient;                          \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
edge_sobel_cl(GeglBuffer          *src,
              const GeglRectangle *src_rect,
              GeglBuffer          *dst,
              const GeglRectangle *dst_rect,
              gboolean            horizontal,
              gboolean            vertical,
              gboolean            keep_signal)
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
        const char *kernel_name[] = {"kernel_edgesobel", NULL};
        cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;
    
    cl_int n_horizontal  = horizontal;
    cl_int n_vertical    = vertical;
    cl_int n_keep_signal = keep_signal;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 0, sizeof(cl_mem) ,(void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 1, sizeof(cl_mem), (void*)&dst_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 2, sizeof(cl_int), (void*)&n_horizontal));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 3, sizeof(cl_int), (void*)&n_vertical));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 4, sizeof(cl_int), (void*)&n_keep_signal));

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

  operation_class->name        = "gegl:edge-sobel";
  operation_class->opencl_support = TRUE;
  operation_class->categories  = "edge-detect";
  operation_class->description =
        _("Specialized direction-dependent edge detection");
}

#endif
