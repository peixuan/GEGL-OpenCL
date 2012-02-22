#include "gegl.h"
#include "gegl-cl-color.h"
#include "gegl-cl-init.h"
#include "gegl-cl-color-kernel.h"
#define CL_FORMAT_N 11

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
		"rgbaf_to_rgbu8",                     /* 8 */
		"rgbu8_to_rgbaf",                     /* 9 */
		"rgba2ycbcra"   ,                     /*10 */
		"ycbcra2rgba"   ,                     /*11 */
		"rgba2graya"    ,                     /*12 */
		"graya2rgba"    ,                     /*13 */
		"rgba2gray"     ,                     /*14 */
		"gray2rgba"     ,                     /*15 */
		"rgba_to_gray_alpha_premultiplied",   /*16 */
		"gray_alpha_premultiplied_to_rgba",   /*17 */
		"rgba2gray_gamma_2_2_premultiplied",  /*18 */
		"gray_gamma_2_2_premultiplied2rgba",  /*19 */
		NULL};

	    format[0] = babl_format ("RGBA float"),
	    format[1] = babl_format ("RaGaBaA float"),		
		format[2] = babl_format ("R'G'B'A float"),
		format[3] = babl_format ("R'aG'aB'aA float"),
		format[4] = babl_format ("RGBA u8"),
		format[5] = babl_format ("RGB u8"),
		format[6] = babl_format ("Y'CbCrA float"),
		format[7] = babl_format ("YA float"),		
		format[8] = babl_format ("Y float"),
		format[9] = babl_format ("YaA float"),
		format[10]= babl_format ("Y'aA float"),

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
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_1(10)
		else if (out_format == babl_format ("YA float"))         CONV_1(12)
		else if (out_format == babl_format ("Y float"))          CONV_1(14)
		else if (out_format == babl_format ("YaA float"))        CONV_1(16)
		else if (out_format == babl_format ("Y'aA float"))		 CONV_1(18)

	}
	else if (in_format == babl_format ("RaGaBaA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(1)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(1, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(1, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(1, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(1, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(1,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(1,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(1,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(1,16)
		else if (out_format == babl_format ("Y'aA float"))		 CONV_2(1,18)
	}
	else if (in_format == babl_format ("R'G'B'A float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(3)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(3, 0)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(3, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(3, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(3, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(3,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(3,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(3,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(3,16)
		else if (out_format == babl_format ("Y'aA float"))       CONV_2(3,18)
	}
	else if (in_format == babl_format ("R'aG'aB'aA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(5)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(5, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(5, 2)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(5, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(5, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(5,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(5,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(5,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(5,16)
		else if (out_format == babl_format ("Y'aA float"))       CONV_2(5,18)
	}
	else if (in_format == babl_format ("RGBA u8"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(7)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(7, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(7, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(7, 4)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(7, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(7,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(7,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(7,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(7,16)
		else if (out_format == babl_format ("Y'aA float"))       CONV_2(7,18)
	}
	else if (in_format == babl_format ("RGB u8"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(9)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(9, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(9, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(9, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(9, 6)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(9,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(9,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(9,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(9,16)
		else if (out_format == babl_format ("Y'aA float"))       CONV_2(9,18)
	}
	else if (in_format == babl_format ("Y'CbCrA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(11)
	    else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(11, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(11, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(11, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(11, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(11, 8)
		else if (out_format == babl_format ("YA float"))         CONV_2(11,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(11,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(11,16)
		else if (out_format == babl_format ("Y'aA float"))		 CONV_2(11,18)
	}
	else if (in_format == babl_format ("YA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(13)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(13, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(13, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(13, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(13, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(13, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(13,10)
		else if (out_format == babl_format ("Y float"))          CONV_2(13,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(13,16)
		else if (out_format == babl_format ("Y'aA float"))		 CONV_2(13,18)
	}
	else if (in_format == babl_format ("Y float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(15)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(15, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(15, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(15, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(15, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(15, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(15,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(15,12)
		else if (out_format == babl_format ("YaA float"))        CONV_2(15,16)
		else if (out_format == babl_format ("Y'aA float"))		 CONV_2(15,18)
	}
	else if (in_format == babl_format ("YaA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(17)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(17, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(17, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(17, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(17, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(17, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(17,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(17,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(17,14)
		else if (out_format == babl_format ("Y'aA float"))		 CONV_2(17,18)
	}
	else if (in_format == babl_format ("Y'aA float"))
	{
		if      (out_format == babl_format ("RGBA float"))       CONV_1(19)
		else if (out_format == babl_format ("RaGaBaA float"))    CONV_2(19, 0)
		else if (out_format == babl_format ("R'G'B'A float"))    CONV_2(19, 2)
		else if (out_format == babl_format ("R'aG'aB'aA float")) CONV_2(19, 4)
		else if (out_format == babl_format ("RGBA u8"))          CONV_2(19, 6)
		else if (out_format == babl_format ("RGB u8"))           CONV_2(19, 8)
		else if (out_format == babl_format ("Y'CbCrA float"))    CONV_2(19,10)
		else if (out_format == babl_format ("YA float"))         CONV_2(19,12)
		else if (out_format == babl_format ("Y float"))          CONV_2(19,14)
		else if (out_format == babl_format ("YaA float"))        CONV_2(19,16)
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
   else if(out_in==0){	  
	   *out_tex=color_in_tex;
       *in_tex=color_out_tex;		 	  
   }	
	return TRUE;
	}
