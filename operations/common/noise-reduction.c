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
 * Ali Alsam, Hans Jakob Rivertz, Øyvind Kolås (c) 2011
 */

#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_int (iterations, "Strength", 1, 32, 4, "How many iteratarion to run the algorithm.")

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "noise-reduction.c"

#include "gegl-chant.h"
#include <math.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"

static void
noise_reduct_cl (GeglBuffer  *src,
                 const GeglRectangle  *src_rect,
                 GeglBuffer  *dst,
                 const GeglRectangle  *dst_rect,
                 const int   iterations);

/* The core noise_reduction function, which is implemented as
 * portable C - this is the function where most cpu time goes
 */
static void
noise_reduction (float *src_buf,     /* source buffer, one pixel to the left
                                        and up from the starting pixel */
                 int    src_stride,  /* stridewidth of buffer in pixels */
                 float *dst_buf,     /* destination buffer */
                 int    dst_width,   /* width to render */
                 int    dst_height,  /* height to render */
                 int    dst_stride)  /* stride of target buffer */
{
  int c;
  int x,y;
  int dst_offset;

#define NEIGHBOURS 8
#define AXES       (NEIGHBOURS/2)

#define POW2(a) ((a)*(a))
/* core code/formulas to be tweaked for the tuning the implementation */
#define GEN_METRIC(before, center, after) \
                   POW2((center) * 2 - (before) - (after))

/* Condition used to bail diffusion from a direction */
#define BAIL_CONDITION(new,original) ((new) > (original))

#define SYMMETRY(a)  (NEIGHBOURS - (a) - 1) /* point-symmetric neighbour pixel */

#define O(u,v) (((u)+((v) * src_stride)) * 4)
  int   offsets[NEIGHBOURS] = {  /* array of the relative distance i float
                                  * pointers to each of neighbours
                                  * in source buffer, allows quick referencing.
                                  */
              O( -1, -1), O(0, -1), O(1, -1),
              O( -1,  0),           O(1,  0),
              O( -1,  1), O(0, 1),  O(1,  1)};
#undef O

  dst_offset = 0;
  for (y=0; y<dst_height; y++)
    {
      float *center_pix = src_buf + ((y+1) * src_stride + 1) * 4;
      dst_offset = dst_stride * y;
      for (x=0; x<dst_width; x++)
        {
          for (c=0; c<3; c++) /* do each color component individually */
            {
              float  metric_reference[AXES];
              int    axis;
              int    direction;
              float  sum;
              int    count;

              for (axis = 0; axis < AXES; axis++)
                { /* initialize original metrics for the horizontal, vertical
                     and 2 diagonal metrics */
                  float *before_pix  = center_pix + offsets[axis];
                  float *after_pix   = center_pix + offsets[SYMMETRY(axis)];

                  metric_reference[axis] =
                    GEN_METRIC (before_pix[c], center_pix[c], after_pix[c]);
                }

              sum   = center_pix[c];
              count = 1;

              /* try smearing in data from all neighbours */
              for (direction = 0; direction < NEIGHBOURS; direction++)
                {
                  float *pix   = center_pix + offsets[direction];
                  float  value = pix[c] * 0.5 + center_pix[c] * 0.5;
                  int    axis;
                  int    valid;

                  /* check if the non-smoothing operating check is true if
                   * smearing from this direction for any of the axes */
                  valid = 1; /* assume it will be valid */
                  for (axis = 0; axis < AXES; axis++)
                    {
                      float *before_pix = center_pix + offsets[axis];
                      float *after_pix  = center_pix + offsets[SYMMETRY(axis)];
                      float  metric_new =
                             GEN_METRIC (before_pix[c], value, after_pix[c]);

                      if (BAIL_CONDITION(metric_new, metric_reference[axis]))
                        {
                          valid = 0; /* mark as not a valid smoothing, and .. */
                          break;     /* .. break out of loop */
                        }
                    }
                  if (valid) /* we were still smooth in all axes */
                    {        /* add up contribution to final result  */
                      sum += value;
                      count ++;
                    }
                }
              dst_buf[dst_offset*4+c] = sum / count;
            }
          dst_buf[dst_offset*4+3] = center_pix[3]; /* copy alpha unmodified */
          dst_offset++;
          center_pix += 4;
        }
    }
}

static void prepare (GeglOperation *operation)
{
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

  area->left = area->right = area->top = area->bottom = o->iterations;
  gegl_operation_set_format (operation, "input",  babl_format ("R'G'B'A float"));
  gegl_operation_set_format (operation, "output", babl_format ("R'G'B'A float"));
}

static void prepare_cl (GeglOperation *operation)
{
    GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
    GeglChantO              *o = GEGL_CHANT_PROPERTIES (operation);

    area->left = area->right = area->top = area->bottom = o->iterations;
    gegl_operation_set_format (operation, "input",  babl_format ("R'G'B'A float"));
    GeglNode * self;
    GeglPad *pad;
    Babl * format=babl_format ("R'G'B'A float");
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

#define INPLACE 1

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
  GeglChantO   *o = GEGL_CHANT_PROPERTIES (operation);

  if (gegl_cl_is_opencl_available())
  {
      GeglRectangle rect = *result;
      rect.x      -= o->iterations;
      rect.y      -= o->iterations;
      rect.width  += o->iterations * 2;
      rect.height += o->iterations * 2;

      noise_reduct_cl(
          input, &rect,
          output, result,
          o->iterations);

      return TRUE;
  }
  int iteration;
  int stride;
  float *src_buf;
#ifndef INPLACE
  float *dst_buf;
#endif
  GeglRectangle rect;
  rect = *result;

  stride = result->width + o->iterations * 2;

  src_buf = g_new0 (float,
         (stride) * (result->height + o->iterations * 2) * 4);
#ifndef INPLACE
  dst_buf = g_new0 (float,
         (stride) * (result->height + o->iterations * 2) * 4);
#endif

  {
    rect.x      -= o->iterations;
    rect.y      -= o->iterations;
    rect.width  += o->iterations*2;
    rect.height += o->iterations*2;
    gegl_buffer_get (input, 1.0, &rect, babl_format ("R'G'B'A float"),
                     src_buf, stride * 4 * 4);
  }

  for (iteration = 0; iteration < o->iterations; iteration++)
    {
      noise_reduction (src_buf, stride,
#ifdef INPLACE
                       src_buf + (stride + 1) * 4,
#else
                       dst_buf,
#endif
                       result->width  + (o->iterations - 1 - iteration) * 2,
                       result->height + (o->iterations - 1 - iteration) * 2,
                       stride);
#ifndef INPLACE
      { /* swap buffers */
        float *tmp = src_buf;
        src_buf = dst_buf;
        dst_buf = tmp;
      }
#endif
    }

  gegl_buffer_set (output , result, babl_format ("R'G'B'A float"),
#ifndef INPLACE
                   src_buf,
#else
                   src_buf + ((stride +1) * 4) * o->iterations,
#endif
                   stride * 4 * 4);

  g_free (src_buf);
#ifndef INPLACE
  g_free (dst_buf);
#endif

  return  TRUE;
}


static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglRectangle  result = {0,0,0,0};
  GeglRectangle *in_rect = gegl_operation_source_get_bounding_box (operation,
                                                                     "input");
  if (!in_rect)
    return result;
  return *in_rect;
}

static const char* kernel_source =
"#define NEIGHBOURS 8                                                  \n"
"#define AXES       (NEIGHBOURS/2)                                     \n"
"                                                                      \n"
"#define POW2(a) ((a)*(a))                                             \n"
"#define GEN_METRIC(before, center, after)                           \\\n"
"    POW2((center) * 2 - (before) - (after))                           \n"
"#define BAIL_CONDITION(new,original) ((new) < (original))             \n"
"                                                                      \n"
"#define SYMMETRY(a)  (NEIGHBOURS - (a) - 1)                           \n"
"                                                                      \n"
"#define O(u,v) (((u)+((v) * (src_stride))))                           \n"
"                                                                      \n"
"__kernel void noise_reduction_cl (__global       float4 *src_buf,     \n"
"                                  int     src_stride,                 \n"
"                                  __global       float4 *dst_buf,     \n"
"                                  int     dst_stride)                 \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"                                                                      \n"
"    __global  float4 *center_pix =                                    \n"
"        src_buf + (gidy + 1) * src_stride + gidx + 1;                 \n"
"    int     dst_offset = dst_stride * gidy + gidx;                    \n"
"                                                                      \n"
"    int offsets[NEIGHBOURS] = {                                       \n"
"        O(-1, -1), O(  0, -1), O(  1, -1),                            \n"
"        O(-1,  0),             O(  1,  0),                            \n"
"        O(-1,  1), O(  0,  1), O(  1,  1)                             \n"
"    };                                                                \n"
"                                                                      \n"
"    float4 sum;                                                       \n"
"    int4   count;                                                     \n"
"    float4 metric_reference[AXES];                                    \n"
"                                                                      \n"
"    for (int axis = 0; axis < AXES; axis++)                           \n"
"    {                                                                 \n"
"        float4 before_pix      = *(center_pix + offsets[axis]);       \n"
"        float4 after_pix       =                                      \n"
"            *(center_pix + offsets[SYMMETRY(axis)]);                  \n"
"        metric_reference[axis] =                                      \n"
"            GEN_METRIC (before_pix, *center_pix, after_pix);          \n"
"    }                                                                 \n"
"                                                                      \n"
"    sum   = *center_pix;                                              \n"
"    count = 1;                                                        \n"
"                                                                      \n"
"    for (int direction = 0; direction < NEIGHBOURS; direction++)      \n"
"    {                                                                 \n"
"        __global const float4 *pix   =                                \n"
"            center_pix + offsets[direction];                          \n"
"        float4 value = (*pix + *center_pix) * (0.5f);                 \n"
"        int    axis;                                                  \n"
"        int4   mask = {1, 1, 1, 0};                                   \n"
"                                                                      \n"
"        for (axis = 0; axis < AXES; axis++)                           \n"
"        {                                                             \n"
"            float4 before_pix = *(center_pix + offsets[axis]);        \n"
"            float4 after_pix  =                                       \n"
"                *(center_pix + offsets[SYMMETRY(axis)]);              \n"
"                                                                      \n"
"            float4 metric_new =                                       \n"
"                GEN_METRIC (before_pix, value, after_pix);            \n"
"            mask              =                                       \n"
"                ((                                                    \n"
"                BAIL_CONDITION(metric_new, metric_reference[axis]     \n"
"                ))) & mask;                                           \n"
"        }                                                             \n"
"        sum            += mask >0 ? value : 0;                        \n"
"        count          += mask >0 ? 1     : 0;                        \n"
"    }                                                                 \n"
"    dst_buf[dst_offset]       = (sum/convert_float4(count));          \n"
"    dst_buf[dst_offset].w     = (*center_pix).w;                      \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data = NULL;

static void
noise_reduct_cl (GeglBuffer  *src,
                 const GeglRectangle  *src_rect,
                 GeglBuffer  *dst,
                 const GeglRectangle  *dst_rect,
                 const int   iterations)
{
    const Babl  * in_format = babl_format("R'G'B'A float");
    const Babl  *out_format = babl_format("R'G'B'A float");
    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
#include "gegl-cl-operation-area-filter-fw1.h"
    ///////////////////////////////////////////////////////////////////////////
    /* Algorithm specific processing flow.
       Build kernels, setting parameters, and running them. */

    if (!cl_data)
    {
        const char *kernel_name[] ={"noise_reduction_cl", NULL};
            cl_data = gegl_cl_compile_and_build(kernel_source, kernel_name);
    }
    if (!cl_data) CL_ERROR;

    int i = 0;
    size_t gbl_size_tmp[2];
    cl_int n_src_stride  = dst_rect->width + iterations * 2;
    for (i = 0;i<iterations;++i)
    {
        if (i > 0)
        {
            cl_mem tmp_mem = dst_mem;
            dst_mem = src_mem;
            src_mem = tmp_mem;
        }
        gbl_size_tmp[0] = gbl_size[0] + 2 * (iterations - 1 -i);
        gbl_size_tmp[1] = gbl_size[1] + 2 * (iterations - 1 -i);

        CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
            cl_data->kernel[0], 0, sizeof(cl_mem), (void*)&src_mem));
        CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
            cl_data->kernel[0], 1, sizeof(cl_int), (void*)&n_src_stride));
        CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
            cl_data->kernel[0], 2, sizeof(cl_mem), (void*)&dst_mem));
        CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
            cl_data->kernel[0], 3, sizeof(cl_int), (void*)&n_src_stride));

        CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
            gegl_cl_get_command_queue(), cl_data->kernel[0],
            2, NULL,
            gbl_size_tmp, NULL,
            0, NULL, NULL));

        errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
        if (CL_SUCCESS != errcode) CL_ERROR;
    }

    ///////////////////////////////////////////////////////////////////////////
    /* AreaFilter general processing flow.
       Making the necessary color space conversion and Saving data. */
#include "gegl-cl-operation-area-filter-fw4.h"
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
  operation_class->get_bounding_box = get_bounding_box;

  operation_class->name        = "gegl:noise-reduction";
  operation_class->opencl_support = TRUE;
  operation_class->categories  = "enhance";
  operation_class->description = "Anisotropic like smoothing operation";
}

#endif
