    ///////////////////////////////////////////////////////////////////////////
    /* AreaFilter general processing flow.
       Making the necessary color space conversion and Saving data. */

    if (CL_COLOR_NOT_SUPPORTED == need_babl_out ||
        CL_COLOR_EQUAL         == need_babl_out)
    {
        dst_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
            dst_mem, CL_TRUE, CL_MAP_READ,
            0, size_in,
            0, NULL, NULL,
            &errcode);
        if (CL_SUCCESS != errcode) CL_ERROR;

        gegl_buffer_set(dst, src_rect, out_format, dst_buf,
            GEGL_AUTO_ROWSTRIDE);
        errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
            dst_mem, dst_buf, 
            0, NULL, NULL);
        if (CL_SUCCESS != errcode) CL_ERROR;
    }
    else if (CL_COLOR_CONVERT == need_babl_out)
    {
        gegl_cl_color_conv(&dst_mem, &src_mem, 1,
            src_rect->width * src_rect->height,
            out_format, dst_format);
        errcode = gegl_clEnqueueBarrier(gegl_cl_get_command_queue());
        if (CL_SUCCESS != errcode) CL_ERROR;

        dst_buf = gegl_clEnqueueMapBuffer(gegl_cl_get_command_queue(),
            dst_mem, CL_TRUE, CL_MAP_READ,
            0, size_src,
            0, NULL, NULL,
            &errcode);
        if (CL_SUCCESS != errcode) CL_ERROR;

        gegl_buffer_set(dst, src_rect, dst_format, dst_buf,
            GEGL_AUTO_ROWSTRIDE);
        errcode = gegl_clEnqueueUnmapMemObject(gegl_cl_get_command_queue(),
            dst_mem, dst_buf, 
            0, NULL, NULL);
        if (CL_SUCCESS != errcode) CL_ERROR;
    }

    gegl_clReleaseMemObject(src_mem);
    gegl_clReleaseMemObject(dst_mem);