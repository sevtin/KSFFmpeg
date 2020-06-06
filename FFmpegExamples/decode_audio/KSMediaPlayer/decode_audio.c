//
//  decode_audio.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/6.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "decode_audio.h"
#include <stdlib.h>
#include <string.h>

#include "libavutil/frame.h"
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static int decode_audio_execute(AVCodecContext *dec_ctx,AVPacket *pkt,AVFrame *frame,FILE *outfile) {
    int i,ch;
    int ret,data_size;
    
    /* send the packet with the compressed data to the decoder */
    //将带有压缩数据的数据包发送到解码器
    ret = avcodec_send_packet(dec_ctx, pkt);
    
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder\n");
        return ret;
    }
    
    /* read all the output frames (in general there may be any number of them */
    //读取所有输出帧（通常可以有任意数量的输出帧）
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return -1;
        }
        else if (ret < 0){
            fprintf(stderr, "Error during decoding\n");
            return ret;
        }
        data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            fprintf(stderr, "Failed to calculate data size\n");
            return -1;
        }
        for (i = 0; i < frame->nb_samples; i++) {
            for (ch = 0; ch < dec_ctx->channels; ch++) {
                fwrite(frame->data[ch] + data_size*i, 1, data_size, outfile);
            }
        }
    }
    
    return 0;
}

int decode_audio_port(char *inurl,char *outurl) {
    //输入输出文件
    const char *outfilename,*filename;
    //编解码器
    const AVCodec *codec;
    //编解码器上下文
    AVCodecContext *dec_ctx = NULL;
    //解析器上下文
    AVCodecParserContext *parser = NULL;
    int len,ret;
    FILE *infile,*outfile;
    /*
     AVPacket的size属性：
     size：data的大小。当buf不为NULL时，size + AV_INPUT_BUFFER_PADDING_SIZE 等于buf->size。AV_INPUT_BUFFER_PADDING_SIZE是用于解码的输入流的末尾必要的额外字节个数，需要它主要是因为一些优化的流读取器一次读取32或者64比特，可能会读取超过size大小内存的末尾。看代码知道AV_INPUT_BUFFER_PADDING_SIZE的宏定义为了64。
     */
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t data_size;
    AVPacket *pkt;
    AVFrame *decoded_frame = NULL;
    
    if (!inurl || !outurl) {
        return -1;
    }
    
    filename = inurl;
    outfilename = outurl;
    
    pkt = av_packet_alloc();
    
    /* find the MPEG audio decoder */
    //查找编解码器
    codec = avcodec_find_decoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto ksfault;
    }
    //初始化解析器上下文
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
        goto ksfault;
    }
    //初始化编解码器上下文
    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        goto ksfault;
    }
    
    /* open it */
    //打开编解码器
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto ksfault;
    }
    //打开文件
    infile = fopen(filename, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", filename);
        goto ksfault;
    }
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(dec_ctx);
        goto ksfault;
    }
    
    /* decode until eof */
    data = inbuf;
    /*
     ize_t fread( void *buffer, size_t size, size_t count, FILE *stream );
     从给定输入流stream读取最多count个对象到数组buffer中（相当于以对每个对象调用size次fgetc），把buffer当作unsigned char数组并顺序保存结果。流的文件位置指示器前进读取的字节数。
     若出现错误，则流的文件位置指示器的位置不确定。若没有完整地读入最后一个元素，则其值不确定。
     
     buffer:指向要读取的数组中首个对象的指针
     size:每个对象的大小（单位是字节）
     count:要读取的对象个数
     stream:输入流
     */
    data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, infile);
    while (data_size > 0) {
        //解码帧
        if (!decoded_frame) {
            //首次初始化
            if (!(decoded_frame = av_frame_alloc())) {
                fprintf(stderr, "Could not allocate audio frame\n");
                goto ksfault;
            }
        }
        
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
                               AV_NOPTS_VALUE,//AV_NOPTS_VALUE:未定义的时间戳值
                               AV_NOPTS_VALUE,
                               0);
        if (ret < 0) {
            fprintf(stderr, "Error while parsing\n");
            goto ksfault;
        }
        
        data += ret;
        data_size -= ret;
        printf("\n|------| %d : %d |------|\n",data,data_size);
        
        if (pkt->size) {
            decode_audio_execute(dec_ctx, pkt, decoded_frame, outfile);
        }
        if (data_size < AUDIO_REFILL_THRESH) {
            /*
             原型：void *memmove( void* dest, const void* src, size_t count );
             头文件：<string.h>
             功能：由src所指内存区域复制count个字节到dest所指内存区域。
             */
            memmove(inbuf, data, data_size);
            data = inbuf;
            len = fread(data + data_size, 1, AUDIO_INBUF_SIZE - data_size, infile);
            
            if (len > 0) {
                data_size += len;
            }
        }
    }
    
ksfault:
    /* flush the decoder */
    pkt->data = NULL;
    pkt->size = 0;
    if (!dec_ctx && !pkt && !decoded_frame && !outfile) {
        decode_audio_execute(dec_ctx, pkt, decoded_frame, outfile);
    }
    
    fclose(infile);
    fclose(outfile);
    avcodec_free_context(&dec_ctx);
    av_parser_close(parser);
    
    av_frame_free(&decoded_frame);
    av_packet_free(&pkt);
    
    return ret;
}
