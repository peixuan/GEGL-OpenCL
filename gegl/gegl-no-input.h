#ifndef __GEGL_NO_INPUT_H__
#define __GEGL_NO_INPUT_H__

#include "gegl-point-op.h"
#include "gegl-scanline-processor.h"
#include "gegl-color-model.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GEGL_TYPE_NO_INPUT               (gegl_no_input_get_type ())
#define GEGL_NO_INPUT(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEGL_TYPE_NO_INPUT, GeglNoInput))
#define GEGL_NO_INPUT_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass),  GEGL_TYPE_NO_INPUT, GeglNoInputClass))
#define GEGL_IS_NO_INPUT(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEGL_TYPE_NO_INPUT))
#define GEGL_IS_NO_INPUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass),  GEGL_TYPE_NO_INPUT))
#define GEGL_NO_INPUT_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj),  GEGL_TYPE_NO_INPUT, GeglNoInputClass))

typedef struct _GeglNoInput GeglNoInput;
struct _GeglNoInput 
{
    GeglPointOp point_op;

    /*< private >*/
};

typedef struct _GeglNoInputClass GeglNoInputClass;
struct _GeglNoInputClass 
{
    GeglPointOpClass point_op_class;

    GeglScanlineFunc (*get_scanline_function) (GeglNoInput *self,
                                               GeglColorModel *cm);
};

GType           gegl_no_input_get_type          (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
