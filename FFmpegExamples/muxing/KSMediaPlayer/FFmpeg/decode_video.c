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

static int decode_video_execute(AVCodecContext *dec_ctx, AVFrame *frame,AVPacket *pkt,const char *filename) {
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
    //输入输出文件
    const char *filename,*outfilename;
    //编解码器
    const AVCodec *codec;
    //解析器上下文
    AVCodecParserContext *parser;
    //编解码器上下文
    AVCodecContext *dec_ctx;
    FILE *file;
    AVFrame *frame;
    /*
    AVPacket的size属性：
    size：data的大小。当buf不为NULL时，size + AV_INPUT_BUFFER_PADDING_SIZE 等于buf->size。AV_INPUT_BUFFER_PADDING_SIZE是用于解码的输入流的末尾必要的额外字节个数，需要它主要是因为一些优化的流读取器一次读取32或者64比特，可能会读取超过size大小内存的末尾。看代码知道AV_INPUT_BUFFER_PADDING_SIZE的宏定义为了64。
    */
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
    //查找编解码器
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto ksfault;
    }
    
    //初始化解析器上下文
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "parser not found\n");
        goto ksfault;
    }
    
    //初始化编解码器上下文
    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        goto ksfault;
    }
    
    /* For some codecs, such as msmpeg4 and mpeg4, width and height
       MUST be initialized there because this information is not
       available in the bitstream. */

    /* open it */
    //打开编解码器
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto ksfault;
    }
    
    //打开文件
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
        /*
        ize_t fread( void *buffer, size_t size, size_t count, FILE *stream );
        从给定输入流stream读取最多count个对象到数组buffer中（相当于以对每个对象调用size次fgetc），把buffer当作unsigned char数组并顺序保存结果。流的文件位置指示器前进读取的字节数。
        若出现错误，则流的文件位置指示器的位置不确定。若没有完整地读入最后一个元素，则其值不确定。
        
        buffer:指向要读取的数组中首个对象的指针
        size:每个对象的大小（单位是字节）
        count:要读取的对象个数
        stream:输入流
        */
        data_size = fread(inbuf, 1, INBUF_SIZE, file);
        if (!data_size) {
            break;
        }
        /* use the parser to split the data into frames */
        data = inbuf;
        while (data_size > 0) {
            /*
            /**
            * Parse a packet.
            *
            * @param s             parser context.解析器上下文
            * @param avctx         codec context.编解码器上下文
            * @param poutbuf       set to pointer to parsed buffer or NULL if not yet finished.设置为指向已解析缓冲区的指针；如果尚未完成，则为NULL
            * @param poutbuf_size  set to size of parsed buffer or zero if not yet finished.设置为已解析缓冲区的大小，如果尚未完成，则设置为零
            * @param buf           input buffer.输入缓冲区
            * @param buf_size      buffer size in bytes without the padding. I.e. the full buffer
                                   size is assumed to be buf_size + AV_INPUT_BUFFER_PADDING_SIZE.
                                   To signal EOF, this should be 0 (so that the last frame
                                   can be output).输入缓冲区大小
            * @param pts           input presentation timestamp.
            * @param dts           input decoding timestamp.
            * @param pos           input byte position in stream.
            * @return the number of bytes of the input bitstream used.//使用的输入比特流的字节数
            */
            ret = av_parser_parse2(parser,
                                   dec_ctx,
                                   &pkt->data,
                                   &pkt->size,
                                   data,
                                   data_size,
                                   AV_NOPTS_VALUE,
                                   AV_NOPTS_VALUE,//AV_NOPTS_VALUE:未定义的时间戳值
                                   0);
            if (ret < 0) {
                fprintf(stderr, "Error while parsing\n");
                goto ksfault;
            }
            data += ret;
            data_size -= ret;
            if (pkt->size) {
                decode_video_execute(dec_ctx, frame, pkt, outfilename);
            }
        }
    }
    
ksfault:
    if (dec_ctx && frame) {
        decode_video_execute(dec_ctx, frame, NULL, outfilename);
    }
    fclose(file);
    av_parser_close(parser);
    avcodec_free_context(&dec_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    
    return 0;
}
