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

gegl_chant_double (radius, _("Radius"), 0.0, 70.0, 8.0,
  _("Radius of square pixel region, (width and height will be radius*2+1)"))
gegl_chant_int (pairs, _("Pairs"), 1, 2, 2,
  _("Number of pairs; higher number preserves more acute features"))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "snn-mean.c"

#include "gegl-chant.h"
#include <math.h>

static void
snn_mean (GeglBuffer          *src,
          GeglBuffer          *dst,
          const GeglRectangle *dst_rect,
          gdouble              radius,
          gint                 pairs);

static void
snn_mean_cl(GeglBuffer  *src,
            const GeglRectangle  *src_rect,
            GeglBuffer  *dst,
            const GeglRectangle  *dst_rect,
            const float dradius,
            const int   pairs);

static void prepare (GeglOperation *operation)
{
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

  area->left = area->right = area->top = area->bottom = ceil (o->radius);
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

static void prepare_cl (GeglOperation *operation)
{
    GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
    GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

    area->left = area->right = area->top = area->bottom = ceil (o->radius);
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
  GeglChantO          *o = GEGL_CHANT_PROPERTIES (operation);
  GeglBuffer          *temp_in;
  GeglRectangle        compute;

  compute  = gegl_operation_get_required_for_output (
                   operation, "input", result);

  if (o->radius < 1.0)
    {
      output = g_object_ref (input);
    }
  else
    {
      temp_in = gegl_buffer_create_sub_buffer (input, &compute);

      if (gegl_cl_is_opencl_available())
          snn_mean_cl(temp_in, &compute, output, result, o->radius, o->pairs);
      else
      snn_mean (temp_in, output, result, o->radius, o->pairs);

      g_object_unref (temp_in);
    }

  return  TRUE;
}

#define RGB_LUMINANCE_RED    (0.212671)
#define RGB_LUMINANCE_GREEN  (0.715160)
#define RGB_LUMINANCE_BLUE   (0.072169)

static inline gfloat rgb2luminance (gfloat *pix)
{
  return pix[0] * RGB_LUMINANCE_RED +
         pix[1] * RGB_LUMINANCE_GREEN +
         pix[2] * RGB_LUMINANCE_BLUE;
}

#define POW2(a)((a)*(a))

static inline gfloat colordiff (gfloat *pixA,
                                gfloat *pixB)
{
  return POW2(pixA[0]-pixB[0])+
         POW2(pixA[1]-pixB[1])+
         POW2(pixA[2]-pixB[2]);
}


static void
snn_mean (GeglBuffer          *src,
          GeglBuffer          *dst,
          const GeglRectangle *dst_rect,
          gdouble              dradius,
          gint                 pairs)
{
  gint x,y;
  gint offset;
  gfloat *src_buf;
  gfloat *dst_buf;
  gint radius = dradius;
  gint src_width = gegl_buffer_get_width (src);
  gint src_height = gegl_buffer_get_height (src);

  src_buf = g_new0 (gfloat, gegl_buffer_get_pixel_count (src) * 4);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, NULL, babl_format ("RGBA float"), src_buf, GEGL_AUTO_ROWSTRIDE);

  offset = 0;

  for (y=0; y<dst_rect->height; y++)
    {
      gfloat *center_pix;

      center_pix = src_buf + ((radius) + (y+radius)* src_width)*4;

      for (x=0; x<dst_rect->width; x++)
        {
          gint u,v;

          gfloat  accumulated[4]={0,};
          gint    count=0;

          /* iterate through the upper left quater of pixels */
          for (v=-radius;v<=0;v++)
            for (u=-radius;u<= (pairs==1?radius:0);u++)
              {
                gfloat *selected_pix = center_pix;
                gfloat  best_diff = 1000.0;
                gint    i;

                /* skip computations for the center pixel */
                if (u != 0 &&
                    v != 0)
                  {
                    /* compute the coordinates of the symmetric pairs for
                     * this locaiton in the quadrant
                     */
                    gint xs[4], ys[4];

                    xs[0] = x+u+radius;
                    xs[1] = x-u+radius;
                    xs[2] = x-u+radius;
                    xs[3] = x+u+radius;
                    ys[0] = y+v+radius;
                    ys[1] = y-v+radius;
                    ys[2] = y+v+radius;
                    ys[3] = y-v+radius;

                    /* check which member of the symmetric quadruple to use */
                    for (i=0;i<pairs*2;i++)
                      {
                        if (xs[i] >= 0 && xs[i] < src_width &&
                            ys[i] >= 0 && ys[i] < src_height)
                          {
                            gfloat *tpix = src_buf + (xs[i]+ys[i]* src_width)*4;
                            gfloat diff = colordiff (tpix, center_pix);
                            if (diff < best_diff)
                              {
                                best_diff = diff;
                                selected_pix = tpix;
                              }
                          }
                      }
                  }

                /* accumulate the components of the best sample from
                 * the symmetric quadruple
                 */
                for (i=0;i<4;i++)
                  {
                    accumulated[i] += selected_pix[i];
                  }
                count++;

                if (u==0 && v==0)
                  break; /* to avoid doubly processing when using only 1 pair */
              }
          for (u=0; u<4; u++)
            dst_buf[offset*4+u] = accumulated[u]/count;
          offset++;

          center_pix += 4;
        }
    }
  gegl_buffer_set (dst, dst_rect, babl_format ("RGBA float"), dst_buf,
                   GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"float colordiff (float4 pixA,                                         \n"
"                 float4 pixB)                                         \n"
"{                                                                     \n"
"    float4 pix = pixA-pixB;                                           \n"
"    pix *= pix;                                                       \n"
"    return pix.x+pix.y+pix.z;                                         \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void snn_mean_CL (__global const   float4 *src_buf,          \n"
"                                            int src_width,            \n"
"                                            int src_height,           \n"
"                           __global         float4 *dst_buf,          \n"
"                                            int radius,               \n"
"                                            int pairs)                \n"
"{                                                                     \n"
"    int gidx   =get_global_id(0);                                     \n"
"    int gidy   =get_global_id(1);                                     \n"
"    int offset =gidy * get_global_size(0) + gidx;                     \n"
"                                                                      \n"
"    __global const float4 *center_pix=                                \n"
"        src_buf + ((radius+gidx) + (gidy+radius)* src_width);         \n"
"    float4 accumulated=0;                                             \n"
"                                                                      \n"
"    int count=0;                                                      \n"
"    if(pairs==2)                                                      \n"
"    {                                                                 \n"
"        for(int i=-radius;i<0;i++)                                    \n"
"        {                                                             \n"
"            for(int j=-radius;j<0;j++)                                \n"
"            {                                                         \n"
"                __global const float4 *selected_pix = center_pix;     \n"
"                float  best_diff = 1000.0f;                           \n"
"                                                                      \n"
"                    int xs[4]={                                       \n"
"                        gidx+j+radius, gidx-j+radius,                 \n"
"                        gidx-j+radius, gidx+j+radius                  \n"
"                    };                                                \n"
"                    int ys[4]={                                       \n"
"                        gidy+i+radius, gidy-i+radius,                 \n"
"                        gidy+i+radius, gidy-i+radius};                \n"
"                                                                      \n"
"                    for (int k=0;k<4;k++)                             \n"
"                    {                                                 \n"
"                        if (xs[k] >= 0 && xs[k] < src_width &&        \n"
"                            ys[k] >= 0 && ys[k] < src_height)         \n"
"                        {                                             \n"
"                            __global const float4 *tpix =             \n"
"                                src_buf + (xs[k] + ys[k] * src_width);\n"
"                            float diff=colordiff(*tpix, *center_pix); \n"
"                            if (diff < best_diff)                     \n"
"                            {                                         \n"
"                                best_diff = diff;                     \n"
"                                selected_pix = tpix;                  \n"
"                            }                                         \n"
"                        }                                             \n"
"                    }                                                 \n"
"                                                                      \n"
"                accumulated += *selected_pix;                         \n"
"                                                                      \n"
"                ++count;                                              \n"
"                if (i==0 && j==0)                                     \n"
"                    break;                                            \n"
"            }                                                         \n"
"        }                                                             \n"
"        dst_buf[offset] = accumulated/count;                          \n"
"        return;                                                       \n"
"    }                                                                 \n"
"    else if(pairs==1)                                                 \n"
"    {                                                                 \n"
"        for(int i=-radius;i<=0;i++)                                   \n"
"        {                                                             \n"
"            for(int j=-radius;j<=radius;j++)                          \n"
"            {                                                         \n"
"                __global const float4 *selected_pix = center_pix;     \n"
"                float  best_diff = 1000.0f;                           \n"
"                                                                      \n"
"                /* skip computations for the center pixel */          \n"
"                if (i != 0 && j != 0)                                 \n"
"                {                                                     \n"
"                    int xs[4]={                                       \n"
"                        gidx+i+radius, gidx-i+radius,                 \n"
"                        gidx-i+radius, gidx+i+radius                  \n"
"                    };                                                \n"
"                    int ys[4]={                                       \n"
"                        gidy+j+radius, gidy-j+radius,                 \n"
"                        gidy+j+radius, gidy-j+radius                  \n"
"                    };                                                \n"
"                                                                      \n"
"                    for (i=0;i<2;i++)                                 \n"
"                    {                                                 \n"
"                        if (xs[i] >= 0 && xs[i] < src_width &&        \n"
"                            ys[i] >= 0 && ys[i] < src_height)         \n"
"                        {                                             \n"
"                            __global const float4 *tpix =             \n"
"                                src_buf + (xs[i] + ys[i] * src_width);\n"
"                            float diff=colordiff (*tpix, *center_pix);\n"
"                            if (diff < best_diff)                     \n"
"                            {                                         \n"
"                                best_diff = diff;                     \n"
"                                selected_pix = tpix;                  \n"
"                            }                                         \n"
"                        }                                             \n"
"                    }                                                 \n"
"                }                                                     \n"
"                accumulated += *selected_pix;                         \n"
"                ++count;                                              \n"
"                if (i==0 && j==0)                                     \n"
"                    break;                                            \n"
"            }                                                         \n"
"        }                                                             \n"
"        dst_buf[offset] = accumulated/count;                          \n"
"        return;                                                       \n"
"    }                                                                 \n"
"    return;                                                           \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
snn_mean_cl(GeglBuffer  *src,
            const GeglRectangle  *src_rect,
            GeglBuffer  *dst,
            const GeglRectangle  *dst_rect,
            const float dradius,
            const int   pairs)
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
            "snn_mean_CL", NULL};
            cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;

    cl_int cl_src_width  = src_rect->width;
    cl_int cl_src_height = src_rect->height;
    cl_int cl_radius     = (int)dradius;
    cl_int cl_pairs      = pairs;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 1, sizeof(cl_int), (void*)&cl_src_width));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 2, sizeof(cl_int), (void*)&cl_src_height));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 3, sizeof(cl_mem), (void*)&dst_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 4, sizeof(cl_int), (void*)&cl_radius));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 5, sizeof(cl_int), (void*)&cl_pairs));

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

  operation_class->name        = "gegl:snn-mean";
  operation_class->opencl_support = TRUE;
  operation_class->categories  = "misc";
  operation_class->description =
        _("Noise reducing edge enhancing blur filter based "
          " on Symmetric Nearest Neighbours");
}

#endif
