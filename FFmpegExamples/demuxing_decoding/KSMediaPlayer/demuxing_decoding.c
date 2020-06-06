//
//  demuxing_decoding.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/6.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "demuxing_decoding.h"
#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;

static const char *src_filename = NULL;
static const char *video_dst_filename = NULL;
static const char *audio_dst_filename = NULL;

static FILE *video_dst_file = NULL;
static FILE *audio_dst_file = NULL;

static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;
static int video_frame_count = 0;
static int audio_frame_count = 0;

/* Enable or disable frame reference counting. You are not supposed to support
* both paths in your application but pick the one most appropriate to your
* needs. Look for the use of refcount in this example to see what are the
* differences of API usage between them. */
static int refcount = 0;


static int decode_packet(int *got_frame, int cached) {
    int ret = 0;
    int decoded = pkt.size;
    *got_frame = 0;
    
    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        ret = avcodec_decode_video2(video_dec_ctx,
                                    frame,
                                    got_frame,
                                    &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding video frame (%s)\n", av_err2str(ret));
            return ret;
        }
        if (*got_frame) {
            if (frame->width != width
                || frame->height != height
                || frame->format != pix_fmt) {
                
                /* To handle this change, one could call av_image_alloc again and
                 * decode the following frames into another rawvideo file. */
                fprintf(stderr, "Error: Width, height and pixel format have to be "
                        "constant in a rawvideo file, but the width, height or "
                        "pixel format of the input video changed:\n"
                        "old: width = %d, height = %d, format = %s\n"
                        "new: width = %d, height = %d, format = %s\n",
                        width, height, av_get_pix_fmt_name(pix_fmt),
                        frame->width, frame->height,
                        av_get_pix_fmt_name(frame->format));
                
                return -1;
            }
            
            printf("video_frame%s n:%d coded_n:%d\n",
            cached ? "(cached)" : "",video_frame_count++, frame->coded_picture_number);
            
            /* copy decoded frame to destination buffer:
            * this is required since rawvideo expects non aligned data */
            
            av_image_copy(video_dst_data,
                          video_dst_linesize,
                          (const uint8_t **)(frame->data),
                          frame->linesize,
                          pix_fmt,
                          width,
                          height);
            
            /* write to rawvideo file */
            fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    }
    else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */
        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
        * called again with the remainder of the packet data.
        * Sample: fate-suite/lossless-audio/luckynight-partial.shn
        * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);
        
        if (*got_frame) {
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
            printf("audio_frame%s n:%d nb_samples:%d pts:%s\n",
                              cached ? "(cached)" : "",
                              audio_frame_count++, frame->nb_samples,
                              av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));
            /* Write the raw audio data samples of the first plane. This works
            * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
            * most audio decoders output planar audio, which uses a separate
            * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
            * In other words, this code will write only the first audio channel
            * in these cases.
            * You should use libswresample or libavfilter to convert the frame
            * to packed data. */
            fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
        }
    }
    
    if (*got_frame && refcount) {
        av_frame_unref(frame);
    }
    return decoded;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx,
                              AVFormatContext *fmt_ctx,
                              enum AVMediaType type) {
    int ret,stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    }
    else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        /* Copy codec parameters from input stream to output codec context */
        av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    return 0;
}

static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt) {
    int i;
    struct sample_fmt_entey {
        enum AVSampleFormat sample_fmt;
        const char *fmt_be, *fmt_le;
    }sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entey *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
    
    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}

int demuxing_decoding_port(char *val_src_filename,
                           char *val_video_dst_filename,
                           char *val_audio_dst_filename) {
    int ret = 0, got_frame;
    
    if (!val_src_filename
        || !val_video_dst_filename
        || !val_audio_dst_filename) {
        return -1;
    }
    
    src_filename = val_src_filename;
    video_dst_filename = val_video_dst_filename;
    audio_dst_filename = val_audio_dst_filename;
    
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        goto ksfault;
    }
    
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto ksfault;
    }
    /* open input file, and allocate format context */
    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        
    }
    
    
ksfault:
    return 0;
}
