#include <glib-object.h>
#include "gegl.h"
#include "ctest.h"
#include "csuite.h"
#include "testutils.h"

#define AREA_WIDTH 10 
#define AREA_HEIGHT 10 

static void
test_value_tile_set(Test *test)
{
  GeglRect area = {0,0,AREA_WIDTH,AREA_HEIGHT};
  GeglColorModel *color_model = gegl_color_model_instance("RgbFloat");
  GeglTile * tile = g_object_new (GEGL_TYPE_TILE, 
                                  "area", &area, 
                                  "colormodel", color_model,
                                   NULL);  
  GValue *value =  g_new0(GValue, 1); 

  g_value_init(value, GEGL_TYPE_TILE);
  g_value_set_tile(value, tile);
  ct_test(test, tile == g_value_get_tile(value));
  g_value_unset(value);

  g_free(value);

  g_object_unref(color_model);
  g_object_unref(tile);
}

static void
test_value_tile_copy(Test *test)
{
  GeglRect area = {0,0,AREA_WIDTH,AREA_HEIGHT};
  GeglColorModel *color_model = gegl_color_model_instance("RgbFloat");
  GeglTile * tile = g_object_new (GEGL_TYPE_TILE, 
                                  "area", &area, 
                                  "colormodel", color_model,
                                   NULL);  

  GValue * src_value =  g_new0(GValue, 1); 
  GValue * dest_value =  g_new0(GValue, 1); 

  g_value_init(dest_value, GEGL_TYPE_TILE);
  g_value_init(src_value, GEGL_TYPE_TILE);

  g_value_set_tile(src_value, tile);

  g_value_copy(src_value, dest_value);

  ct_test(test, tile == g_value_get_tile(dest_value));

  g_value_unset(dest_value);
  g_value_unset(src_value);

  g_free(dest_value);
  g_free(src_value);

  g_object_unref(color_model);
  g_object_unref(tile);
}

static void
test_value_tile_compatible(Test *test)
{
  GeglRect area = {0,0,AREA_WIDTH,AREA_HEIGHT};
  GeglColorModel *color_model = gegl_color_model_instance("RgbFloat");
  GeglTile * src_tile = g_object_new (GEGL_TYPE_TILE, 
                                  "area", &area, 
                                  "colormodel", color_model,
                                   NULL);  
  GeglTile * dest_tile = g_object_new (GEGL_TYPE_TILE, 
                                  "area", &area, 
                                  "colormodel", color_model,
                                   NULL);  

  GValue * src_value =  g_new0(GValue, 1); 
  GValue * dest_value =  g_new0(GValue, 1); 
  GValue * float_value = g_new0(GValue, 1);
  GValue * int_value = g_new0(GValue, 1);

  g_value_init(dest_value, GEGL_TYPE_TILE);
  g_value_init(src_value, GEGL_TYPE_TILE);
  g_value_init(float_value, G_TYPE_FLOAT);
  g_value_init(int_value, G_TYPE_INT);

  g_value_set_tile(src_value, src_tile);
  g_value_set_float(float_value, 3.4);

  ct_test(test, g_value_type_compatible(G_VALUE_TYPE(src_value), G_VALUE_TYPE(dest_value)));
  ct_test(test, g_value_type_transformable(G_VALUE_TYPE(src_value), G_VALUE_TYPE(dest_value)));
  ct_test(test, g_value_transform(src_value, dest_value));
  ct_test(test, !g_value_type_compatible(G_VALUE_TYPE(src_value), G_VALUE_TYPE(float_value)));
  ct_test(test, !g_value_type_transformable(G_VALUE_TYPE(src_value), G_VALUE_TYPE(float_value)));
  ct_test(test, !g_value_transform(src_value, float_value));

  ct_test(test, g_value_type_transformable(G_VALUE_TYPE(float_value), G_VALUE_TYPE(int_value)));
  ct_test(test, g_value_transform(float_value, int_value));

  /* float to int conversion just truncates */
  ct_test(test, 3 == g_value_get_int(int_value));

  g_value_unset(dest_value);
  g_value_unset(src_value);
  g_value_unset(float_value);

  g_free(dest_value);
  g_free(src_value);
  g_free(float_value);

  g_object_unref(color_model);
  g_object_unref(src_tile);
  g_object_unref(dest_tile);
}

static void
value_test_setup(Test *test)
{
}

static void
value_test_teardown(Test *test)
{
}

Test *
create_value_test()
{
  Test* t = ct_create("GeglValueTest");

  g_assert(ct_addSetUp(t, value_test_setup));
  g_assert(ct_addTearDown(t, value_test_teardown));
  g_assert(ct_addTestFun(t, test_value_tile_set));
  g_assert(ct_addTestFun(t, test_value_tile_copy));
  g_assert(ct_addTestFun(t, test_value_tile_compatible));
  g_assert(ct_addTestFun(t, test_value_tile_compatible));

  return t; 
}
