#include <glib-object.h>

#include "gegl-types.h"

#include "gegl-node.h"
#include "gegl-pad.h"
#include "gegl-visitable.h"

#include "gegl-mock-property-visitor.h"


static void gegl_mock_property_visitor_class_init (GeglMockPropertyVisitorClass *klass);
static void gegl_mock_property_visitor_init       (GeglMockPropertyVisitor      *self);

static void visit_pad (GeglVisitor *visitor, GeglPad *pad);


G_DEFINE_TYPE (GeglMockPropertyVisitor, gegl_mock_property_visitor,
               GEGL_TYPE_VISITOR)


static void
gegl_mock_property_visitor_class_init (GeglMockPropertyVisitorClass *klass)
{
  GeglVisitorClass *visitor_class = GEGL_VISITOR_CLASS (klass);

  visitor_class->visit_pad = visit_pad;
}

static void
gegl_mock_property_visitor_init (GeglMockPropertyVisitor *self)
{
}

static void
visit_pad (GeglVisitor *visitor,
           GeglPad     *pad)
{
  GEGL_VISITOR_CLASS (gegl_mock_property_visitor_parent_class)->visit_pad (visitor, pad);

#if 0
  {
    GeglOperation *operation = gegl_pad_get_operation (property);
    g_print ("Visiting property %s from op %s\n",
             gegl_pad_get_name (property),
             gegl_object_get_name (GEGL_OBJECT (operation)));
  }
#endif
}
