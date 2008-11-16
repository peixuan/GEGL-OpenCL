/* This file is part of GEGL
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


#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include "gegl-types.h"
#include "gegl-buffer-private.h"
#include "gegl-sampler-linear.h"
#include <string.h>
#include <math.h>


enum
{
  PROP_0,
  PROP_LAST
};

static void    gegl_sampler_linear_get (GeglSampler  *self,
                                        gdouble       x,
                                        gdouble       y,
                                        void         *output);
static void    set_property            (GObject      *gobject,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec);
static void    get_property            (GObject      *gobject,
                                        guint         prop_id,
                                        GValue       *value,
                                        GParamSpec   *pspec);


G_DEFINE_TYPE (GeglSamplerLinear, gegl_sampler_linear, GEGL_TYPE_SAMPLER)

static void
gegl_sampler_linear_class_init (GeglSamplerLinearClass *klass)
{
  GeglSamplerClass *sampler_class = GEGL_SAMPLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = set_property;
  object_class->get_property = get_property;

  sampler_class->get     = gegl_sampler_linear_get;
}

static void
gegl_sampler_linear_init (GeglSamplerLinear *self)
{
  GEGL_SAMPLER (self)->context_rect = (GeglRectangle){ 0, 0, 2, 2};
  GEGL_SAMPLER (self)->interpolate_format = babl_format ("RaGaBaA float");
}

void
gegl_sampler_linear_get (GeglSampler *self,
                         gdouble      x,
                         gdouble      y,
                         void        *output)
{
  gfloat            *sampler_bptr;
  const gint         offsets[4]={0,        4,
                                 (64-1)*4, 4};
  gfloat             newval[4] = {0.0, 0.0, 0.0, 0.0};
  gfloat             q[4];
  gfloat             dx,dy;
  gfloat             uf, vf;
  gint               u,v;
  gint               i;


  uf = x - (gint) x;
  vf = y - (gint) y;

  q[0] = (1 - uf) * (1 - vf);
  q[1] = uf * (1 - vf);
  q[2] = (1 - uf) * vf;
  q[3] = uf * vf;
  dx = (gint) x;
  dy = (gint) y;

  sampler_bptr = gegl_sampler_get_ptr (self, dx, dy);

  /* FIXME: unroll this loop */
  for (i=0, v=dy; v < dy+2; v++)
    for (u=dx; u < dx+2; u++, i++)
      {
        sampler_bptr += offsets[i];
        newval[0] += q[i] * sampler_bptr[0];
        newval[1] += q[i] * sampler_bptr[1];
        newval[2] += q[i] * sampler_bptr[2];
        newval[3] += q[i] * sampler_bptr[3];
      }

  babl_process (babl_fish (self->interpolate_format, self->format),
                newval, output, 1);
}



static void
set_property (GObject      *gobject,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  switch (property_id)
    {

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

static void
get_property (GObject    *gobject,
              guint       property_id,
              GValue     *value,
              GParamSpec *pspec)
{
  switch (property_id)
    {

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, pspec);
        break;
    }
}

