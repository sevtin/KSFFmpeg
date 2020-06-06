//
//  avio_reading.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/4.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "avio_reading.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/file.h"

struct buffer_data {
    uint8_t *ptr;
    size_t size;///< size left in the buffer
};

//用来将内存buffer的数据拷贝到buf
static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct buffer_data *bd = (struct buffer_data *)opaque;
    /* FFMIN(a,b) ((a) > (b) ? (b) : (a)) */
    //取最小值:4096
    buf_size = FFMIN(buf_size, bd->size);
    if (!buf_size) {
        return AVERROR_EOF;
    }
    /* void *memcpy(void *dst, const void *src, size_t n) */
    /* 由src指向地址为起始地址的连续n个字节的数据复制到以dst指向地址为起始地址的空间内。*/
    memcpy(buf, bd->ptr, buf_size);
    
    bd->ptr += buf_size;
    /* 从文件缓存大小逐渐减少至0*/
    bd->size -= buf_size;
    
    printf("\n|------| %d : %d |------|\n",bd->size,buf_size);
    return buf_size;
}

int avio_reading_port(char *url) {
    /* AVFormatContext是用于描述了一个媒体文件或媒体流的构成和基本信息的结构体,
     * 主要用于封装和解封装。
     */
    AVFormatContext *fmt_ctx = NULL;
    /*
     * AVIOContext是用于管理输入输出数据的结构体
     */
    AVIOContext *avio_ctx = NULL;
    
    /*
     读取数据缓存
     */
    uint8_t *buffer = NULL;
    uint8_t *avio_ctx_buffer = NULL;
    /*
     读取数据大小
     */
    size_t buffer_size;
    size_t avio_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = {0};
    
    if (!url) {
        return -1;
    }
    input_filename = url;
    
    /* slurp file content into buffer */
    /* 将整个输入文件读进buffer中（包含申请buffer的动作） */
    ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0) {
        goto ksfault;
    }
    
    /* fill opaque structure used by the AVIOContext read callback */
    /*
     记录读取的数据缓存和数据大小
     */
    bd.ptr = buffer;
    bd.size = buffer_size;
    
     /* avformat_alloc_context申请一个AVFormatContext */
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    /* av_malloc申请一块buffer */
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    
    /*
    avio_alloc_context开头会读取部分数据探测流的信息,不会全部读取，除非设置的缓存过大
    av_read_frame会在读帧的时候调用avio_alloc_context中的read_packet方法取流数据，每隔avio_ctx_buffer_size调用一次，直至读完
    */
    avio_ctx = avio_alloc_context(avio_ctx_buffer,
                                   (int)avio_ctx_buffer_size,
                                   0,
                                   &bd,
                                   &read_packet,
                                   NULL,
                                   NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    fmt_ctx->pb = avio_ctx;
    
    /* avformat_open_input打开输入流并读取头信息 */
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto ksfault;
    }
    
    /* avformat_find_stream_info获取流信息 */
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto ksfault;
    }
    
    av_dump_format(fmt_ctx, 0, input_filename, 0);
    
ksfault:
    /* avformat_close_input关闭输入流 */
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    /* av_freep释放申请的buffer，因为此时avio_ctx->buffer即为avio_ctx_buffer */
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
    }
    /* avio_context_free释放AVIOContext */
    avio_context_free(&avio_ctx);
     /* av_file_unmap,unmap和释放加载文件的buffer */
    av_file_unmap(buffer, buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return ret;
    }
    return 0;
}


