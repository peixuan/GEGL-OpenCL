#ifndef __GEGL_MOCK_OBJECT_H__
#define __GEGL_MOCK_OBJECT_H__

#include "gegl-object.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GEGL_TYPE_MOCK_OBJECT               (gegl_mock_object_get_type ())
#define GEGL_MOCK_OBJECT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEGL_TYPE_MOCK_OBJECT, GeglMockObject))
#define GEGL_MOCK_OBJECT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  GEGL_TYPE_MOCK_OBJECT, GeglMockObjectClass))
#define GEGL_IS_MOCK_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEGL_TYPE_MOCK_OBJECT))
#define GEGL_IS_MOCK_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEGL_TYPE_MOCK_OBJECT))
#define GEGL_MOCK_OBJECT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEGL_TYPE_MOCK_OBJECT, GeglMockObjectClass))

typedef struct _GeglMockObject GeglMockObject;
struct _GeglMockObject
{
       GeglObject object;
};

typedef struct _GeglMockObjectClass GeglMockObjectClass;
struct _GeglMockObjectClass
{
       GeglObjectClass object;
};

GType         gegl_mock_object_get_type          (void) G_GNUC_CONST;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
