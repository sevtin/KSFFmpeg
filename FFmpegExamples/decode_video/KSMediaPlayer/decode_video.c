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
    
    //将带有压缩数据的数据包发送到解码器(发送数据到ffmepg，放到解码队列中)
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return ret;
    }
    
    //读取所有输出帧（通常可以有任意数量的输出帧）
    while (ret >= 0) {
        /*
         从解码器中获取解码的输出数据(将成功的解码队列中取出1个frame  (如果失败会返回０）)
         @参数 avctx 编码上下文
         @参数 frame 这将会指向从解码器分配的一个引用计数的视频或者音频帧（取决于解码类型）
         @注意该函数在处理其他事情之前会调用av_frame_unref(frame)

         @返回值
         0：成功，返回一帧数据
         AVERROR(EAGAIN)：当前输出无效，用户必须发送新的输入
         AVERROR_EOF：解码器已经完全刷新，当前没有多余的帧可以输出
         AVERROR(EINVAL)：解码器没有被打开，或者它是一个编码器
         其他负值：对应其他的解码错误
         */
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

int decode_video_port(char *inurl,char *outurl) {
    const char *filename,*outfilename;
    const AVCodec *codec;
    AVCodecParserContext *parser;
    AVCodecContext *dec_ctx;
    FILE *file;
    AVFrame *frame;
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t data_size;
    int ret;
    AVPacket *pkt;
    
    if (!inurl || !outurl) {
        return -1;
    }
    
    filename = inurl;
    outfilename = outurl;
    
    pkt = av_packet_alloc();
    if (!pkt) {
        return -1;
    }
    
     /* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    
    /* find the MPEG-1 video decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto ksfault;
    }
    
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        goto ksfault;
    }
    
    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        goto ksfault;
    }
    
    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    /* open it */
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto ksfault;
    }
    
    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Could not open %s\n", filename);
        goto ksfault;
    }
    
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        goto ksfault;
    }
    
    while (!feof(file)) {
        /* read raw data from the input file */
        data_size = fread(inbuf, 1, INBUF_SIZE, file);
        if (!data_size) {
            break;
        }
        /* use the parser to split the data into frames */
        data = inbuf;
        while (data_size > 0) {
            ret = av_parser_parse2(parser,
                                   dec_ctx,
                                   &pkt->data,
                                   &pkt->size,
                                   data,
                                   data_size,
                                   AV_NOPTS_VALUE,
                                   AV_NOPTS_VALUE,
                                   0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                goto ksfault;
            }
            data += ret;
            data_size -= ret;
            if (pkt->size) {
                decode(dec_ctx, frame, pkt, outfilename);
            }
        }
    }
    
ksfault:
    if (dec_ctx && frame) {
        decode(dec_ctx, frame, NULL, outfilename);
    }
    fclose(file);
    av_parser_close(parser);
    avcodec_free_context(&dec_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    
    return 0;
}
