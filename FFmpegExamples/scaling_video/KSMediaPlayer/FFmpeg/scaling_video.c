//
//  scaling_video.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/11.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "scaling_video.h"
#include "libavutil/imgutils.h"
#include "libavutil/parseutils.h"
#include "libswscale/swscale.h"

static void fill_yuv_image(uint8_t *data[4], int linesize[4], int width, int height, int frame_index) {
    int x, y;
    
    /* Y */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            data[0][y * linesize[0] + x] = x + y + frame_index * 3;
        }
    }
    
    /* Cb and Cr */
    for (y = 0; y <height/2; y++) {
        for (x = 0; x < width/2; x++) {
            data[1][y * linesize[1] + x] = 128 + y + frame_index * 2;
            data[2][y * linesize[2] + x] = 64 + x +frame_index * 5;
        }
    }
}

int scaling_video_port(char *dst_url, char *format) {
    uint8_t *src_data[4], *dst_data[4];
    int src_linesize[4], dst_linesize[4];
    int src_w = 320, src_h = 240, dst_w, dst_h;
    enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P, dst_pix_fmt = AV_PIX_FMT_RGB24;
    const char *dst_size = NULL;
    const char *dst_filename = NULL;
    FILE *dst_file;
    int dst_bufsize;
    struct SwsContext *sws_ctx;
    int i, ret;
    
    if (!dst_url || !format) {
        return -1;
    }
    dst_filename = dst_url;
    dst_size = format;
    
    /*
     av_parse_video_size()于解析出输入的分辨率字符串的宽高信息。例如，输入的字符串为“1920x1080”或者“1920*1080”，经过av_parse_video_size()的处理之后，可以得到宽度为1920，高度为1080；此外，输入一个“特定分辨率”字符串例如“vga”，也可以得到宽度为640，高度为480。该函数不属于AVOption这部分的内容，而是整个FFmpeg通用的一个字符串解析函数。
     */
    if (av_parse_video_size(&dst_w, &dst_h, dst_size) < 0) {
        fprintf(stderr, "Invalid size '%s', must be in the form WxH or a valid size abbreviation\n", dst_size);
        return -1;
    }
    
    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        goto ksend;
    }
    
    /* create scaling context */
    /*
     成功后返回SwsContext 类型的结构体。

     参数1：被转换源的宽
     参数2：被转换源的高
     参数3：被转换源的格式，eg：YUV、RGB……（枚举格式，也可以直接用枚举的代号表示eg：AV_PIX_FMT_YUV420P这些枚举的格式在libavutil/pixfmt.h中列出）
     参数4：转换后指定的宽
     参数5：转换后指定的高
     参数6：转换后指定的格式同参数3的格式
     参数7：转换所使用的算法，
     参数8：NULL
     参数9：NULL
     参数10：NULL
     
    struct SwsContext *sws_getContext(int srcW, //输入图像的宽度
                                      int srcH, //输入图像的宽度
                                      enum AVPixelFormat srcFormat, //输入图像的像素格式
                                      int dstW, //输出图像的宽度
                                      int dstH, //输出图像的高度
                                      enum AVPixelFormat dstFormat, //输出图像的像素格式
                                      int flags,//选择缩放算法(只有当输入输出图像大小不同时有效),一般选择SWS_FAST_BILINEAR
                                      SwsFilter *srcFilter, //输入图像的滤波器信息, 若不需要传NULL
                                      SwsFilter *dstFilter, //输出图像的滤波器信息, 若不需要传NULL
                                      const double *param //特定缩放算法需要的参数(?)，默认为NULL
                                      );
     */
    sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt,
                             dst_w, dst_h, dst_pix_fmt,
                             SWS_BILINEAR, NULL, NULL, NULL);
    
    if (!sws_ctx) {
        fprintf(stderr,
                "Impossible to create scale context for the conversion "
                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(src_pix_fmt), src_w, src_h,
                av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h);
        ret = AVERROR(EINVAL);
        goto ksend;
    }
    
    /* allocate source and destination image buffers 分配源和目标图像缓冲区 */
    /*
    ffmpeg中av_image_alloc():照指定的宽、高、像素格式来分析图像内存。
    参数说明：
      pointers[4]：保存图像通道的地址。如果是RGB，则前三个指针分别指向R,G,B的内存地址。第四个指针保留不用
      linesizes[4]：保存图像每个通道的内存对齐的步长，即一行的对齐内存的宽度，此值大小等于图像宽度。
      w:              要申请内存的图像宽度。
      h:              要申请内存的图像高度。
      pix_fmt:        要申请内存的图像的像素格式。
      align:          用于内存对齐的值。
      返回值：所申请的内存空间的总大小。如果是负值，表示申请失败。
    */
    if ((ret = av_image_alloc(src_data,
                              src_linesize,
                              src_w,
                              src_h,
                              src_pix_fmt,
                              16)) < 0) {
        fprintf(stderr, "Could not allocate source image\n");
        goto ksend;
    }
    
    /* buffer is going to be written to rawvideo file, no alignment 缓冲区将被写入rawvideo文件，不对齐*/
    if ((ret = av_image_alloc(dst_data, dst_linesize, dst_w, dst_h, dst_pix_fmt, 1)) < 0) {
        fprintf(stderr, "Could not allocate destination image\n");
        goto ksend;
    }
    
    dst_bufsize = ret;
    
    for (i = 0; i < 100; i++) {
        /* generate synthetic video */
        fill_yuv_image(src_data, src_linesize, src_w, src_h, i);
        /* convert to destination format 转换为目标格式*/
        //处理图像数据,用于转换像素
        sws_scale(sws_ctx,
                  (const uint8_t * const *)src_data,
                  src_linesize,
                  0,
                  src_h,
                  dst_data,
                  dst_linesize);
        /* write scaled image to file */
        fwrite(dst_data[0], 1, dst_bufsize, dst_file);
    }
    fprintf(stderr, "Scaling succeeded. Play the output file with the command:\n"
            "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
            av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h, dst_filename);
ksend:
    fclose(dst_file);
    av_freep(&src_data[0]);
    av_freep(&dst_data[0]);
    sws_freeContext(sws_ctx);
    
    return ret < 0;
}
