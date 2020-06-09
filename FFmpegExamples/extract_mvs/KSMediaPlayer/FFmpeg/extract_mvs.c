//
//  extract_mvs.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/8.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "extract_mvs.h"
#include "libavutil/motion_vector.h"
#include "libavformat/avformat.h"

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL;
static AVStream *video_stream = NULL;
static const char *src_filename = NULL;

static int video_stream_idx = -1;
static AVFrame *frame = NULL;
static int video_frame_count = 0;

static int decode_packet(const AVPacket *pkt) {
    int ret = avcodec_send_packet(video_dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(ret));
        return ret;
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(video_dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(ret));
            return ret;
        }
        if (ret >= 0) {
            int i;
            /* 用于保存AVFrame辅助数据的结构。*/
            AVFrameSideData *sd;
            video_frame_count++;
            sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
            if (sd) {
                const AVMotionVector *mvs = (const AVMotionVector *)sd->data;
                for (i = 0; i < sd->size / sizeof(*mvs); i++) {
                    const AVMotionVector *mv = &mvs[i];
                    printf("%d,%2d,%2d,%2d,%4d,%4d,%4d,%4d,0x%"PRIx64"\n",
                           video_frame_count, mv->source,
                           mv->w, mv->h, mv->src_x, mv->src_y,
                           mv->dst_x, mv->dst_y, mv->flags);
                }
            }
            av_frame_unref(frame);
        }
    }
    return 0;
}

static int open_codec_context(AVFormatContext *fmt_ctx,enum AVMediaType type) {
    int ret;
    AVStream *st;
    AVCodecContext *dec_ctx = NULL;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    }
    else{
        int stream_idx = ret;
        st = fmt_ctx->streams[stream_idx];
        dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx) {
            fprintf(stderr, "Failed to allocate codec\n");
            return AVERROR(EINVAL);
        }
        
        ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters to codec context\n");
            return ret;
        }
        
        /* Init the video decoder */
        av_dict_set(&opts, "flags2", "+export_mvs", 0);
        if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        video_stream_idx = stream_idx;
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dec_ctx = dec_ctx;
    }
    return 0;
}

int extract_mvs_port(char *src_url) {
    int ret = 0;
    AVPacket pkt = { 0 };
    src_filename = src_url;
    
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        ret = -1;
        goto ksfault;
    }
    
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        ret = -1;
        goto ksfault;
    }
    open_codec_context(fmt_ctx, AVMEDIA_TYPE_VIDEO);
    
    av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    if (!video_stream) {
        fprintf(stderr, "Could not find video stream in the input, aborting\n");
        ret = -1;
        goto ksfault;
    }
    
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    
    printf("framenum,source,blockw,blockh,srcx,srcy,dstx,dsty,flags\n");
    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == video_stream_idx) {
            ret = decode_packet(&pkt);
        }
        av_packet_unref(&pkt);
        if (ret < 0) {
            break;
        }
    }
    
    /* flush cached frames */
    decode_packet(NULL);
    
ksfault:
    avcodec_free_context(&video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    
    return ret < 0;
}
