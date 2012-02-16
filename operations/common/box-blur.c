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

#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (radius, _("Radius"), 0.0, 200.0, 4.0,
   _("Radius of square pixel region, (width and height will be radius*2+1)."))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "box-blur.c"

#include "gegl-chant.h"
#include <stdio.h>
#include <math.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"

#ifdef USE_DEAD_CODE
static inline float
get_mean_component (gfloat *buf,
                    gint    buf_width,
                    gint    buf_height,
                    gint    x0,
                    gint    y0,
                    gint    width,
                    gint    height,
                    gint    component)
{
  gint    x, y;
  gdouble acc=0;
  gint    count=0;

  gint offset = (y0 * buf_width + x0) * 4 + component;

  for (y=y0; y<y0+height; y++)
    {
    for (x=x0; x<x0+width; x++)
      {
        if (x>=0 && x<buf_width &&
            y>=0 && y<buf_height)
          {
            acc += buf [offset];
            count++;
          }
        offset+=4;
      }
      offset+= (buf_width * 4) - 4 * width;
    }
   if (count)
     return acc/count;
   return 0.0;
}
#endif

static inline void
get_mean_components (gfloat *buf,
                     gint    buf_width,
                     gint    buf_height,
                     gint    x0,
                     gint    y0,
                     gint    width,
                     gint    height,
                     gfloat *components)
{
  gint    y;
  gdouble acc[4]={0,0,0,0};
  gint    count[4]={0,0,0,0};

  gint offset = (y0 * buf_width + x0) * 4;

  for (y=y0; y<y0+height; y++)
    {
    gint x;
    for (x=x0; x<x0+width; x++)
      {
        if (x>=0 && x<buf_width &&
            y>=0 && y<buf_height)
          {
            gint c;
            for (c=0;c<4;c++)
              {
                acc[c] += buf [offset+c];
                count[c]++;
              }
          }
        offset+=4;
      }
      offset+= (buf_width * 4) - 4 * width;
    }
    {
      gint c;
      for (c=0;c<4;c++)
        {
         if (count[c])
           components[c] = acc[c]/count[c];
         else
           components[c] = 0.0;
        }
    }
}

/* expects src and dst buf to have the same extent */
static void
hor_blur (GeglBuffer          *src,
          const GeglRectangle *src_rect,
          GeglBuffer          *dst,
          const GeglRectangle *dst_rect,
          gint                 radius)
{
  gint u,v;
  gint offset;
  gfloat *src_buf;
  gfloat *dst_buf;

  /* src == dst for hor blur */
  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RaGaBaA float"), src_buf, GEGL_AUTO_ROWSTRIDE);

  offset = 0;
  for (v=0; v<dst_rect->height; v++)
    for (u=0; u<dst_rect->width; u++)
      {
        gint i;
        gfloat components[4];

        get_mean_components (src_buf,
                             src_rect->width,
                             src_rect->height,
                             u - radius,
                             v,
                             1 + radius*2,
                             1,
                             components);

        for (i=0; i<4; i++)
          dst_buf [offset++] = components[i];
      }

  gegl_buffer_set (dst, dst_rect, babl_format ("RaGaBaA float"), dst_buf, GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}


/* expects dst buf to be radius smaller than src buf */
static void
ver_blur (GeglBuffer          *src,
          const GeglRectangle *src_rect,
          GeglBuffer          *dst,
          const GeglRectangle *dst_rect,
          gint                 radius)
{
  gint u,v;
  gint offset;
  gfloat *src_buf;
  gfloat *dst_buf;

  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RaGaBaA float"), src_buf, GEGL_AUTO_ROWSTRIDE);

  offset=0;
  for (v=0; v<dst_rect->height; v++)
    for (u=0; u<dst_rect->width; u++)
      {
        gfloat components[4];
        gint c;

        get_mean_components (src_buf,
                             src_rect->width,
                             src_rect->height,
                             u + radius,  /* 1x radius is the offset between the bufs */
                             v - radius + radius, /* 1x radius is the offset between the bufs */
                             1,
                             1 + radius * 2,
                             components);

        for (c=0; c<4; c++)
          dst_buf [offset++] = components[c];
      }

  gegl_buffer_set (dst, dst_rect, babl_format ("RaGaBaA float"), dst_buf, GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}

static void
box_blur_cl     (GeglBuffer  *src,
                 const GeglRectangle  *src_rect,
                 GeglBuffer  *dst,
                 const GeglRectangle  *dst_rect,
                 const int   radius);

static void prepare (GeglOperation *operation)
{
  GeglChantO              *o;
  GeglOperationAreaFilter *op_area;

  op_area = GEGL_OPERATION_AREA_FILTER (operation);
  o       = GEGL_CHANT_PROPERTIES (operation);

  op_area->left   =
  op_area->right  =
  op_area->top    =
  op_area->bottom = ceil (o->radius);

  gegl_operation_set_format (operation, "output",
                             babl_format ("RaGaBaA float"));
}

static void prepare_cl (GeglOperation *operation)
{
    GeglChantO              *o;
    GeglOperationAreaFilter *op_area;

    op_area = GEGL_OPERATION_AREA_FILTER (operation);
    o       = GEGL_CHANT_PROPERTIES (operation);

    op_area->left   =
        op_area->right  =
        op_area->top    =
        op_area->bottom = ceil (o->radius);

    GeglNode * self;
    GeglPad *pad;
    Babl * format=babl_format ("RaGaBaA float");
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
  GeglRectangle rect;
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
  GeglBuffer *temp;
  GeglOperationAreaFilter *op_area;
  op_area = GEGL_OPERATION_AREA_FILTER (operation);

  rect = *result;

  rect.x-=op_area->left;
  rect.y-=op_area->top;
  rect.width+=op_area->left + op_area->right;
  rect.height+=op_area->top + op_area->bottom;

  if (gegl_cl_is_opencl_available())
  {
      box_blur_cl(input, &rect, output, result, o->radius);
  }

  temp  = gegl_buffer_new (&rect,
                           babl_format ("RaGaBaA float"));

  hor_blur (input, &rect, temp, &rect, o->radius);
  ver_blur (temp, &rect, output, result, o->radius);

  g_object_unref (temp);
  return  TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"void get_mean_components (__global const float4 *buf,                 \n"
"                          int     buf_width,                          \n"
"                          int     buf_height,                         \n"
"                          int     x0,                                 \n"
"                          int     y0,                                 \n"
"                          int     width,                              \n"
"                          int     height,                             \n"
"                          float4 *components)                         \n"
"{                                                                     \n"
"    int    y;                                                         \n"
"    float4 acc = 0;                                                   \n"
"    int    count=0;                                                   \n"
"                                                                      \n"
"    int offset = (y0 * buf_width + x0);                               \n"
"    if(width == 1)                                                    \n"
"    {                                                                 \n"
"        for (y=y0; y<y0+height; y++)                                  \n"
"        {                                                             \n"
"            int x = x0;                                               \n"
"            if (x>=0 && x<buf_width &&                                \n"
"                y>=0 && y<buf_height)                                 \n"
"            {                                                         \n"
"                acc += buf[offset];                                   \n"
"                ++count;                                              \n"
"            }                                                         \n"
"            offset += buf_width - width + 1;                          \n"
"        }                                                             \n"
"    }                                                                 \n"
"    else if(height ==1)                                               \n"
"    {                                                                 \n"
"        y = y0;                                                       \n"
"        int x;                                                        \n"
"        for (x=x0; x<x0+width; x++)                                   \n"
"        {                                                             \n"
"            if (x>=0 && x<buf_width &&                                \n"
"                y>=0 && y<buf_height)                                 \n"
"            {                                                         \n"
"                acc += buf[offset];                                   \n"
"                ++count;                                              \n"
"            }                                                         \n"
"            offset++;                                                 \n"
"        }                                                             \n"
"    }                                                                 \n"
"    else                                                              \n"
"    {                                                                 \n"
"        for (y=y0; y<y0+height; y++)                                  \n"
"        {                                                             \n"
"            int x;                                                    \n"
"            for (x=x0; x<x0+width; x++)                               \n"
"            {                                                         \n"
"                if (x>=0 && x<buf_width &&                            \n"
"                    y>=0 && y<buf_height)                             \n"
"                {                                                     \n"
"                    acc += buf[offset];                               \n"
"                    ++count;                                          \n"
"                }                                                     \n"
"                offset++;                                             \n"
"            }                                                         \n"
"            offset += buf_width - width;                              \n"
"        }                                                             \n"
"    }                                                                 \n"
"    *components = count > 0 ? acc/count : 0.0f;                       \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void ver_blur_cl(__global const float4*   src_buf,           \n"
"                          int       src_width,                        \n"
"                          int       src_height,                       \n"
"                          __global       float4*   dst_buf,           \n"
"                          int       radius)                           \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    float4 components;                                                \n"
"    get_mean_components(src_buf,                                      \n"
"        src_width,                                                    \n"
"        src_height,                                                   \n"
"        gidx + radius,                                                \n"
"        gidy,                                                         \n"
"        1,                                                            \n"
"        1 + radius * 2,                                               \n"
"        &components);                                                 \n"
"                                                                      \n"
"    dst_buf[gidx + gidy * get_global_size(0)] = components;           \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void hor_blur_cl(__global const float4  *src_buf,            \n"
"                          int      src_width,                         \n"
"                          int      src_height,                        \n"
"                          __global       float4  *dst_buf,            \n"
"                          int      radius)                            \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    float4 components;                                                \n"
"    get_mean_components(src_buf,                                      \n"
"        src_width,                                                    \n"
"        src_height,                                                   \n"
"        gidx - radius,                                                \n"
"        gidy,                                                         \n"
"        1 + radius * 2,                                               \n"
"        1,                                                            \n"
"        &components);                                                 \n"
"                                                                      \n"
"    dst_buf[gidx + gidy * get_global_size(0)] = components;           \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
box_blur_cl     (GeglBuffer  *src,
                 const GeglRectangle  *src_rect,
                 GeglBuffer  *dst,
                 const GeglRectangle  *dst_rect,
                 const int   radius)
{
    const Babl  * in_format = babl_format("RaGaBaA float");
    const Babl  *out_format = babl_format("RaGaBaA float");
    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
#include "gegl-cl-operation-area-filter-fw1.h"
    ///////////////////////////////////////////////////////////////////////////
    /* Algorithm specific processing flow.
       Build kernels, setting parameters, and running them. */

    if (!cl_data)
    {
        const char *kernel_name[] ={
            "hor_blur_cl", "ver_blur_cl", NULL};
            cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;

    cl_int cl_src_width  = src_rect->width;
    cl_int cl_src_height = src_rect->height;
    cl_int cl_radius     = radius;

    size_t gbl_src_size[2] = {src_rect->width, src_rect->height};

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

    CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
        gegl_cl_get_command_queue(), cl_data->kernel[0],
        2, NULL,
        gbl_src_size, NULL,
        0, NULL, NULL));

    errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
    if (CL_SUCCESS != errcode) CL_ERROR;

    cl_mem tmp_mem = dst_mem;
    dst_mem = src_mem;
    src_mem = tmp_mem;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 1, sizeof(cl_int), (void*)&cl_src_width));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 2, sizeof(cl_int), (void*)&cl_src_height));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 3, sizeof(cl_mem), (void*)&dst_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 4, sizeof(cl_int), (void*)&cl_radius));

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

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process    = process;
  if (gegl_cl_is_opencl_available())  
      operation_class->prepare = prepare_cl;
  else
      operation_class->prepare = prepare;

  operation_class->categories  = "blur";
  operation_class->name        = "gegl:box-blur";
  operation_class->opencl_support = TRUE;
  operation_class->description =
       _("Performs an averaging of a square box of pixels.");
}

#endif
