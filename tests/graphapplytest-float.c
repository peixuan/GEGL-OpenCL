#include <glib-object.h>
#include "gegl.h"
#include "ctest.h"
#include "csuite.h"
#include "testutils.h"

#define IMAGE_OP_WIDTH 20 
#define IMAGE_OP_HEIGHT 20 

static void
test_graph_apply(Test *t)
{
  /* 
     graph
       | 
     fill 

    --------------
    |(.05,.1,.15)|
    ------------- 
          |
      (.1,.2,.3)

  */ 

  GeglColor *color = g_object_new(GEGL_TYPE_COLOR, 
                                  "rgb-float", .1, .2, .3, 
                                  NULL);

  GeglOp * fill = g_object_new (GEGL_TYPE_FILL, 
                                "fill-color", color,
                                NULL); 

  GeglOp * fade = g_object_new (GEGL_TYPE_FADE,
                                "multiplier", .5,
                                NULL); 

  GeglOp * graph = g_object_new (GEGL_TYPE_GRAPH,
                                 "root", fade, 
                                 "input", 0, fill,
                                 NULL);
  g_object_unref(color);

  gegl_op_apply(graph); 

  /* Note: The result is in the fade image_op data */
  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(fade), .05, .1, .15));  

  g_object_unref(graph);
  g_object_unref(fade);
  g_object_unref(fill);
}

static void
test_graph_apply_with_source(Test *t)
{

  /* 
    --------
   | fade  |  <--graph
   ---------
       |
     fill 
      
     graph
       | 
     fill 

    --------------
    |(.05,.1,.15)|
    ------------- 
          |
      (.1,.2,.3)

  */ 

  GeglColor *color = g_object_new(GEGL_TYPE_COLOR, 
                                  "rgb-float", .1, .2, .3, 
                                  NULL);

  GeglOp * fill = g_object_new (GEGL_TYPE_FILL, 
                                "fill-color", color,
                                 NULL); 

  GeglOp * fade = g_object_new (GEGL_TYPE_FADE,
                                "multiplier", .5,
                                NULL); 

  GeglOp * graph = g_object_new (GEGL_TYPE_GRAPH,
                                 "root", fade, 
                                 "input", 0, fill,
                                 NULL);

  g_object_unref(color);

  gegl_op_apply(graph); 

  /* Note: The result is in the fade image_op data */
  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(fade), .05, .1, .15));  

  g_object_unref(graph);
  g_object_unref(fade);
  g_object_unref(fill);
}

static void
test_graph_apply_with_source_and_output(Test *t)
{
  /* 
     fade2
       |  
    --------
   | fade1 | <----graph
   ---------
       |
     fill 

   
     fade2
       |
     graph
       |
     fill

    (.025,.05,.075)
          |          
    --------------
    |(.05,.1,.15)|
    ------------- 
          |
     (.1,.2,.3) 

  */ 

  GeglColor *color = g_object_new(GEGL_TYPE_COLOR, 
                                  "rgb-float", .1, .2, .3, 
                                  NULL);

  GeglOp * fill = g_object_new (GEGL_TYPE_FILL, 
                                "fill-color", color,
                                 NULL); 

  GeglOp * fade1 = g_object_new (GEGL_TYPE_FADE,
                                 "multiplier", .5,
                                 NULL); 

  GeglOp * graph = g_object_new (GEGL_TYPE_GRAPH,
                                 "root", fade1, 
                                 "input", 0, fill,
                                 NULL);

  GeglOp * fade2 = g_object_new (GEGL_TYPE_FADE,
                                 "source", graph,
                                 "multiplier", .5,
                                 NULL); 

  g_object_unref(color);

  gegl_op_apply(fade2); 

  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(fade2), .025, .05, .075));  

  g_object_unref(fade2);
  g_object_unref(graph);
  g_object_unref(fade1);
  g_object_unref(fill);
}

static void
test_graph_apply_with_2_ops_source_and_output(Test *t)
{
  /* 
     fade3
       |  
    --------
   | fade2 | <----graph
   |   |   |
   | fade1 |
   ---------
       |
     fill 

   
     fade3
       |
     graph
       |
     fill 

    (.0125,.025,.0375)
          |          
    ----------------
    |(.025,.05,.075)|
    |     |         |
    |(.05,.1,.15)   |  
    ---------------- 
          |
     (.1,.2,.3) 

  */ 


  GeglColor *color = g_object_new(GEGL_TYPE_COLOR, 
                                  "rgb-float", .1, .2, .3, 
                                  NULL);

  GeglOp * fill = g_object_new (GEGL_TYPE_FILL, 
                                "fill-color", color,
                                 NULL); 

  GeglOp * fade1 = g_object_new (GEGL_TYPE_FADE,
                                 "multiplier", .5,
                                 NULL); 

  GeglOp * fade2 = g_object_new (GEGL_TYPE_FADE,
                                 "source", fade1,
                                 "multiplier", .5,
                                 NULL); 

  GeglOp * graph = g_object_new (GEGL_TYPE_GRAPH,
                                 "root", fade2, 
                                 "input", 0, fill,
                                 NULL);

  GeglOp * fade3 = g_object_new (GEGL_TYPE_FADE,
                                 "source", graph,
                                 "multiplier", .5,
                                 NULL); 

  g_object_unref(color);

  gegl_op_apply(fade3); 

  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(fade3), .0125, .025, .0375));  

  g_object_unref(fade3);
  g_object_unref(fade2);
  g_object_unref(graph);
  g_object_unref(fade1);
  g_object_unref(fill);
}

static void
test_graph_apply_add_graph_and_fill(Test *t)
{
  /*        
             iadd
            /    \ 
           /      \
      --------   fill2 
     | fade   |   
     |   |    |    
     | fill1  | 
     ---------  

             iadd
            /    \ 
           /      \
      graph      fill2  


           (.45,.6,.75)
            /        \
           /          \         
    --------------   (.4,.5,.6)
    |(.05,.1,.15)|     
    |     |      |  
    |(.1,.2,.3)  | 
    -------------  

  */ 

  GeglColor *color1 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .1, .2, .3,
                                   NULL);

  GeglOp * fill1 = g_object_new (GEGL_TYPE_FILL, 
                                 "fill-color", color1,
                                 NULL); 

  GeglOp * fade = g_object_new (GEGL_TYPE_FADE,
                                "source", fill1,
                                "multiplier", .5,
                                NULL); 

  GeglOp * graph = g_object_new (GEGL_TYPE_GRAPH,
                                 "root", fade, 
                                 NULL);

  GeglColor *color2 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .4, .5, .6,
                                   NULL);

  GeglOp * fill2 = g_object_new (GEGL_TYPE_FILL, 
                                 "fill-color", color2,
                                 NULL); 

  GeglOp * iadd = g_object_new (GEGL_TYPE_I_ADD, 
                                "source-0", graph,
                                "source-1", fill2,
                                NULL);  

  g_object_unref(color1);
  g_object_unref(color2);

  gegl_op_apply(iadd); 

  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(iadd), .45, .6, .75));  

  g_object_unref(iadd);
  g_object_unref(fill2);
  g_object_unref(graph);
  g_object_unref(fade);
  g_object_unref(fill1);
}

static void
test_graph_apply_add_graph_and_graph(Test *t)
{
  /*        
             iadd
            /    \ 
           /      \
      --------    -------------
     | fade   |  |    iadd1    |
     |   |    |  |   /    \    |  
     | fill1  |  | fill2 fill3 | 
     ---------   --------------

             iadd
            /    \ 
           /      \
      graph1    graph2  


           (1.15,1.4,1.65)
            /        \
           /          \         
    --------------   ----------------------
    |(.05,.1,.15)|   |    (1.1,1.3,1.5)    |
    |     |      |   |     /        \      |
    |(.1,.2,.3)  |   |(.4,.5,.6) (.7,.8,.9)|
    -------------     --------------------- 

  */ 

  GeglColor *color1 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .1, .2, .3,
                                   NULL);

  GeglOp * fill1 = g_object_new (GEGL_TYPE_FILL, 
                                  "fill-color", color1,
                                  NULL); 

  GeglOp * fade = g_object_new (GEGL_TYPE_FADE,
                                "source", fill1,
                                "multiplier", .5,
                                NULL); 

  GeglOp * graph1 = g_object_new (GEGL_TYPE_GRAPH,
                                  "root", fade, 
                                  NULL);

  GeglColor *color2 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .4, .5, .6,
                                   NULL);

  GeglOp * fill2 = g_object_new (GEGL_TYPE_FILL, 
                                 "fill-color", color2,
                                 NULL); 

  GeglColor *color3 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .7, .8, .9,
                                   NULL);

  GeglOp * fill3 = g_object_new (GEGL_TYPE_FILL, 
                                 "fill-color", color3,
                                 NULL); 

  GeglOp * iadd1 = g_object_new (GEGL_TYPE_I_ADD,
                                 "source-0", fill2,
                                 "source-1", fill3,
                                 NULL); 

  GeglOp * graph2 = g_object_new (GEGL_TYPE_GRAPH,
                                  "root", iadd1, 
                                  NULL);

  GeglOp * iadd = g_object_new (GEGL_TYPE_I_ADD,
                               "source-0", graph1,
                               "source-1", graph2,
                               NULL); 
                        

  g_object_unref(color1);
  g_object_unref(color2);
  g_object_unref(color3);

  gegl_op_apply(iadd); 

  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(iadd), 1.15, 1.4, 1.65));  

  g_object_unref(iadd);
  g_object_unref(graph2);
  g_object_unref(graph1);
  g_object_unref(fade);
  g_object_unref(fill1);
  g_object_unref(iadd1);
  g_object_unref(fill2);
  g_object_unref(fill3);
}

static void
test_graph_apply_with_2_sources(Test *t)
{
  /*        
             fade1 
              | 
         -------------
        |    iadd   |
        |   /    \  |  
        |fade2 fade3| 
        --------------
           |      |
         fill1  fill2

             fade1 
               |
             graph  
             /    \ 
          fill1 fill2 


          (.125,.175,.225)
              |
     ----------------------------
     |    (.25,.35,.45)         | 
     |    /         \           | 
     |(.05,.1,.15)(.2,.25,.3)   |
      -------------------------- 
          |          |
      (.1,.2,.3)  (.4,.5, .6)

  */ 

  GeglColor *color1 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .1, .2, .3,
                                   NULL);

  GeglOp * fill1 = g_object_new (GEGL_TYPE_FILL, 
                                 "fill-color", color1,
                                 NULL); 

  GeglColor *color2 = g_object_new(GEGL_TYPE_COLOR, 
                                   "rgb-float", .4, .5, .6,
                                   NULL);

  GeglOp * fill2 = g_object_new (GEGL_TYPE_FILL, 
                                 "fill-color", color2,
                                 NULL); 

  GeglOp * fade2 = g_object_new (GEGL_TYPE_FADE,
                                 "multiplier", .5,
                                 NULL); 

  GeglOp * fade3 = g_object_new (GEGL_TYPE_FADE,
                                 "multiplier", .5,
                                 NULL); 

  GeglOp * iadd = g_object_new (GEGL_TYPE_I_ADD,
                                "source-0", fade2,
                                "source-1", fade3,
                                NULL); 

  GeglOp * graph = g_object_new (GEGL_TYPE_GRAPH,
                                 "root", iadd, 
                                 "input", 0, fill1,
                                 "input", 2, fill2,
                                 NULL);

  GeglOp * fade1 = g_object_new (GEGL_TYPE_FADE,
                                 "source", graph,
                                 "multiplier", .5,
                                 NULL); 

  g_object_unref(color1);
  g_object_unref(color2);

  gegl_op_apply(fade1); 

  ct_test(t, testutils_check_pixel_rgb_float(GEGL_IMAGE_OP(fade1), .125, .175, .225));  

  g_object_unref(iadd);
  g_object_unref(graph);
  g_object_unref(fade1);
  g_object_unref(fade2);
  g_object_unref(fade3);
  g_object_unref(fill1);
  g_object_unref(fill2);
}

static void
graph_apply_test_setup(Test *test)
{
}

static void
graph_apply_test_teardown(Test *test)
{
}

Test *
create_graph_apply_test_float()
{
  Test* t = ct_create("GeglGraphApplyTestFloat");

  g_assert(ct_addSetUp(t, graph_apply_test_setup));
  g_assert(ct_addTearDown(t, graph_apply_test_teardown));

#if 1 
  g_assert(ct_addTestFun(t, test_graph_apply));
  g_assert(ct_addTestFun(t, test_graph_apply_with_source));
  g_assert(ct_addTestFun(t, test_graph_apply_with_source_and_output));
  g_assert(ct_addTestFun(t, test_graph_apply_with_2_ops_source_and_output));
  g_assert(ct_addTestFun(t, test_graph_apply_add_graph_and_fill));
  g_assert(ct_addTestFun(t, test_graph_apply_add_graph_and_graph));
  g_assert(ct_addTestFun(t, test_graph_apply_with_2_sources));
#endif

  return t; 
}
