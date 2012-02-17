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

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "edge-laplace.c"

#include "gegl-chant.h"
#include <math.h>

#define LAPLACE_RADIUS 1

static void
edge_laplace (GeglBuffer          *src,
              const GeglRectangle *src_rect,
              GeglBuffer          *dst,
              const GeglRectangle *dst_rect);

static void
edge_laplace_cl(GeglBuffer          *src,
                const GeglRectangle *src_rect,
                GeglBuffer          *dst,
                const GeglRectangle *dst_rect);

#include <stdio.h>

static void prepare (GeglOperation *operation)
{
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  //GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

  area->left = area->right = area->top = area->bottom = LAPLACE_RADIUS;
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static void prepare_cl (GeglOperation *operation)
{
    GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
    //GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

    area->left = area->right = area->top = area->bottom = LAPLACE_RADIUS;
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
  GeglRectangle compute;

  compute = gegl_operation_get_required_for_output (operation, "input", result);

  if (gegl_cl_is_opencl_available())
      edge_laplace_cl (input, &compute, output, result);
  else
  edge_laplace (input, &compute, output, result);

  return  TRUE;
}

static void
minmax  (gfloat  x1,
         gfloat  x2,
         gfloat  x3,
         gfloat  x4,
         gfloat  x5,
         gfloat *min_result,
         gfloat *max_result)
{
  gfloat min1, min2, max1, max2;

  if (x1 > x2)
    {
      max1 = x1;
      min1 = x2;
    }
  else
    {
      max1 = x2;
      min1 = x1;
    }

  if (x3 > x4)
    {
      max2 = x3;
      min2 = x4;
    }
  else
    {
      max2 = x4;
      min2 = x3;
    }

  if (min1 < min2)
    *min_result = fminf (min1, x5);
  else
    *min_result = fminf (min2, x5);

  if (max1 > max2)
    *max_result = fmaxf (max1, x5);
  else
    *max_result = fmaxf (max2, x5);
}


static void
edge_laplace (GeglBuffer          *src,
              const GeglRectangle *src_rect,
              GeglBuffer          *dst,
              const GeglRectangle *dst_rect)
{

  gint x,y;
  gint offset;
  gfloat *src_buf;
  gfloat *temp_buf;
  gfloat *dst_buf;

  gint src_width = src_rect->width;

  src_buf  = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  temp_buf = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  dst_buf  = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, src_rect,
                   babl_format ("RGBA float"), src_buf, GEGL_AUTO_ROWSTRIDE);

  for (y=0; y<dst_rect->height; y++)
    for (x=0; x<dst_rect->width; x++)
      {
        gfloat *src_pix;

        gfloat gradient[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        gint c;

        gfloat minval, maxval;

        gint i=x+LAPLACE_RADIUS, j=y+LAPLACE_RADIUS;
        offset = i + j * src_width;
        src_pix = src_buf + offset * 4;

        for (c=0;c<3;c++)
          {
            minmax (src_pix[c-src_width*4], src_pix[c+src_width*4],
                    src_pix[c-4], src_pix[c+4], src_pix[c],
                    &minval, &maxval); /* four-neighbourhood */

            gradient[c] = 0.5f * fmaxf((maxval-src_pix[c]), (src_pix[c]-minval));

            gradient[c] = (src_pix[c-4-src_width*4] +
                           src_pix[c-src_width*4] +
                           src_pix[c+4-src_width*4] +

                           src_pix[c-4] -8.0f* src_pix[c] +src_pix[c+4] +

                           src_pix[c-4+src_width*4] + src_pix[c+src_width*4] +
                           src_pix[c+4+src_width*4]) > 0.0f?
                          gradient[c] : -1.0f*gradient[c];
        }

        //alpha
        gradient[3] = src_pix[3];

        for (c=0; c<4;c++)
          temp_buf[offset*4+c] = gradient[c];
      }

  //1-pixel edges
  offset = 0;

  for (y=0; y<dst_rect->height; y++)
    for (x=0; x<dst_rect->width; x++)
      {

        gfloat value[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        gint c;

        gint i=x+LAPLACE_RADIUS, j=y+LAPLACE_RADIUS;
        gfloat *src_pix = temp_buf + (i + j * src_width) * 4;

        for (c=0;c<3;c++)
        {
          gfloat current = src_pix[c];
          current = ((current > 0.0f) &&
                     (src_pix[c-4-src_width*4] < 0.0f ||
                      src_pix[c+4-src_width*4] < 0.0f ||
                      src_pix[c  -src_width*4] < 0.0f ||
                      src_pix[c-4+src_width*4] < 0.0f ||
                      src_pix[c+4+src_width*4] < 0.0f ||
                      src_pix[   +src_width*4] < 0.0f ||
                      src_pix[c-4            ] < 0.0f ||
                      src_pix[c+4            ] < 0.0f))?
                    current : 0.0f;

          value[c] = current;
        }

        //alpha
        value[3] = src_pix[3];

        for (c=0; c<4;c++)
          dst_buf[offset*4+c] = value[c];

        offset++;
      }

  gegl_buffer_set (dst, dst_rect, babl_format ("RGBA float"), dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (temp_buf);
  g_free (dst_buf);
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"#define SOBEL_RADIUS 1                                                \n"
"#define LAPLACE_RADIUS 1                                              \n"
"void minmax(float x1, float x2, float x3,                             \n"
"            float x4, float x5,                                       \n"
"            float *min_result,                                        \n"
"            float *max_result)                                        \n"
"{                                                                     \n"
"    float min1, min2, max1, max2;                                     \n"
"                                                                      \n"
"    if (x1 > x2)                                                      \n"
"    {                                                                 \n"
"        max1 = x1;                                                    \n"
"        min1 = x2;                                                    \n"
"    }                                                                 \n"
"    else                                                              \n"
"    {                                                                 \n"
"        max1 = x2;                                                    \n"
"        min1 = x1;                                                    \n"
"    }                                                                 \n"
"                                                                      \n"
"    if (x3 > x4)                                                      \n"
"    {                                                                 \n"
"        max2 = x3;                                                    \n"
"        min2 = x4;                                                    \n"
"    }                                                                 \n"
"    else                                                              \n"
"    {                                                                 \n"
"        max2 = x4;                                                    \n"
"        min2 = x3;                                                    \n"
"    }                                                                 \n"
"                                                                      \n"
"    if (min1 < min2)                                                  \n"
"        *min_result = fmin(min1, x5);                                 \n"
"    else                                                              \n"
"        *min_result = fmin(min2, x5);                                 \n"
"    if (max1 > max2)                                                  \n"
"        *max_result = fmax(max1, x5);                                 \n"
"    else                                                              \n"
"        *max_result = fmax(max2, x5);                                 \n"
"}                                                                     \n"
"                                                                      \n"
"kernel void pre_edgelaplace (global float4 *in,                       \n"
"                             global float4 *out)                      \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    int src_width  = get_global_size(0) + LAPLACE_RADIUS * 2;         \n"
"    int src_height = get_global_size(1);                              \n"
"                                                                      \n"
"    int i = gidx + LAPLACE_RADIUS, j = gidy + LAPLACE_RADIUS;         \n"
"    int gid1d = i + j * src_width;                                    \n"
"                                                                      \n"
"    float pix_fl[4] = {                                               \n"
"        in[gid1d - 1 - src_width].x, in[gid1d - 1 - src_width].y,     \n"
"        in[gid1d - 1 - src_width].z, in[gid1d - 1 - src_width].w      \n"
"    };                                                                \n"
"    float pix_fm[4] = {                                               \n"
"        in[gid1d     - src_width].x, in[gid1d     - src_width].y,     \n"
"        in[gid1d     - src_width].z, in[gid1d     - src_width].w      \n"
"    };                                                                \n"
"    float pix_fr[4] = {                                               \n"
"        in[gid1d + 1 - src_width].x, in[gid1d + 1 - src_width].y,     \n"
"        in[gid1d + 1 - src_width].z, in[gid1d + 1 - src_width].w      \n"
"    };                                                                \n"
"    float pix_ml[4] = {                                               \n"
"        in[gid1d - 1            ].x, in[gid1d - 1            ].y,     \n"
"        in[gid1d - 1            ].z, in[gid1d - 1            ].w      \n"
"    };                                                                \n"
"    float pix_mm[4] = {                                               \n"
"        in[gid1d                ].x, in[gid1d                ].y,     \n"
"        in[gid1d                ].z, in[gid1d                ].w      \n"
"    };                                                                \n"
"    float pix_mr[4] = {                                               \n"
"        in[gid1d + 1            ].x, in[gid1d + 1            ].y,     \n"
"        in[gid1d + 1            ].z, in[gid1d + 1            ].w      \n"
"    };                                                                \n"
"    float pix_bl[4] = {                                               \n"
"        in[gid1d - 1 + src_width].x, in[gid1d - 1 + src_width].y,     \n"
"        in[gid1d - 1 + src_width].z, in[gid1d - 1 + src_width].w      \n"
"    };                                                                \n"
"    float pix_bm[4] = {                                               \n"
"        in[gid1d     + src_width].x, in[gid1d     + src_width].y,     \n"
"        in[gid1d     + src_width].z, in[gid1d     + src_width].w      \n"
"    };                                                                \n"
"    float pix_br[4] = {                                               \n"
"        in[gid1d + 1 + src_width].x, in[gid1d + 1 + src_width].y,     \n"
"        in[gid1d + 1 + src_width].z, in[gid1d + 1 + src_width].w      \n"
"    };                                                                \n"
"                                                                      \n"
"    int c;                                                            \n"
"    float minval, maxval;                                             \n"
"    float gradient[4];                                                \n"
"                                                                      \n"
"    for (c = 0;c < 3; ++c)                                            \n"
"    {                                                                 \n"
"        minmax(pix_fm[c], pix_bm[c], pix_ml[c], pix_mr[c],            \n"
"            pix_mm[c], &minval, &maxval);                             \n"
"        gradient[c] = 0.5f *                                          \n"
"            fmax((maxval - pix_mm[c]),(pix_mm[c] - minval));          \n"
"        gradient[c] =                                                 \n"
"            (pix_fl[c] + pix_fm[c] + pix_fr[c] +                      \n"
"             pix_ml[c] + pix_mr[c] + pix_bl[c] +                      \n"
"             pix_bm[c] + pix_br[c] - 8.0f * pix_mm[c]) >              \n"
"             0.0f ? gradient[c] : -1.0f * gradient[c];                \n"
"    }                                                                 \n"
"    gradient[3] = pix_mm[3];                                          \n"
"                                                                      \n"
"    out[gid1d] = (float4)                                             \n"
"        (gradient[0], gradient[1], gradient[2], gradient[3]);         \n"
"}                                                                     \n"
"                                                                      \n"
"kernel void knl_edgelaplace (global float4 *in,                       \n"
"                             global float4 *out)                      \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    int src_width  = get_global_size(0) + LAPLACE_RADIUS * 2;         \n"
"    int src_height = get_global_size(1);                              \n"
"                                                                      \n"
"    int i = gidx + LAPLACE_RADIUS, j = gidy + LAPLACE_RADIUS;         \n"
"    int gid1d = i + j * src_width;                                    \n"
"                                                                      \n"
"    float pix_fl[4] = {                                               \n"
"        in[gid1d - 1 - src_width].x, in[gid1d - 1 - src_width].y,     \n"
"        in[gid1d - 1 - src_width].z, in[gid1d - 1 - src_width].w      \n"
"    };                                                                \n"
"    float pix_fm[4] = {                                               \n"
"        in[gid1d     - src_width].x, in[gid1d     - src_width].y,     \n"
"        in[gid1d     - src_width].z, in[gid1d     - src_width].w      \n"
"    };                                                                \n"
"    float pix_fr[4] = {                                               \n"
"        in[gid1d + 1 - src_width].x, in[gid1d + 1 - src_width].y,     \n"
"        in[gid1d + 1 - src_width].z, in[gid1d + 1 - src_width].w      \n"
"    };                                                                \n"
"    float pix_ml[4] = {                                               \n"
"        in[gid1d - 1            ].x, in[gid1d - 1            ].y,     \n"
"        in[gid1d - 1            ].z, in[gid1d - 1            ].w      \n"
"    };                                                                \n"
"    float pix_mm[4] = {                                               \n"
"        in[gid1d                ].x, in[gid1d                ].y,     \n"
"        in[gid1d                ].z, in[gid1d                ].w      \n"
"    };                                                                \n"
"    float pix_mr[4] = {                                               \n"
"        in[gid1d + 1            ].x, in[gid1d + 1            ].y,     \n"
"        in[gid1d + 1            ].z, in[gid1d + 1            ].w      \n"
"    };                                                                \n"
"    float pix_bl[4] = {                                               \n"
"        in[gid1d - 1 + src_width].x, in[gid1d - 1 + src_width].y,     \n"
"        in[gid1d - 1 + src_width].z, in[gid1d - 1 + src_width].w      \n"
"    };                                                                \n"
"    float pix_bm[4] = {                                               \n"
"        in[gid1d     + src_width].x, in[gid1d     + src_width].y,     \n"
"        in[gid1d     + src_width].z, in[gid1d     + src_width].w      \n"
"    };                                                                \n"
"    float pix_br[4] = {                                               \n"
"        in[gid1d + 1 + src_width].x, in[gid1d + 1 + src_width].y,     \n"
"        in[gid1d + 1 + src_width].z, in[gid1d + 1 + src_width].w      \n"
"    };                                                                \n"
"                                                                      \n"
"    int c;                                                            \n"
"    float value[4];                                                   \n"
"                                                                      \n"
"    for (c = 0;c < 3; ++c)                                            \n"
"    {                                                                 \n"
"        float current = pix_mm[c];                                    \n"
"        current =                                                     \n"
"            ((current > 0.0f) &&                                      \n"
"             (pix_fl[c] < 0.0f || pix_fm[c] < 0.0f ||                 \n"
"              pix_fr[c] < 0.0f || pix_ml[c] < 0.0f ||                 \n"
"              pix_mr[c] < 0.0f || pix_bl[c] < 0.0f ||                 \n"
"              pix_bm[c] < 0.0f || pix_br[c] < 0.0f )                  \n"
"            ) ? current : 0.0f;                                       \n"
"        value[c] = current;                                           \n"
"    }                                                                 \n"
"    value[3] = pix_mm[3];                                             \n"
"                                                                      \n"
"    out[gidx + gidy * get_global_size(0)] = (float4)                  \n"
"        (value[0], value[1], value[2], value[3]);                     \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
edge_laplace_cl(GeglBuffer          *src,
                const GeglRectangle *src_rect,
                GeglBuffer          *dst,
                const GeglRectangle *dst_rect)
{
    const Babl  * in_format = babl_format("RGBA float");
    const Babl  *out_format = babl_format("RGBA float");
    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
#include "gegl-cl-operation-area-filter-fw1.h"
    ///////////////////////////////////////////////////////////////////////////
    /* Algorithm specific processing flow.
       Build kernels, setting parameters, and running them. */

    if (!cl_data)
    {
        const char *kernel_name[] ={
            "pre_edgelaplace", "knl_edgelaplace", NULL};
            cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 1, sizeof(cl_mem), (void*)&dst_mem));

    CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
        gegl_cl_get_command_queue(), cl_data->kernel[0],
        2, NULL,
        gbl_size, NULL,
        0, NULL, NULL));

    errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
    if (CL_SUCCESS != errcode) CL_ERROR;

    cl_mem tmp_mem = dst_mem;
    dst_mem = src_mem;
    src_mem = tmp_mem;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 1, sizeof(cl_mem), (void*)&dst_mem));

    CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
        gegl_cl_get_command_queue(), cl_data->kernel[1],
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

  operation_class->name        = "gegl:edge-laplace";
  operation_class->opencl_support = TRUE;
  operation_class->categories  = "edge-detect";
  operation_class->description =
        _("High-resolution edge detection");
}

#endif
