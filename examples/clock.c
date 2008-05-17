#include <gegl.h>
#include <glib/gprintf.h>

#include <sys/time.h>
#include <time.h>

gint
main (gint    argc,
      gchar **argv)
{
  GeglNode   *gegl;   /* the gegl graph we're using as a node factor */
  GeglNode   *display,
             *text,
             *layer,
             *crop,
             *shift,
             *blank;

  if (argv[1]==NULL)
    {
      g_print ("\nUsage: %s <GeglBuffer>\n"
               "\n"
               "Continously writes a timestamp to 0,0 in the buffer\n", argv[0]);
      exit (-1);
    }

  gegl_init (&argc, &argv);  /* initialize the GEGL library */

  gegl = gegl_node_new ();


  blank      = gegl_node_new_child (gegl,
                                    "operation", "color",
                                    "value", gegl_color_new ("rgba(0.0,0.0,0.0,0.4)"),
                                    NULL);

  crop       = gegl_node_new_child (gegl,
                                    "operation", "crop",
                                    "x", 0.0,
                                    "y", 0.0,
                                    "width", 260.0,
                                    "height", 22.0,
                                    NULL);

  layer      = gegl_node_new_child (gegl,
                                    "operation", "layer",
                                    NULL);

  shift      = gegl_node_new_child (gegl,
                                    "operation", "shift",
                                    "x", 0.0,
                                    "y", 0.0,
                                    NULL);

  text       = gegl_node_new_child (gegl,
                                   "operation", "text",
                                   "size", 20.0,
                             /*      "color", gegl_color_new ("rgb(0.0,0.0,0.0)"),*/
                                   NULL);
  display    = gegl_node_new_child (gegl,
                                    "operation", "composite-buffer",
                                    "path", argv[1],
                                    NULL);

  gegl_node_link_many (blank, crop, layer, shift, display, NULL);
  gegl_node_connect_to (text, "output", layer, "aux");
  
  /* request that the save node is processed, all dependencies will
   * be processed as well
   */
  {
    gint frame;
    gint frames = 1024;
    GTimeVal val;

    for (frame=0; frame<frames; frame++)
      {
        gchar string[512];
        struct timeval tv;

        gettimeofday(&tv, NULL);
        gegl_node_set (text, "string", ctime((void*)&tv), NULL);
        gegl_node_process (display);
        g_usleep (1000000);
      }
  }
  /* free resources globally used by GEGL */
  gegl_exit ();

  return 0;
}