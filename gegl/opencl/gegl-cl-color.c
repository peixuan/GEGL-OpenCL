#include "gegl.h"
#include "gegl-cl-color.h"
#include "gegl-cl-init.h"
#include "gegl-cl-color-kernel.h"
#define CL_FORMAT_N 10

static gegl_cl_run_data * kernels_color = NULL;

static const Babl *format[CL_FORMAT_N];

void
gegl_cl_color_compile_kernels(void)
{
	const char *kernel_name[] = {"non_premultiplied_to_premultiplied", /* 0 */
		"premultiplied_to_non_premultiplied", /* 1 */
		"rgba2rgba_gamma_2_2",                /* 2 */
		"rgba_gamma_2_22rgba",                /* 3 */
		"rgba2rgba_gamma_2_2_premultiplied",  /* 4 */
		"rgba_gamma_2_2_premultiplied2rgba",  /* 5 */
		"rgbaf_to_rgbau8",                    /* 6 */
		"rgbau8_to_rgbaf",                    /* 7 */
		"rgbaf_to_rgbu8",                    /* 8 */
		"rgbu8_to_rgbaf",                    /* 9 */
		NULL};

	    format[0] = babl_format ("RaGaBaA float"),
		format[1] = babl_format ("RGBA float"),
		format[2] = babl_format ("R'G'B'A float"),
		format[3] = babl_format ("RGBA float"),
		format[4] = babl_format ("R'aG'aB'aA float"),
		format[5] = babl_format ("RGBA float"),
		format[6] = babl_format ("RGBA u8"),
		format[7] = babl_format ("RGBA float"),
		format[8] = babl_format ("RGB u8"),
		format[9] = babl_format ("RGBA float"),

		kernels_color = gegl_cl_compile_and_build (kernel_color_source, kernel_name);
}

gegl_cl_color_op
gegl_cl_color_supported (const Babl *in_format, const Babl *out_format)
{
  int i;
  gboolean supported_format_in  = FALSE;
  gboolean supported_format_out = FALSE;

  if (in_format == out_format)
	  return CL_COLOR_EQUAL;

  for (i = 0; i < CL_FORMAT_N; i++)
    {
      if (format[i] == in_format)  supported_format_in  = TRUE;
      if (format[i] == out_format) supported_format_out = TRUE;
    }

  if (supported_format_in && supported_format_out)
	  return CL_COLOR_CONVERT;
  else
	  return CL_COLOR_NOT_SUPPORTED;
}

#define CONV_1(x)   {conv[0] = x; conv[1] = -1;}
#define CONV_2(x,y) {conv[0] = x; conv[1] =  y;}

#define CL_ERROR {g_assert(0);}

gboolean
gegl_cl_color_conv (cl_mem *in_tex, cl_mem *out_tex, int out_in,const size_t pixel_count,
					const Babl *in_format, const Babl *out_format)
{
	
	int i;
	int errcode;
	int conv[2] = {-1, -1};
	cl_mem color_in_tex,color_out_tex;
	
    size_t bpp_in_format=babl_format_get_bytes_per_pixel(in_format);
	size_t bpp_out_format=babl_format_get_bytes_per_pixel(out_format);
	
	color_in_tex=* in_tex;
	color_out_tex=* out_tex;
	
	g_printf("[OpenCL] Converting between color formats: (%s -> %s)\n", babl_get_name(in_format),
		                                                                          babl_get_name(out_format));

	if(in_format == babl_format ("RGBA float"))
	{
		if      (out_format == babl_format ("RaGaBaA float"))    CONV_1(0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_1(2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_1(4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_1(6)
		else if (out_format == babl_format ("RGB u8"))           CONV_1(8)
	}
	else if (in_format == babl_format ("RaGaBaA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(1)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(1, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(1, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(1, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(1, 8)
	}
	else if (in_format == babl_format ("R'G'B'A float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(3)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(3, 0)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(3, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(3, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(3, 8)
	}
	else if (in_format == babl_format ("R'aG'aB'aA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(5)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(5, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(5, 2)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(5, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(5, 8)
	}
	else if (in_format == babl_format ("RGBA u8"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(7)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(7, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(7, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(7, 4)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(7, 8)
	}
	else if (in_format == babl_format ("RGB u8"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(9)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(9, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(9, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(9, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(9, 6)
	}


	/* We have to know,just do once here: one kind of format---->RGBA float
	   if done twice,we should match the divided cl_men precisely.
	   and more extension will be added afterwards
	*/
	for (i=0; conv[i] >= 0 && i<2; i++)
		
	{
	
		cl_mem tmp_tex;
		errcode = gegl_clSetKernelArg(kernels_color->kernel[conv[i]], 0, sizeof(cl_mem), (void*)&color_in_tex);
		if (errcode != CL_SUCCESS) CL_ERROR;
        
		errcode = gegl_clSetKernelArg(kernels_color->kernel[conv[i]], 1, sizeof(cl_mem), (void*)&color_out_tex);
		if (errcode != CL_SUCCESS) CL_ERROR;



		const size_t global_size[1]={pixel_count};
		
		errcode = gegl_clEnqueueNDRangeKernel(gegl_cl_get_command_queue (),
		kernels_color->kernel[conv[i]], 1,
		NULL, global_size, NULL,
		0, NULL, NULL);
		if (errcode != CL_SUCCESS) CL_ERROR

		errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
		if (errcode != CL_SUCCESS) CL_ERROR

		tmp_tex = color_in_tex;
		color_in_tex =color_out_tex;
		color_out_tex = tmp_tex;
	}

   //the out_in flag means whether the in_tex and out_tex should still be seen as input and output 
   if(out_in==1){
		*in_tex=color_in_tex;
		*out_tex=color_out_tex;
	}
	
	return TRUE;
	}
