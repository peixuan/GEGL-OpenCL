#include <stdio.h>
#include <stdlib.h>
#include "gegl.h"
#include "gegl-check-op.h"
#include "testutils.h"

gboolean
testutils_check_pixel_rgb_float(GeglImageOp *image_op, 
                           gfloat a, 
                           gfloat b, 
                           gfloat c)
{
  return testutils_check_pixel_rgb_float_xy(image_op, 0, 0, a, b, c);
}

gboolean
testutils_check_pixel_rgba_float(GeglImageOp *image_op, 
                           gfloat a, 
                           gfloat b, 
                           gfloat c,
                           gfloat d)
{
  return testutils_check_pixel_rgba_float_xy(image_op, 0, 0, a, b, c, d);
}

gboolean
testutils_check_pixel_rgb_float_xy(GeglImageOp *image_op, 
                              gint x, gint y,
                              gfloat a, gfloat b, gfloat c)
{
  gboolean success;

  GeglColor *color = g_object_new(GEGL_TYPE_COLOR,
                                  "rgb-float", a, b, c,
                                  NULL);
                                  
  GeglOp * check_op = g_object_new(GEGL_TYPE_CHECK_OP, 
                                   "color", color, 
                                   "x", x, "y", y,
                                   "image-op", image_op,
                                   NULL);
  g_object_unref(color);

  gegl_op_apply(check_op); 

  success = gegl_check_op_get_success(GEGL_CHECK_OP(check_op)); 

  g_object_unref(check_op);

  return success;
}

gboolean
testutils_check_pixel_rgba_float_xy(GeglImageOp *image_op, 
                              gint x, gint y,
                              gfloat a, gfloat b, gfloat c, gfloat d)
{
  gboolean success;

  GeglColor *color = g_object_new(GEGL_TYPE_COLOR,
                                  "rgba-float", a, b, c, d,
                                  NULL);

  GeglOp * check_op = g_object_new(GEGL_TYPE_CHECK_OP, 
                                   "color", color, 
                                   "x", x, "y", y,
                                   "image-op", image_op,
                                   NULL);

  g_object_unref(color);
  gegl_op_apply(check_op); 

  success = gegl_check_op_get_success(GEGL_CHECK_OP(check_op)); 

  g_object_unref(check_op);

  return success;
}

gboolean
testutils_check_rgb_uint8(GeglImageOp *image_op, 
                           guint8 a, 
                           guint8 b, 
                           guint8 c)
{
  return testutils_check_rgb_uint8_xy(image_op, 0, 0, a, b, c);
}

gboolean
testutils_check_rgb_uint8_xy(GeglImageOp *image_op, 
                             gint x, gint y,
                             guint8 a, guint8 b, guint8 c)
{
  gboolean success;

  GeglColor *color = g_object_new(GEGL_TYPE_COLOR,
                                  "rgb-float", a/255.0, b/255.0, c/255.0,
                                  NULL);

  GeglOp * check_op = g_object_new(GEGL_TYPE_CHECK_OP, 
                                   "color", color, 
                                   "x", x, "y", y,
                                   "image-op", image_op,
                                   NULL);
  g_object_unref(color);
  gegl_op_apply(check_op); 

  success = gegl_check_op_get_success(GEGL_CHECK_OP(check_op)); 

  g_object_unref(check_op);

  return success;
}
