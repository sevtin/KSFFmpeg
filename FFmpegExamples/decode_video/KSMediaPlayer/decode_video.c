//
//  decode_video.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/6.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "decode_video.h"
#include <stdlib.h>
#include <string.h>
#include "libavcodec/avcodec.h"

#define INBUF_SIZE 4096

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename) {
    FILE *file;
    int i;
    file = fopen(filename, "w");
    fprintf(file, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++) {
        /*
         把ptr所指向的数组中的数据写入到给定流stream中。
         ptr-- 这是指向要被写入的元素数组的指针。
         size-- 这是要被写入的每个元素的大小，以字节为单位。
         nmemb-- 这是元素的个数，每个元素的大小为 size 字节。
         stream-- 这是指向 FILE 对象的指针，该 FILE 对象指定了一个输出流。
         */
        fwrite(buf + i * wrap, 1, xsize, file);
    }
    fclose(file);
}

static int decode(AVCodecContext *dec_ctx, AVFrame *frame,AVPacket *pkt,const char *filename) {
    char buf[1024];
    int ret;
    
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return ret;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return -1;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return ret;
        }
        printf("saving frame %3d\n", dec_ctx->frame_number);
        /*
         fflush(stdin)刷新标准输入缓冲区，把输入缓冲区里的东西丢弃[非标准]
         fflush(stdout)刷新标准输出缓冲区，把输出缓冲区里的东西打印到标准输出设备上
         */
        fflush(stdout);
        
        /* the picture is allocated by the decoder. no need to
        free it */
        snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
        pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
    }
    return 0;
}
