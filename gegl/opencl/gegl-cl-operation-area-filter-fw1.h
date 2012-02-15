    /* AreaFilter general processing flow.
       Loading data and making the necessary color space conversion. */
    const Babl  *src_format = gegl_buffer_get_format(src);
    const Babl  *dst_format = gegl_buffer_get_format(dst);

    const size_t bpp_src    = babl_format_get_bytes_per_pixel(src_format);
    const size_t bpp_dst    = babl_format_get_bytes_per_pixel(dst_format);
    const size_t bpp_in     = babl_format_get_bytes_per_pixel( in_format);
    const size_t bpp_out    = babl_format_get_bytes_per_pixel(out_format);
    const size_t size_src   = src_rect->width * src_rect->height * bpp_src;
    const size_t size_dst   = dst_rect->width * dst_rect->height * bpp_dst;
    const size_t size_in    = src_rect->width * src_rect->height * bpp_in ;
    const size_t size_out   = dst_rect->width * dst_rect->height * bpp_out;
    const size_t gbl_size[2]= {dst_rect->width, dst_rect->height};
    
    gegl_cl_color_op need_babl_in  =
        gegl_cl_color_supported(src_format,  in_format);
    gegl_cl_color_op need_babl_out =
        gegl_cl_color_supported(out_format, dst_format);
    
    gfloat *src_buf = NULL;
    gfloat *dst_buf = NULL;
    
    cl_mem src_mem;
    cl_mem dst_mem;

    int errcode;

    src_mem = gegl_clCreateBuffer(gegl_cl_get_context(),
        CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
        MAX(MAX(size_src,size_dst),MAX(size_in,size_out)),
        NULL, &errcode);
    if (CL_SUCCESS != errcode) CL_ERROR;
    dst_mem = gegl_clCreateBuffer(gegl_cl_get_context(),
        CL_MEM_ALLOC_HOST_PTR|CL_MEM_READ_WRITE,
        MAX(MAX(size_src,size_dst),MAX(size_in,size_out)),
        NULL, &errcode);
    if (CL_SUCCESS != errcode) CL_ERROR;

    if (CL_COLOR_NOT_SUPPORTED == need_babl_in ||
        CL_COLOR_EQUAL         == need_babl_in)
    {
        src_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
            src_mem, CL_TRUE, CL_MAP_WRITE,
            NULL, size_in,
            NULL, NULL, NULL,
            &errcode);
        if (CL_SUCCESS != errcode) CL_ERROR;

        gegl_buffer_get(src, 1.0, src_rect,  in_format, src_buf,
            GEGL_AUTO_ROWSTRIDE);
        errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
            src_mem, src_buf, 
            NULL, NULL, NULL);
        if (CL_SUCCESS != errcode) CL_ERROR;
    }
    else if (CL_COLOR_CONVERT == need_babl_in)
    {
        src_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
            src_mem, CL_TRUE, CL_MAP_WRITE,
            NULL, size_src,
            NULL, NULL, NULL,
            &errcode);
        if (CL_SUCCESS != errcode) CL_ERROR;
        gegl_buffer_get(src, 1.0, src_rect, src_format, src_buf,
            GEGL_AUTO_ROWSTRIDE);
        errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
            src_mem, src_buf, 
            NULL, NULL, NULL);
        if (CL_SUCCESS != errcode) CL_ERROR;

        gegl_cl_color_conv(&src_mem, &dst_mem, 1,
            src_rect->width * src_rect->height,
            src_format, in_format);
        errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
        if (CL_SUCCESS != errcode) CL_ERROR;
    }
    ///////////////////////////////////////////////////////////////////////////