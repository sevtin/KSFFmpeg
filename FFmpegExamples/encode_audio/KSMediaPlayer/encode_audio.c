//
//  encode_audio.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/7.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "encode_audio.h"
#include <stdio.h>
#include <stdlib.h>

#include "libavcodec/avcodec.h"

#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"

/* check that a given sample format is supported by the encoder */
//检查编码器是否支持给定的样本格式
static int check_sample_fmt(const AVCodec *codec,enum AVSampleFormat sample_fmt) {
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sample_fmt) {
            return 1;
        }
        p++;
    }
    return 0;
}

/* just pick the highest supported samplerate */
//只需选择支持的最高采样率
static int select_sample_rate(const AVCodec *codec) {
    const int *p;
    int best_samplerate = 0;
    /*
     codec->supported_samplerates:支持的声音采样频率的数组，NULL表示未知，数组由0结束
     */
    if (!codec->supported_samplerates) {
        return 44100;
    }
    
    p = codec->supported_samplerates;
    while (*p) {
        if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate)) {
            best_samplerate = *p;
        }
        p++;
    }
    return best_samplerate;
}

/* select layout with the highest channel count */
//选择具有最高通道数的布局
static int select_channel_layout(const AVCodec *codec) {
    const uint64_t *p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels = 0;
    if (!codec->channel_layouts) {
        return AV_CH_LAYOUT_STEREO;
    }
    
    p = codec->channel_layouts;
    while (*p) {
        //根据通道的layout返回通道的个数
        int nb_channels = av_get_channel_layout_nb_channels(*p);
        if (nb_channels > best_nb_channels) {
            best_ch_layout = *p;
            best_nb_channels = nb_channels;
        }
        p++;
    }
    return best_ch_layout;
}

static int encode(AVCodecContext *ctx,AVFrame *frame,AVPacket *pkt, FILE *output) {
    int ret;
    /* send the frame for encoding */
    //负责将未压缩的AVFrame音视频数据给编码器。
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        return -1;
    }
    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0 ) {
        /*
         接受编码后的AVPacket
         */
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            return -1;
        }
        /* write to rawvideo file */
        /*
        把ptr所指向的数组中的数据写入到给定流stream中。
        size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)

        ptr-- 这是指向要被写入的元素数组的指针。
        size-- 这是要被写入的每个元素的大小，以字节为单位。
        nmemb-- 这是元素的个数，每个元素的大小为 size 字节。
        stream-- 这是指向 FILE 对象的指针，该 FILE 对象指定了一个输出流。
        */
        fwrite(pkt->data, 1, pkt->size, output);
        //减少引用计数
        av_packet_unref(pkt);
    }
    return 0;
}

int encode_audio_port(char *url) {
    const char *filename;
    //编解码器
    const AVCodec *codec;
    //编解码器上下文
    AVCodecContext *dec_ctx;
    //帧
    AVFrame *frame;
    //包
    AVPacket *pkt;
    int i, j, k, ret;
    FILE *file;
    uint16_t *samples;
    float t,tincr;
    
    if (!url) {
        ret = -1;
        goto ksfault;
    }
    filename = url;
    
    /* find the MP2 encoder */
    //查找编码器
    codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        ret = -1;
        goto ksfault;
    }
    //初始化编解码器上下文
    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        ret = -1;
        goto ksfault;
    }
    
    /* put sample parameters */
    //放入样本参数
    dec_ctx->bit_rate = 64000;
    /* check that the encoder supports s16 pcm input */
    //检查编码器是否支持s16 pcm输入
    dec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    
    if (!check_sample_fmt(codec, dec_ctx->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
               av_get_sample_fmt_name(dec_ctx->sample_fmt));
        ret = -1;
        goto ksfault;
    }
    
    /* select other audio parameters supported by the encoder */
    //选择编码器支持的其他音频参数
    dec_ctx->sample_rate = select_sample_rate(codec);//采样率,常用44100
    dec_ctx->channel_layout = select_channel_layout(codec);//声道布局，常用AV_CH_LAYOUT_STEREO，双声道
    dec_ctx->channels = av_get_channel_layout_nb_channels(dec_ctx->channel_layout);//声道数
    
    /* open it */
    /*
     打开编码器
     avctx：需要初始化的AVCodecContext。
     codec：输入的AVCodec
     options：一些选项。例如使用libx264编码的时候，“preset”，“tune”等都可以通过该参数设置。
     */
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        ret = -1;
        goto ksfault;
    }
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Could not open %s\n", filename);
        goto ksfault;
    }
    
    /* packet for holding encoded output */
    //用于保存编码输出的数据包
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        goto ksfault;
    }
    
    /* frame containing input raw audio */
    //包含输入原始音频的帧
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        goto ksfault;
    }
    
    frame->nb_samples = dec_ctx->frame_size;//采样率大小
    frame->format = dec_ctx->sample_fmt;//采样格式
    frame->channel_layout = dec_ctx->channel_layout;//声道布局
    
    /* allocate the data buffers */
    /*
     为音频或视频数据分配新的缓冲区。
     调用本函数前，帧中的如下成员必须先设置好：
        format (视频像素格式或音频采样格式)
        width、height(视频画面和宽和高)
        nb_samples、channel_layout(音频单个声道中的采样点数目和声道布局)
     本函数会填充AVFrame.data和AVFrame.buf数组，如果有需要，还会分配和填充AVFrame.extended_data和AVFrame.extended_buf。
     对于planar格式，会为每个plane分配一个缓冲区。
     */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        goto ksfault;
    }
    
    /* encode a single tone sound */
    //编码单音
    t = 0;
    tincr = 2 * M_PI * 440.0 / dec_ctx->sample_rate;
    for (i = 0; i < 200; i++) {
        /* make sure the frame is writable -- makes a copy if the encoder
         * kept a reference internally */
        /*
         av_frame_make_writable：确保AVFrame是可写的，尽可能避免数据的复制。
         如果AVFrame不是是可写的，将分配新的buffer和复制数据。
         */
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            goto ksfault;
        }
        samples = (uint16_t*)frame->data[0];
        for (j = 0; j < dec_ctx->frame_size; j++) {
            samples[2*j] = (int)(sin(t) * 10000);
            
            for (k = 1; k < dec_ctx->channels; k++) {
                samples[2*j + k] = samples[2*j];
            }
            t += tincr;
        }
        encode(dec_ctx, frame, pkt, file);
    }
    
ksfault:
    /* flush the encoder */
    if (dec_ctx != NULL && pkt != NULL && file != NULL) {
        encode(dec_ctx, NULL, pkt, file);
    }
    fclose(file);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&dec_ctx);
    
    return 0;
}
