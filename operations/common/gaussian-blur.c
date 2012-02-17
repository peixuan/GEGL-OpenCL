/* This file is an image processing operation for GEGL
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
 * Copyright 2006 Dominik Ernst <dernst@gmx.de>
 *
 * Recursive Gauss IIR Filter as described by Young / van Vliet
 * in "Signal Processing 44 (1995) 139 - 151"
 *
 * NOTE: The IIR filter should not be used for radius < 0.5, since it
 *       becomes very inaccurate.
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include "gegl.h"
#include "gegl-types-internal.h"
#include "graph/gegl-pad.h"
#include "graph/gegl-node.h"
#include "gegl-utils.h"


#ifdef GEGL_CHANT_PROPERTIES

gegl_chant_double (std_dev_x, _("Size X"), 0.0, 200.0, 4.0,
   _("Standard deviation for the horizontal axis. (multiply by ~2 to get radius)"))
gegl_chant_double (std_dev_y, _("Size Y"), 0.0, 200.0, 4.0,
   _("Standard deviation for the vertical axis. (multiply by ~2 to get radius.)"))
gegl_chant_string (filter, _("Filter"), "auto",
   _("Optional parameter to override the automatic selection of blur filter. "
     "Choices are fir, iir, auto"))

#else

#define GEGL_CHANT_TYPE_AREA_FILTER
#define GEGL_CHANT_C_FILE       "gaussian-blur.c"

#include "gegl-chant.h"
#include <math.h>
#include <stdio.h>

#define RADIUS_SCALE   4


static void
iir_young_find_constants (gfloat   radius,
                          gdouble *B,
                          gdouble *b);

static gint
fir_gen_convolve_matrix (gdouble   sigma,
                         gdouble **cmatrix_p);

static void
fir_blur_cl   (GeglBuffer          *src,
               const GeglRectangle *src_rect,
               GeglBuffer          *dst,
               const GeglRectangle *dst_rect,
               gdouble             *cmatrix,
               gint                 matrix_length,
               gint                 yoff,
               gboolean             ver);

static void
iir_young_blur_cl   (GeglBuffer          *src,
                     const GeglRectangle *src_rect,
                     GeglBuffer          *dst,
                     const GeglRectangle *dst_rect,
                     gdouble              B,
                     gdouble             *b,
                     gboolean             ver);

static void
iir_young_find_constants (gfloat   sigma,
                          gdouble *B,
                          gdouble *b)
{
  gdouble q;

  if (sigma == 0.0)
    {
      /* to prevent unexpected ringing at tile boundaries,
         we define an (expensive) copy operation here */
      *B = 1.0;
      b[0] = 1.0;
      b[1] = b[2] = b[3] = 0.0;
      return;
    }

  if(sigma >= 2.5)
    q = 0.98711*sigma - 0.96330;
  else
    q = 3.97156 - 4.14554*sqrt(1-0.26891*sigma);

  b[0] = 1.57825 + (2.44413*q) + (1.4281*q*q) + (0.422205*q*q*q);
  b[1] = (2.44413*q) + (2.85619*q*q) + (1.26661*q*q*q);
  b[2] = -((1.4281*q*q) + (1.26661*q*q*q));
  b[3] = 0.422205*q*q*q;

  *B = 1 - ( (b[1]+b[2]+b[3])/b[0] );
}

static inline void
iir_young_blur_1D (gfloat  * buf,
                   gint      offset,
                   gint      delta_offset,
                   gdouble   B,
                   gdouble * b,
                   gfloat  * w,
                   gint      w_len)
{
  gint wcount, i;
  gdouble tmp;

  /* forward filter */
  wcount = 0;

  while (wcount < w_len)
    {
      tmp = 0;

      for (i=1; i<4; i++)
        {
          if (wcount-i >= 0)
            tmp += b[i]*w[wcount-i];
        }

      tmp /= b[0];
      tmp += B*buf[offset];
      w[wcount] = tmp;

      wcount++;
      offset += delta_offset;
    }

  /* backward filter */
  wcount = w_len - 1;
  offset -= delta_offset;

  while (wcount >= 0)
    {
      tmp = 0;

      for (i=1; i<4; i++)
        {
          if (wcount+i < w_len)
            tmp += b[i]*buf[offset+delta_offset*i];
        }

      tmp /= b[0];
      tmp += B*w[wcount];
      buf[offset] = tmp;

      offset -= delta_offset;
      wcount--;
    }
}

/* expects src and dst buf to have the same height and no y-offset */
static void
iir_young_hor_blur (GeglBuffer          *src,
                    const GeglRectangle *src_rect,
                    GeglBuffer          *dst,
                    const GeglRectangle *dst_rect,
                    gdouble              B,
                    gdouble             *b)
{
  gint v;
  gint c;
  gint w_len;
  gfloat *buf;
  gfloat *w;

  buf = g_new0 (gfloat, src_rect->height * src_rect->width * 4);
  w   = g_new0 (gfloat, src_rect->width);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RaGaBaA float"),
                   buf, GEGL_AUTO_ROWSTRIDE);

  w_len = src_rect->width;
  for (v=0; v<src_rect->height; v++)
    {
      for (c = 0; c < 4; c++)
        {
          iir_young_blur_1D (buf,
                             v*src_rect->width*4+c,
                             4,
                             B,
                             b,
                             w,
                             w_len);
        }
    }

  gegl_buffer_set (dst, src_rect, babl_format ("RaGaBaA float"),
                   buf, GEGL_AUTO_ROWSTRIDE);
  g_free (buf);
  g_free (w);
}

/* expects src and dst buf to have the same width and no x-offset */
static void
iir_young_ver_blur (GeglBuffer          *src,
                    const GeglRectangle *src_rect,
                    GeglBuffer          *dst,
                    const GeglRectangle *dst_rect,
                    gdouble              B,
                    gdouble             *b)
{
  gint u;
  gint c;
  gint w_len;
  gfloat *buf;
  gfloat *w;

  buf = g_new0 (gfloat, src_rect->height * src_rect->width * 4);
  w   = g_new0 (gfloat, src_rect->height);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RaGaBaA float"),
                   buf, GEGL_AUTO_ROWSTRIDE);

  w_len = src_rect->height;
  for (u=0; u<dst_rect->width; u++)
    {
      for (c = 0; c < 4; c++)
        {
          iir_young_blur_1D (buf,
                             u*4 + c,
                             src_rect->width*4,
                             B,
                             b,
                             w,
                             w_len);
        }
    }

  gegl_buffer_set (dst, src_rect,
                   babl_format ("RaGaBaA float"), buf, GEGL_AUTO_ROWSTRIDE);
  g_free (buf);
  g_free (w);
}


static gint
fir_calc_convolve_matrix_length (gdouble sigma)
{
  return sigma ? ceil (sigma)*6.0+1.0 : 1;
}

static gint
fir_gen_convolve_matrix (gdouble   sigma,
                         gdouble **cmatrix_p)
{
  gint     matrix_length;
  gdouble *cmatrix;

  matrix_length = fir_calc_convolve_matrix_length (sigma);
  cmatrix = g_new (gdouble, matrix_length);
  if (!cmatrix)
    return 0;

  if (matrix_length == 1)
    {
      cmatrix[0] = 1;
    }
  else
    {
      gint i,x;
      gdouble sum = 0;

      for (i=0; i<matrix_length/2+1; i++)
        {
          gdouble y;

          x = i - (matrix_length/2);
          y = (1.0/(sigma*sqrt(2.0*G_PI))) * exp(-(x*x) / (2.0*sigma*sigma));

          cmatrix[i] = y;
          sum += cmatrix[i];
        }

      for (i=matrix_length/2 + 1; i<matrix_length; i++)
        {
          cmatrix[i] = cmatrix[matrix_length-i-1];
          sum += cmatrix[i];
        }

      for (i=0; i<matrix_length; i++)
      {
        cmatrix[i] /= sum;
      }
    }

  *cmatrix_p = cmatrix;
  return matrix_length;
}

static inline float
fir_get_mean_component_1D (gfloat  * buf,
                           gint      offset,
                           gint      delta_offset,
                           gdouble * cmatrix,
                           gint      matrix_length)
{
  gint    i;
  gdouble acc=0;

  for (i=0; i < matrix_length; i++)
    {
      acc += buf[offset] * cmatrix[i];
      offset += delta_offset;
    }

  return acc;
}

/* expects src and dst buf to have the same height and no y-offset */
static void
fir_hor_blur (GeglBuffer          *src,
              const GeglRectangle *src_rect,
              GeglBuffer          *dst,
              const GeglRectangle *dst_rect,
              gdouble             *cmatrix,
              gint                 matrix_length,
              gint                 xoff) /* offset between src and dst */
{
  gint        u,v;
  gint        offset;
  gfloat     *src_buf;
  gfloat     *dst_buf;
  const gint  radius = matrix_length/2;
  const gint  src_width = src_rect->width;

  g_assert (xoff >= radius);

  src_buf = g_new0 (gfloat, src_rect->height * src_rect->width * 4);
  dst_buf = g_new0 (gfloat, dst_rect->height * dst_rect->width * 4);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RaGaBaA float"),
                   src_buf, GEGL_AUTO_ROWSTRIDE);

  offset = 0;
  for (v=0; v<dst_rect->height; v++)
    for (u=0; u<dst_rect->width; u++)
      {
        gint src_offset = (u-radius+xoff + v*src_width) * 4;
        gint c;
        for (c=0; c<4; c++)
          dst_buf [offset++] = fir_get_mean_component_1D (src_buf,
                                                          src_offset + c,
                                                          4,
                                                          cmatrix,
                                                          matrix_length);
      }

  gegl_buffer_set (dst, dst_rect, babl_format ("RaGaBaA float"),
                   dst_buf, GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}

/* expects src and dst buf to have the same width and no x-offset */
static void
fir_ver_blur (GeglBuffer          *src,
              const GeglRectangle *src_rect,
              GeglBuffer          *dst,
              const GeglRectangle *dst_rect,
              gdouble             *cmatrix,
              gint                 matrix_length,
              gint                 yoff) /* offset between src and dst */
{
  gint        u,v;
  gint        offset;
  gfloat     *src_buf;
  gfloat     *dst_buf;
  const gint  radius = matrix_length/2;
  const gint  src_width = src_rect->width;

  g_assert (yoff >= radius);

  src_buf = g_new0 (gfloat, src_rect->width * src_rect->height * 4);
  dst_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  gegl_buffer_get (src, 1.0, src_rect, babl_format ("RaGaBaA float"),
                   src_buf, GEGL_AUTO_ROWSTRIDE);

  offset=0;
  for (v=0; v< dst_rect->height; v++)
    for (u=0; u< dst_rect->width; u++)
      {
        gint src_offset = (u + (v-radius+yoff)*src_width) * 4;
        gint c;
        for (c=0; c<4; c++)
          dst_buf [offset++] = fir_get_mean_component_1D (src_buf,
                                                          src_offset + c,
                                                          src_width * 4,
                                                          cmatrix,
                                                          matrix_length);
      }

  gegl_buffer_set (dst, dst_rect, babl_format ("RaGaBaA float"),
                   dst_buf, GEGL_AUTO_ROWSTRIDE);
  g_free (src_buf);
  g_free (dst_buf);
}

static void prepare (GeglOperation *operation)
{
#define max(A,B) ((A) > (B) ? (A) : (B))
  GeglOperationAreaFilter *area = GEGL_OPERATION_AREA_FILTER (operation);
  GeglChantO              *o    = GEGL_CHANT_PROPERTIES (operation);

  gfloat fir_radius_x = fir_calc_convolve_matrix_length (o->std_dev_x) / 2;
  gfloat fir_radius_y = fir_calc_convolve_matrix_length (o->std_dev_y) / 2;
  gfloat iir_radius_x = o->std_dev_x * RADIUS_SCALE;
  gfloat iir_radius_y = o->std_dev_y * RADIUS_SCALE;

  /* XXX: these should be calculated exactly considering o->filter, but we just
   * make sure there is enough space */
  area->left = area->right = ceil ( max (fir_radius_x, iir_radius_x));
  area->top = area->bottom = ceil ( max (fir_radius_y, iir_radius_y));

  if (gegl_cl_is_opencl_available())
  {
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
      return;
  }
  gegl_operation_set_format (operation, "output",
                             babl_format ("RaGaBaA float"));
#undef max
}


static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result)
{
  GeglRectangle rect;
  GeglBuffer *temp;
  GeglOperationAreaFilter *op_area = GEGL_OPERATION_AREA_FILTER (operation);
  GeglChantO              *o       = GEGL_CHANT_PROPERTIES (operation);

  GeglRectangle temp_extend;
  gdouble       B, b[4];
  gdouble      *cmatrix;
  gint          cmatrix_len;
  gboolean      force_iir;
  gboolean      force_fir;

  rect.x      = result->x - op_area->left;
  rect.width  = result->width + op_area->left + op_area->right;
  rect.y      = result->y - op_area->top;
  rect.height = result->height + op_area->top + op_area->bottom;

  temp_extend = rect;
  temp_extend.x      = result->x;
  temp_extend.width  = result->width;
  temp = gegl_buffer_new (&temp_extend, babl_format ("RaGaBaA float"));

  force_iir = o->filter && !strcmp (o->filter, "iir");
  force_fir = o->filter && !strcmp (o->filter, "fir");

  if ((force_iir || o->std_dev_x > 1.0) && !force_fir)
    {
      iir_young_find_constants (o->std_dev_x, &B, b);
      if (gegl_cl_is_opencl_available())
          iir_young_blur_cl (input, &rect, temp, &temp_extend, B, b, FALSE);
      else
      iir_young_hor_blur (input, &rect, temp, &temp_extend,  B, b);
    }
  else
    {
      cmatrix_len = fir_gen_convolve_matrix (o->std_dev_x, &cmatrix);
      if (gegl_cl_is_opencl_available())
          fir_blur_cl (input, &rect, temp, &temp_extend,
              cmatrix, cmatrix_len, op_area->left, FALSE);
      else
      fir_hor_blur (input, &rect, temp, &temp_extend,
                    cmatrix, cmatrix_len, op_area->left);
      g_free (cmatrix);
    }


  if ((force_iir || o->std_dev_y > 1.0) && !force_fir)
    {
      iir_young_find_constants (o->std_dev_y, &B, b);
      if (gegl_cl_is_opencl_available())
          iir_young_blur_cl (temp, &temp_extend, output, result, B, b, TRUE);
      else
      iir_young_ver_blur (temp, &temp_extend, output, result, B, b);
    }
  else
    {
      cmatrix_len = fir_gen_convolve_matrix (o->std_dev_y, &cmatrix);
      if (gegl_cl_is_opencl_available())
          fir_blur_cl (temp, &temp_extend, output, result, cmatrix, cmatrix_len,
              op_area->top, TRUE);
      else
      fir_ver_blur (temp, &temp_extend, output, result, cmatrix, cmatrix_len,
                    op_area->top);
      g_free (cmatrix);
    }

  g_object_unref (temp);
  return  TRUE;
}

#include "opencl/gegl-cl.h"

static const char* kernel_source_fir =
"float4 fir_get_mean_component_1D_CL(const global float4 *buf,         \n"
"                                    int   offset,                     \n"
"                                    const int    delta_offset,        \n"
"                                    constant float *cmatrix,          \n"
"                                    const int  matrix_length)         \n"
"{                                                                     \n"
"    float4 acc = 0.0f;                                                \n"
"    int i;                                                            \n"
"                                                                      \n"
"    for(i=0; i<matrix_length; i++)                                    \n"
"    {                                                                 \n"
"        acc    += buf[offset] * cmatrix[i];                           \n"
"        offset += delta_offset;                                       \n"
"    }                                                                 \n"
"    return acc;                                                       \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void fir_ver_blur_CL(const global float4 *src_buf,           \n"
"                              const int     src_width,                \n"
"                              global   float4 *dst_buf,               \n"
"                              constant float  *cmatrix,               \n"
"                              const int     matrix_length,            \n"
"                              const int     yoff)                     \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"    int gid  = gidx + gidy * get_global_size(0);                      \n"
"                                                                      \n"
"    int radius = matrix_length / 2;                                   \n"
"    int src_offset = gidx + (gidy - radius + yoff) * src_width;       \n"
"                                                                      \n"
"    dst_buf[gid] = fir_get_mean_component_1D_CL(                      \n"
"        src_buf, src_offset, src_width, cmatrix, matrix_length);      \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void fir_hor_blur_CL(const global float4 *src_buf,           \n"
"                              const int src_width,                    \n"
"                              global float4 *dst_buf,                 \n"
"                              constant float *cmatrix,                \n"
"                              const int matrix_length,                \n"
"                              const int yoff)                         \n"
"{                                                                     \n"
"    int gidx = get_global_id(0);                                      \n"
"    int gidy = get_global_id(1);                                      \n"
"    int gid  = gidx + gidy * get_global_size(0);                      \n"
"                                                                      \n"
"    int radius = matrix_length / 2;                                   \n"
"    int src_offset = gidy * src_width + (gidx - radius + yoff);       \n"
"                                                                      \n"
"    dst_buf[gid] = fir_get_mean_component_1D_CL(                      \n"
"        src_buf, src_offset, 1, cmatrix, matrix_length);              \n"
"}                                                                     \n";

static const char* kernel_source_iir =
"void iir_young_blur_1D_CL(global float4 *buf,                         \n"
"                          int     offset,                             \n"
"                          const          int     delta_offset,        \n"
"                          const          float   B,                   \n"
"                          const          float4  b,                   \n"
"                          global float4 *w,                           \n"
"                          const          int     w_len,               \n"
"                          const          int     idx)                 \n"
"{                                                                     \n"
"    int i;                                                            \n"
"    float4 tmp[3];                                                    \n"
"    float4 ts[3];                                                     \n"
"    float4 t;                                                         \n"
"                                                                      \n"
"    ts[0] = 0;                                                        \n"
"    ts[1] = 0;                                                        \n"
"    ts[2] = 0;                                                        \n"
"    int k = 0;                                                        \n"
"                                                                      \n"
"    for(i=0; i<w_len; i++)                                            \n"
"    {                                                                 \n"
"        tmp[0]= b.w * ts[k++];                                        \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        tmp[1]= b.z * ts[k++];                                        \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        tmp[2]= b.y * ts[k++];                                        \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        t = (tmp[0] + tmp[1] + tmp[2]) / b.x;                         \n"
"                                                                      \n"
"        ts[k++] = w[idx + i] = B*buf[offset] + t;                     \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        offset += delta_offset;                                       \n"
"    }                                                                 \n"
"                                                                      \n"
"    --i;                                                              \n"
"    offset -= delta_offset;                                           \n"
"                                                                      \n"
"    ts[0] = 0;                                                        \n"
"    ts[1] = 0;                                                        \n"
"    ts[2] = 0;                                                        \n"
"    k     = 0;                                                        \n"
"                                                                      \n"
"    for(; i>=0; --i)                                                  \n"
"    {                                                                 \n"
"        tmp[0]= b.w * ts[k++];                                        \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        tmp[1]= b.z * ts[k++];                                        \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        tmp[2]= b.y * ts[k++];                                        \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        t = (tmp[0] + tmp[1] + tmp[2]) / b.x;                         \n"
"                                                                      \n"
"        ts[k++] = buf[offset] = B*w[idx + i] + t;                     \n"
"        if(k >= 3)                                                    \n"
"            k = 0;                                                    \n"
"                                                                      \n"
"        offset -= delta_offset;                                       \n"
"    }                                                                 \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void iir_young_hor_blur_CL(global float4 *src_buf,           \n"
"                                    const  int     src_width,         \n"
"                                    const  int     src_height,        \n"
"                                    const  float   B,                 \n"
"                                    global float4 *w,                 \n"
"                                    const  float4  b)                 \n"
"{                                                                     \n"
"    int gid  = get_global_id(0);                                      \n"
"                                                                      \n"
"    iir_young_blur_1D_CL(src_buf,                                     \n"
"        gid * src_width, 1, B, b, w, src_width, gid * src_width);     \n"
"}                                                                     \n"
"                                                                      \n"
"__kernel void iir_young_ver_blur_CL(global float4 *src_buf,           \n"
"                                    const  int     src_width,         \n"
"                                    const  int     src_height,        \n"
"                                    const  float   B,                 \n"
"                                    global float4 *w,                 \n"
"                                    const  float4  b)                 \n"
"{                                                                     \n"
"    int gid  = get_global_id(0);                                      \n"
"                                                                      \n"
"    iir_young_blur_1D_CL(src_buf, gid,                                \n"
"        src_width, B, b, w, src_height, gid * src_height);            \n"
"}                                                                     \n";

static gegl_cl_run_data *cl_data_fir = NULL;
static gegl_cl_run_data *cl_data_iir = NULL;

static void
fir_blur_cl   (GeglBuffer         *src,
               const GeglRectangle *src_rect,
               GeglBuffer          *dst,
               const GeglRectangle *dst_rect,
               gdouble             *dmatrix,
               gint                 matrix_length,
               gint                 yoff,
               gboolean             ver)
{
    gfloat *fmatrix=(gfloat *)dmatrix;
    gint i;
    for(i=0;i<matrix_length;i++)
    {
        fmatrix[i] = (gfloat)dmatrix[i];
    }

    const Babl  * in_format = babl_format("RaGaBaA float");
    const Babl  *out_format = babl_format("RaGaBaA float");
    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
#include "gegl-cl-operation-area-filter-fw1.h"
    ///////////////////////////////////////////////////////////////////////////
    /* Algorithm specific processing flow.
       Build kernels, setting parameters, and running them. */

    if (!cl_data_fir)
    {
        const char *kernel_name[] ={
            "fir_ver_blur_CL", "fir_hor_blur_CL", NULL};
            cl_data_fir = gegl_cl_compile_and_build(kernel_source_fir, kernel_name);
    }
    if (!cl_data_fir) CL_ERROR;

    gint k = 0;
    if(!ver)
        ++k;

    cl_mem cl_matrix = gegl_clCreateBuffer(gegl_cl_get_context(),
        CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_ONLY,
        matrix_length * sizeof(cl_float), NULL, &errcode);
    if (CL_SUCCESS != errcode) CL_ERROR;

    errcode = gegl_clEnqueueWriteBuffer(gegl_cl_get_command_queue(), cl_matrix,
        CL_TRUE, NULL, matrix_length * sizeof(cl_float), fmatrix, NULL, NULL, NULL);
    if (CL_SUCCESS != errcode) CL_ERROR;

    cl_int cl_src_width     = src_rect->width;
    cl_int cl_matrix_length = matrix_length;
    cl_int cl_yoff          = yoff;

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_fir->kernel[k], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_fir->kernel[k], 1, sizeof(cl_int), (void*)&cl_src_width));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_fir->kernel[k], 2, sizeof(cl_mem), (void*)&dst_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_fir->kernel[k], 3, sizeof(cl_mem), (void*)&cl_matrix));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_fir->kernel[k], 4, sizeof(cl_int), (void*)&cl_matrix_length));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_fir->kernel[k], 5, sizeof(cl_int), (void*)&cl_yoff));

    CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
        gegl_cl_get_command_queue(), cl_data_fir->kernel[k],
        2, NULL,
        gbl_size, NULL,
        0, NULL, NULL));

    errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
    if (CL_SUCCESS != errcode) CL_ERROR;

    gegl_clReleaseMemObject(cl_matrix);

    ///////////////////////////////////////////////////////////////////////////
    /* AreaFilter general processing flow.
       Making the necessary color space conversion and Saving data. */
#include "gegl-cl-operation-area-filter-fw2.h"
}

static void
iir_young_blur_cl   (GeglBuffer          *src,
                     const GeglRectangle *src_rect,
                     GeglBuffer          *dst,
                     const GeglRectangle *dst_rect,
                     gdouble              B,
                     gdouble             *b,
                     gboolean             ver)
{
    gint i;

    gint k = 0;
    gint len = src_rect->height;
    size_t gbl_size_tmp[] = { src_rect->width };
    if(!ver)
    {
        ++k;
        len = src_rect->width;
        gbl_size_tmp[0] = src_rect->height;
    }

    const Babl  * in_format = babl_format("RaGaBaA float");
    const Babl  *out_format = babl_format("RaGaBaA float");
    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
#include "gegl-cl-operation-area-filter-fw1.h"
    ///////////////////////////////////////////////////////////////////////////
    /* Algorithm specific processing flow.
       Build kernels, setting parameters, and running them. */

    if (!cl_data_iir)
    {
        const char *kernel_name[] ={
            "iir_young_ver_blur_CL", "iir_young_hor_blur_CL", NULL};
            cl_data_iir = gegl_cl_compile_and_build(kernel_source_iir, kernel_name);
    }
    if (!cl_data_iir) CL_ERROR;

    cl_mem cl_w = gegl_clCreateBuffer(gegl_cl_get_context(),
        CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
        (len) * gbl_size_tmp[0] * sizeof(cl_float4), NULL, &errcode);
    if (CL_SUCCESS != errcode) CL_ERROR;

    cl_int    cl_src_width = src_rect->width;
    cl_int    cl_src_height= src_rect->height;
    cl_float  cl_B         = (cl_float)B;
    cl_float4 cl_b         = {(gfloat)b[0], (gfloat)b[1], (gfloat)b[2], (gfloat)b[3]};

    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_iir->kernel[k], 0, sizeof(cl_mem), (void*)&src_mem));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_iir->kernel[k], 1, sizeof(cl_int), (void*)&cl_src_width));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_iir->kernel[k], 2, sizeof(cl_int), (void*)&cl_src_height));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_iir->kernel[k], 3, sizeof(cl_float), (void*)&cl_B));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_iir->kernel[k], 4, sizeof(cl_mem), (void*)&cl_w));
    CL_SAFE_CALL(errcode = gegl_clSetKernelArg(
        cl_data_iir->kernel[k], 5, sizeof(cl_float4), (void*)&cl_b));

    CL_SAFE_CALL(errcode = gegl_clEnqueueNDRangeKernel(
        gegl_cl_get_command_queue(), cl_data_iir->kernel[k],
        1, NULL,
        gbl_size_tmp, NULL,
        0, NULL, NULL));

    errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
    if (CL_SUCCESS != errcode) CL_ERROR;

    gegl_clReleaseMemObject(cl_w);

    cl_mem tmp_mem = dst_mem;
    dst_mem = src_mem;
    src_mem = tmp_mem;

    ///////////////////////////////////////////////////////////////////////////
    /* AreaFilter general processing flow.
       Making the necessary color space conversion and Saving data. */
#include "gegl-cl-operation-area-filter-fw3.h"
}

static void
gegl_chant_class_init (GeglChantClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process    = process;
  operation_class->prepare = prepare;

  operation_class->categories  = "blur";
  operation_class->name        = "gegl:gaussian-blur";
  operation_class->opencl_support = TRUE;
  operation_class->description =
        _("Performs an averaging of neighbouring pixels with the "
          "normal distribution as weighting.");
}

#endif
