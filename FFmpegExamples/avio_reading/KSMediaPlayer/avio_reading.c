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


static int read_packet(void *opaque, uint8_t *buf, int buf_size) {
    struct buffer_data *bd = (struct buffer_data *)opaque;
    /* FFMIN(a,b) ((a) > (b) ? (b) : (a)) */
    buf_size = FFMIN(buf_size, bd->size);
    if (!buf_size) {
        return AVERROR_EOF;
    }
    /* void *memcpy(void *dst, const void *src, size_t n) */
    /* 由src指向地址为起始地址的连续n个字节的数据复制到以dst指向地址为起始地址的空间内。*/
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr += buf_size;
    bd->size -= buf_size;
    
    return buf_size;
}

int main_pro(char *url) {
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avios_ctx = NULL;
    uint8_t *buffer = NULL, *avios_ctx_buffer = NULL;
    size_t buffer_size, avios_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = {0};
    
    if (!url) {
        return -1;
    }
    input_filename = url;
    
    ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0) {
        goto ksfault;
    }
    
    bd.ptr = buffer;
    bd.size = buffer_size;
    
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    
    avios_ctx_buffer = av_malloc(avios_ctx_buffer_size);
    if (!avios_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    
    avios_ctx = avio_alloc_context(avios_ctx_buffer, avios_ctx_buffer_size, 0, &bd, &read_packet, NULL, NULL);
    if (!avios_ctx) {
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto ksfault;
    }
    
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto ksfault;
    }
    
    av_dump_format(fmt_ctx, 0, input_filename, 0);
    
ksfault:
    avformat_close_input(&fmt_ctx);
    if (avios_ctx) {
        av_free(&avios_ctx->buffer);
    }
    avio_context_free(&avios_ctx);
    
    av_file_unmap(buffer, buffer_size);
    if (ret < 0) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return ret;
    }
    return 0;
}


