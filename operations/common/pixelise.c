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
#include <math.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"

#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_int (xsize, _("Block Width"), 1, 256, 8,
   _("Width of blocks in pixels"))
gegl_chant_int (ysize, _("Block Height"), 1, 256, 8,
   _("Height of blocks in pixels"))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "pixelise.c"

#include "gegl-chant.h"

#define CELL_X(px, cell_width)  ((px) / (cell_width))
#define CELL_Y(py, cell_height)  ((py) / (cell_height))

static void
pixelise_cl     (GeglBuffer  *src,
                 const GeglRectangle  *src_rect,
                 GeglBuffer  *dst,
                 const GeglRectangle  *dst_rect,
                 const int   xsize,
                 const int   ysize);

static void prepare (GeglOperation *operation)
{
  GeglChantO              *o;
  GeglOperationAreaFilter *op_area;

  op_area = GEGL_OPERATION_AREA_FILTER (operation);
  o       = GEGL_CHANT_PROPERTIES (operation);

  op_area->left   =
  op_area->right  = o->xsize;
  op_area->top    =
  op_area->bottom = o->ysize;

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
        op_area->right  = o->xsize;
    op_area->top    =
        op_area->bottom = o->ysize;

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

static void
calc_block_colors (gfloat* block_colors,
                   const gfloat* input,
                   const GeglRectangle* roi,
                   gint xsize,
                   gint ysize)
{
  gint cx0 = CELL_X(roi->x, xsize);
  gint cy0 = CELL_Y(roi->y, ysize);
  gint cx1 = CELL_X(roi->x + roi->width - 1, xsize);
  gint cy1 = CELL_Y(roi->y + roi->height - 1, ysize);

  gint cx;
  gint cy;
  gfloat weight = 1.0f / (xsize * ysize);
  gint line_width = roi->width + 2*xsize;
  /* loop over the blocks within the region of interest */
  for (cy=cy0; cy<=cy1; ++cy)
    {
      for (cx=cx0; cx<=cx1; ++cx)
        {
          gint px = (cx * xsize) - roi->x + xsize;
          gint py = (cy * ysize) - roi->y + ysize;

          /* calculate the average color for this block */
          gint j,i,c;
          gfloat col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
          for (j=py; j<py+ysize; ++j)
            {
              for (i=px; i<px+xsize; ++i)
                {
                  for (c=0; c<4; ++c)
                    col[c] += input[(j*line_width + i)*4 + c];
                }
            }
          for (c=0; c<4; ++c)
            block_colors[c] = weight * col[c];
          block_colors += 4;
        }
    }
}

static void
pixelise (gfloat* buf,
          const GeglRectangle* roi,
          gint xsize,
          gint ysize)
{
  gint cx0 = CELL_X(roi->x, xsize);
  gint cy0 = CELL_Y(roi->y, ysize);
  gint block_count_x = CELL_X(roi->x + roi->width - 1, xsize) - cx0 + 1;
  gint block_count_y = CELL_Y(roi->y + roi->height - 1, ysize) - cy0 + 1;
  gfloat* block_colors = g_new0 (gfloat, block_count_x * block_count_y * 4);
  gint x;
  gint y;
  gint c;

  /* calculate the average color of all the blocks */
  calc_block_colors(block_colors, buf, roi, xsize, ysize);

  /* set each pixel to the average color of the block it belongs to */
  for (y=0; y<roi->height; ++y)
    {
      gint cy = CELL_Y(y + roi->y, ysize) - cy0;
      for (x=0; x<roi->width; ++x)
        {
          gint cx = CELL_X(x + roi->x, xsize) - cx0;
          for (c=0; c<4; ++c)
            *buf++ = block_colors[(cy*block_count_x + cx)*4 + c];
        }
    }

  g_free (block_colors);
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi)
{
  GeglRectangle src_rect;
  GeglChantO *o = GEGL_CHANT_PROPERTIES (operation);
  GeglOperationAreaFilter *op_area;
  gfloat* buf;

  op_area = GEGL_OPERATION_AREA_FILTER (operation);
  src_rect = *roi;
  src_rect.x -= op_area->left;
  src_rect.y -= op_area->top;
  src_rect.width += op_area->left + op_area->right;
  src_rect.height += op_area->top + op_area->bottom;

  if (gegl_cl_is_opencl_available())
  {
      pixelise_cl(input, &src_rect, output,roi, o->xsize, o->ysize);
      return TRUE;
  }

  buf = g_new0 (gfloat, src_rect.width * src_rect.height * 4);

  gegl_buffer_get (input, 1.0, &src_rect, babl_format ("RaGaBaA float"), buf, GEGL_AUTO_ROWSTRIDE);

  pixelise(buf, roi, o->xsize, o->ysize);

  gegl_buffer_set (output, roi, babl_format ("RaGaBaA float"), buf, GEGL_AUTO_ROWSTRIDE);

  g_free (buf);

  return  TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source =
"kernel void calc_block_color(global float4 *in,                       \n"
"                             global float4 *out,                      \n"
"                             int xsize,                               \n"
"                             int ysize)                               \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"    int src_width  = get_global_size(0);                              \n"
"    int src_height = get_global_size(1) + 2 * xsize;                  \n"
"                                                                      \n"
"    int cx = gidx / xsize;                                            \n"
"    int cy = gidy / ysize;                                            \n"
"                                                                      \n"
"    float weight   = 1.0f / (xsize * ysize);                          \n"
"    int line_width = src_width + 2 * xsize;                           \n"
"                                                                      \n"
"    int px = cx * xsize + xsize;                                      \n"
"    int py = cy * ysize + ysize;                                      \n"
"                                                                      \n"
"    int i,j;                                                          \n"
"    float4 col = 0.0f;                                                \n"
"    for (j = py;j < py +ysize; ++j)                                   \n"
"    {                                                                 \n"
"        for (i = px;i < px + xsize; ++i)                              \n"
"        {                                                             \n"
"            col += in[j * line_width + i];                            \n"
"        }                                                             \n"
"    }                                                                 \n"
"    int block_count_x = (src_width - 1) / xsize +1;                   \n"
"    out[cy * block_count_x + cx] = col * weight;                      \n"
"                                                                      \n"
"}                                                                     \n"
"                                                                      \n"
"kernel void kernel_pixelise (global float4 *in,                       \n"
"                             global float4 *out,                      \n"
"                             int xsize,                               \n"
"                             int ysize)                               \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    int src_width  = get_global_size(0);                              \n"
"    int src_height = get_global_size(1);                              \n"
"                                                                      \n"
"    int cx = gidx / xsize;                                            \n"
"    int cy = gidy / ysize;                                            \n"
"    int block_count_x = (src_width - 1) / xsize +1;                   \n"
"                                                                      \n"
"    out[gidx + gidy * src_width] = in[cy * block_count_x + cx];       \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
pixelise_cl     (GeglBuffer  *src,
                 const GeglRectangle  *src_rect,
                 GeglBuffer  *dst,
                 const GeglRectangle  *dst_rect,
                 const int   xsize,
                 const int   ysize)
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
            "calc_block_color", "kernel_pixelise", NULL};
            cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 1, sizeof(cl_mem), (void*)&dst_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 2, sizeof(cl_int), (void*)&xsize));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[0], 3, sizeof(cl_int), (void*)&ysize));

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
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 2, sizeof(cl_int), (void*)&xsize));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data->kernel[1], 3, sizeof(cl_int), (void*)&ysize));

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
  operation_class->name        = "gegl:pixelise";
  operation_class->opencl_support = TRUE;
  operation_class->description =
       _("Pixelise filter");
}

#endif
