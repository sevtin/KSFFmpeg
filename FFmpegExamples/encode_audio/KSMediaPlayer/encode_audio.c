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
static int select_sample_rate(const AVCodec *codec) {
    const int *p;
    int best_samplerate = 0;
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
static int select_channel_layout(const AVCodec *codec) {
    const uint64_t *p;
    uint64_t best_ch_layout = 0;
    int best_nb_channels = 0;
    if (!codec->channel_layouts) {
        return AV_CH_LAYOUT_STEREO;
    }
    
    p = codec->channel_layouts;
    while (*p) {
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
    ret = avcodec_send_frame(ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending the frame to the encoder\n");
        return -1;
    }
    /* read all the available output packets (in general there may be any
     * number of them */
    while (ret >= 0 ) {
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            return -1;
        }
        fwrite(pkt->data, 1, pkt->size, output);
        av_packet_unref(pkt);
    }
    return 0;
}

int encode_audio_port(char *url) {
    const char *filename;
    const AVCodec *codec;
    AVCodecContext *dec_ctx;
    AVFrame *frame;
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
    codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto ksfault;
    }
    
    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        goto ksfault;
    }
    
    /* put sample parameters */
    dec_ctx->bit_rate = 64000;
    /* check that the encoder supports s16 pcm input */
    dec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    
    if (!check_sample_fmt(codec, dec_ctx->sample_fmt)) {
        fprintf(stderr, "Encoder does not support sample format %s",
               av_get_sample_fmt_name(dec_ctx->sample_fmt));
        goto ksfault;
    }
    
    /* select other audio parameters supported by the encoder */
    dec_ctx->sample_rate = select_sample_rate(codec);
    dec_ctx->channel_layout = select_channel_layout(codec);
    dec_ctx->channels = av_get_channel_layout_nb_channels(dec_ctx->channel_layout);
    
    /* open it */
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto ksfault;
    }
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Could not open %s\n", filename);
        goto ksfault;
    }
    
    /* packet for holding encoded output */
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "could not allocate the packet\n");
        goto ksfault;
    }
    
    /* frame containing input raw audio */
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate audio frame\n");
        goto ksfault;
    }
    
    frame->nb_samples = dec_ctx->frame_size;
    frame->format = dec_ctx->sample_fmt;
    frame->channel_layout = dec_ctx->channel_layout;
    
    /* allocate the data buffers */
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio data buffers\n");
        goto ksfault;
    }
    
    /* encode a single tone sound */
    t = 0;
    tincr = 2 * M_PI * 440.0 / dec_ctx->sample_rate;
    for (i = 0; i < 200; i++) {
        /* make sure the frame is writable -- makes a copy if the encoder
         * kept a reference internally */
        
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
