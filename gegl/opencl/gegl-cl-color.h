#ifndef __GEGL_CL_COLOR_H__
#define __GEGL_CL_COLOR_H__

#include <gegl.h>
#include "gegl-cl-init.h"

typedef enum
{
	CL_COLOR_NOT_SUPPORTED = 0,
	CL_COLOR_EQUAL         = 1,
	CL_COLOR_CONVERT       = 2
} gegl_cl_color_op;

void gegl_cl_color_compile_kernels(void);

gegl_cl_color_op gegl_cl_color_supported (const Babl *in_format, const Babl *out_format);

gboolean
gegl_cl_color_conv (cl_mem *in_tex, cl_mem *out_tex, int out_in,const size_t pixel_count,
					const Babl *in_format, const Babl *out_format);
#endif
