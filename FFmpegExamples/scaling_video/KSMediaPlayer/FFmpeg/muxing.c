//
//  muxing.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/10.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "muxing.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "libavutil/avassert.h"
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/mathematics.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#define STREAM_DURATION   10.0//视频流时长。以秒计数。
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
//单个输出AVStream的包装器
typedef struct OutputStream {
    //数据流
    AVStream *st;
    //编解码器上下文
    AVCodecContext *enc;
    
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    
    AVFrame *frame;
    AVFrame *tmp_frame;
    
    float t, tincr, tincr2;
    //视屏重采样
    struct SwsContext *sws_ctx;
    //音频重采样
    struct SwrContext *swr_ctx;
}OutputStream;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt) {
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

//将数据写入封装器
static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt) {
    /* rescale output packet timestamp values from codec to stream timebase */
    //编码器时间转换，将时间戳由编码器时基AVCodecContext.time_base 转化为流的时基AVStream.time_base
    //av_packet_rescale_ts()用于将AVPacket中各种时间值从一种时间基转换为另一种时间基。
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    
    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    //数据写入封装（缓存几帧自动识别dts写入）
    return  av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
//添加视频和音频流,指定视频编码器和音频编码器的参数
static void add_stream(OutputStream *ost, AVFormatContext *oc,AVCodec **codec,enum AVCodecID codec_id) {
    AVCodecContext *c;
    int i;
    /* find the encoder */
    //通过codec_id找到编码器。
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        goto ksend;
    }
    //将新的流添加到媒体文件。
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        goto ksend;
    }
    
    ost->st->id = oc->nb_streams - 1;
    //申请编解码器上下文
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        goto ksend;
    }
    
    ost->enc = c;
    
    switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO://音频
            //采样格式
            c->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            //码率
            c->bit_rate = 64000;
            //采样频率
            c->sample_rate = 44100;
            if ((*codec)->supported_samplerates) {
                c->sample_rate = (*codec)->supported_samplerates[0];
                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                    if ((*codec)->supported_samplerates[i] == 44100) {
                        c->sample_rate = 44100;
                    }
                }
            }
            //声道数
            c->channels  = av_get_channel_layout_nb_channels(c->channel_layout);
            //声道布局
            c->channel_layout = AV_CH_LAYOUT_STEREO;
            if ((*codec)->channel_layouts) {
                c->channel_layout = (*codec)->channel_layouts[0];
                for (i = 0; (*codec)->channel_layouts[i]; i++) {
                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO) {
                        c->channel_layout = AV_CH_LAYOUT_STEREO;
                    }
                }
            }
            c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
            //音频采样率，视频帧率。
            ost->st->time_base = (AVRational){1, c->sample_rate};
            break;
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = codec_id;//编解码器的id
            c->bit_rate = 400000;
            /* Resolution must be a multiple of two. */
            c->width = 352;//视频的宽高
            c->height = 288;
            /* timebase: This is the fundamental unit of time (in seconds) in terms
             * of which frame timestamps are represented. For fixed-fps content,
             * timebase should be 1/framerate and timestamp increments should be
             * identical to 1. */
            //音频采样率，视频帧率。
            ost->st->time_base = (AVRational){1,STREAM_FRAME_RATE};
            c->time_base = ost->st->time_base;
            
            //最多每十二帧发射一帧内帧
            c->gop_size = 12;/* emit one intra frame every twelve frames at most */
            c->pix_fmt = STREAM_PIX_FMT;//规定视频编码的方式为YUV420;可以见define定义。
            
            if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                /* just for testing, we also add B-frames */
                //只是为了测试，我们还添加了B帧
                c->max_b_frames = 2;
            }
            if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                /* Needed to avoid using macroblocks in which some coeffs overflow.
                 * This does not happen with normal video, it just happens here as
                 * the motion of the chroma plane does not match the luma plane. */
                /*需要避免使用一些coeff溢出的宏块。
                 *对于普通视频不会发生这种情况，它只是在这里发生，因为色度平面的运动与亮度平面不匹配。
                 */
                c->mb_decision = 2;
            }
            break;
        default:
            break;
    }
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
ksend:
    printf("");
}

/**************************************************************/
/* audio output */
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate,
                                  int nb_samples) {
    
    AVFrame *frame = av_frame_alloc();
    int ret;
    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        goto ksend;
    }
    //音频采样类型(即采样深度)
    frame->format = sample_fmt;
    //声道布局
    frame->channel_layout = channel_layout;
    //采样率
    frame->sample_rate = sample_rate;
    //音频的采样率
    frame->nb_samples = nb_samples;
    if (nb_samples) {
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
            fprintf(stderr, "Error allocating an audio buffer\n");
            goto ksend;
        }
    }
    return frame;
ksend:
    return NULL;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg) {
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;
    
    c = ost->enc;
    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    /*
     初始化一个视音频编解码器的AVCodecContext
     avctx：需要初始化的AVCodecContext。
     codec：输入的AVCodec
     options：一些选项。例如使用libx264编码的时候，“preset”，“tune”等都可以通过该参数设置。
     
     */
    ret = avcodec_open2(c, codec, &opt);//打开解码器
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        goto ksend;
    }
    
    /* init signal generator */
    ost->t = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    //每秒增加110 Hz的频率
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;
    
    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
        nb_samples = 10000;
    }
    else{
        nb_samples = c->frame_size;
    }
    
    ost->frame = alloc_audio_frame(c->sample_fmt,
                                   c->channel_layout,
                                   c->sample_rate,
                                   nb_samples);
    
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16,
                                       c->channel_layout,
                                       c->sample_rate,
                                       nb_samples);
    /* copy the stream parameters to the muxer */
    //将流参数复制到复用器
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        goto ksend;
    }
    
    /* create resampler context */
    //创建重采样器上下文
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        goto ksend;
    }
    
    /* set options */
    //设定选项
    av_opt_set_int(ost->swr_ctx, "in_channel_count", c->channels, 0);
    av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(ost->swr_ctx, "out_channel_count", c->channels, 0);
    av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);
    
    /* initialize the resampling context */
    //初始化重采样上下文
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        goto ksend;
    }
    
ksend:
    printf("");
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
//获取音频帧
static AVFrame *get_audio_frame(OutputStream *ost) {
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t *)frame->data[0];
    
    /* check if we want to generate more frames */
    //使用avframe中的pts以及时基来计算生成帧的时间，视频步进1/25 音频步进1024/44100
    //在音视频流复用（MUX）的时候比较时间戳，根据时间戳的先后顺序交叉写入音频包或视频包
    if (av_compare_ts(ost->next_pts, ost->enc->time_base, STREAM_DURATION, (AVRational){1,1}) >= 0) {
        return NULL;
    }
    
    for (j = 0; j < frame->nb_samples; j++) {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->channels; i++) {
            *q++ = v;
        }
        ost->t += ost->tincr;
        ost->tincr += ost->tincr2;
    }
    
    frame->pts = ost->next_pts;
    //音频pts自增
    ost->next_pts += frame->nb_samples;
    
    return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
/*
 *编码一个音频帧并将其发送到多路复用器
 *编码完成后返回1，否则返回0
 */
static int write_audio_frame(AVFormatContext *oc, OutputStream *ost) {
    AVCodecContext *c;
    //数据和大小必须为0
    AVPacket pkt = {0};// data and size must be 0;
    AVFrame *frame;
    int ret;
    int got_packet;
    int dst_nb_samples;
    
    av_init_packet(&pkt);
    c = ost->enc;
    
    frame = get_audio_frame(ost);
    
    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        /*使用重采样器将样本从本机格式转换为目标编解码器格式*/
        /*计算样本的目标数量*/
        /*
         int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding rnd);
         直接看代码, 它的作用是计算 "a * b / c" 的值并分五种方式来取整.
         用在FFmpeg中,
         则是将以 "时钟基c" 表示的 数值a 转换成以 "时钟基b" 来表示。

         一共有5种方式:
         AV_ROUND_ZERO     = 0, // Round toward zero.      趋近于0
         AV_ROUND_INF      = 1, // Round away from zero.   趋远于0
         AV_ROUND_DOWN     = 2, // Round toward -infinity. 趋于更小的整数
         AV_ROUND_UP       = 3, // Round toward +infinity. 趋于更大的整数
         AV_ROUND_NEAR_INF = 5, // Round to nearest and halfway cases away from zero. 四舍五入,小于0.5取值趋向0,大于0.5取值趋远于0
         */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                        c->sample_rate,
                                        c->sample_rate,
                                        AV_ROUND_UP);
        av_assert0(dst_nb_samples == frame->nb_samples);
        
        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        //确保AVFrame是可写的，尽可能避免数据的复制。如果AVFrame不是可写的，将分配新的buffer和复制数据
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0) {
            goto ksend;
        }
        /* convert to destination format */
        //转换为目标格式
        /*
         针对每一帧音频的处理。把一帧帧的音频作相应的重采样
         int swr_convert(struct SwrContext *s, uint8_t **out, int out_count, const uint8_t **in, int in_count);

         参数1：音频重采样的上下文
         参数2：输出的指针。传递的输出的数组
         参数3：输出的样本数量，不是字节数。单通道的样本数量。
         参数4：输入的数组，AVFrame解码出来的DATA
         参数5：输入的单通道的样本数量。
         */
        ret = swr_convert(ost->swr_ctx, ost->frame->data, dst_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
        
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            goto ksend;
        }
        frame = ost->frame;
        //时基转换 从1/采样率 –> 流时基
        /*
         av_rescale_q函数用于time_base之间转换，av_rescale_q(a,b,c)作用相当于执行a*b/c，通过设置b,c的值，可以很方便的实现time_base之间转换。
         
         av_rescale_q用于计算Packet的PTS。av_rescale_q的返回值是一个很大的整数，且每次计算的结果间隔很大。
         不同于avcodec_encode_video改变AVCodecContext *avctx的pts（小整数，且间隔小）。
         av_rescale_q(a,b,c)是用来把时间戳从一个时基调整到另外一个时基时候用的函数。
         它基本的动作是计算a*b/c，但是这个函数还是必需的，因为直接计算会有溢出的情况发生。
         AV_TIME_BASE_Q是AV_TIME_BASE作为分母后的版本。
         它们是很不相同的：AV_TIME_BASE * time_in_seconds = avcodec_timestamp
         而AV_TIME_BASE_Q * avcodec_timestamp = time_in_seconds
         （注意AV_TIME_BASE_Q实际上是一个AVRational对象，
         所以你必需使用avcodec中特定的q函数来处理它）。
         */
        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1,c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }
    //编码音频帧
    ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        fprintf(stderr, "Error encoding audio frame: %s\n", av_err2str(ret));
        goto ksend;
    }
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error while writing audio frame: %s\n",av_err2str(ret));
            goto ksend;
        }
    }
    return (frame || got_packet) ? 0 : 1;
ksend:
    return 0;
}

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height) {
    AVFrame *picture;
    int ret;
    
    picture = av_frame_alloc();
    if (!picture) {
        return NULL;
    }
    picture->format = pix_fmt;
    picture->width = width;
    picture->height = height;
    
    /* allocate the buffers for the frame data */
    //为帧数据分配缓冲区
    /*
     为音频或视频数据分配新的缓冲区。
     调用本函数前，帧中的如下成员必须先设置好：

     format (视频像素格式或音频采样格式)
     width、height(视频画面和宽和高)
     nb_samples、channel_layout(音频单个声道中的采样点数目和声道布局)
     本函数会填充AVFrame.data和AVFrame.buf数组，如果有需要，还会分配和填充AVFrame.extended_data和AVFrame.extended_buf。
     对于planar格式，会为每个plane分配一个缓冲区。
     */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        return NULL;
    }
    return picture;
}

/*
作用打开编码器，申请各种内存，包括：frame,src_picture,dst_picture,其中dst_pciture和frame是一回事。
*/
static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg) {
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);
    
    /* open the codec */
    //打开编码器
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        goto ksend;
    }
    
    /* allocate and init a re-usable frame */
    //分配并初始化可重用frame
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        goto ksend;
    }
    
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            goto ksend;
        }
    }
    
    /* copy the stream parameters to the muxer */
    //将流参数复制到复用器
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        goto ksend;
    }
ksend:
    printf("");
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index, int width, int height) {
    int x, y, i;
    i = frame_index;
    
    /* Y */
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;
            
        }
    }
    /* Cb and Cr */
    for (y = 0; y < height/2; y++) {
        for (x = 0; x < width/2; x++) {
            pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
            pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

static AVFrame *get_video_frame(OutputStream *ost) {
    AVCodecContext *c = ost->enc;
    /* check if we want to generate more frames */
    //比较当前时间是否是否大于设定时间 next_pts*time_base > STREAM_DURATION*1
    //相当于只产生10s的视频
    if (av_compare_ts(ost->next_pts, c->time_base, STREAM_DURATION, (AVRational){1,1}) >= 0) {
        return NULL;
    }
    
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    //确保AVFrame是可写的，尽可能避免数据的复制。如果AVFrame不是可写的，将分配新的buffer和复制数据
    if (av_frame_make_writable(ost->frame) < 0) {
        goto ksend;
    }
    
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        /* as we only generate a YUV420P picture, we must convert it
         * to the codec pixel format if needed */
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(c->width, c->height,
                                          AV_PIX_FMT_YUV420P,
                                          c->width, c->height,
                                          c->pix_fmt, SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr, "Could not initialize the conversion context\n");
                goto ksend;
            }
        }
        //格式缩放重采样
        fill_yuv_image(ost->tmp_frame, (int)ost->next_pts, c->width, c->height);
        sws_scale(ost->sws_ctx, (const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize, 0, c->height, ost->frame->data, ost->frame->linesize);
    }
    else{
        //根据next_pts 参数不一样，动态生成不同画面的frame
        fill_yuv_image(ost->frame, (int)ost->next_pts, c->width, c->height);
    }
    //pts 自增
    ost->frame->pts = ost->next_pts++;
    
ksend:
    printf("");
    return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */

static int write_video_frame(AVFormatContext *oc,OutputStream *ost) {
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = {0};
    
    c = ost->enc;
    
    frame = get_video_frame(ost);
    
    /*
    void av_init_packet(AVPacket *pkt):初始化packet的值为默认值，该函数不会影响data引用的数据缓存空间和size，需要单独处理。
    */
    av_init_packet(&pkt);
    
    /* encode the image */
    //将frame数据编码成pkt编码后的数据
    //编码一帧视频
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
        goto ksend;
    }
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
    }
    else{
        ret = 0;
    }
    if (ret < 0) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        goto ksend;
    }
    return (frame || got_packet) ? 0 : 1;
    
ksend:
    return 1;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost) {
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */

int muxing_port(char *src_url) {
    OutputStream video_st = { 0 }, audio_st = { 0 };
    const char *filename;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVCodec *audio_codec = NULL, *video_codec = NULL;
    int ret;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    int i;
    
    if (!src_url) {
        return -1;
    }
    filename = src_url;
    for (i = 2; i+1 < 2; i+=2) {
        if (!strcmp("", "-flags") || !strcmp("", "-fflags")) {
            av_dict_set(&opt, "", "", 0);
        }
    }
    
    /* allocate the output media context */
    //该函数通常是第一个调用的函数，函数可以初始化一个用于输出的AVFormatContext结构体
    
    /*
     avformat_alloc_output_context2:函数通常是第一个调用的函数（除了组件注册函数av_register_all()）。avformat_alloc_output_context2()函数可以初始化一个用于输出的AVFormatContext结构体。
     
     ctx：函数调用成功之后创建的AVFormatContext结构体。
     oformat：指定AVFormatContext中的AVOutputFormat，用于确定输出格式。如果指定为NULL，可以设定后两个参数（format_name或者filename）由FFmpeg猜测输出格式。
     PS：使用该参数需要自己手动获取AVOutputFormat，相对于使用后两个参数来说要麻烦一些。
     format_name：指定输出格式的名称。根据格式名称，FFmpeg会推测输出格式。输出格式可以是“flv”，“mkv”等等。
     filename：指定输出文件的名称。根据文件名称，FFmpeg会推测输出格式。文件名称可以是“xx.flv”，“yy.mkv”等等。
     函数执行成功的话，其返回值大于等于0。
     */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc) {
        return -1;
    }
    
    fmt = oc->oformat;
    
    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    /*使用默认格式编解码器添加音频和视频流
     *并初始化编解码器。 */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        //
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }
    
    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    /*现在已经设置了所有参数，我们可以打开音频和视频编解码器并分配必要的编码缓冲区。 */
    //如果产生AVStream成功了
    if (have_video) {
        open_video(oc, video_codec, &video_st, opt);
    }
    if (have_audio) {
        open_audio(oc, audio_codec, &audio_st, opt);
    }
    av_dump_format(oc, 0, filename, 1);
    
    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
            return -1;
        }
    }
    
    /* Write the stream header, if any. */
    //写视频文件的文件头
    /*
     avformat_write_header()用于写视频文件头，而av_write_trailer()用于写视频文件尾。
     s：用于输出的AVFormatContext。
     options：额外的选项，目前没有深入研究过，一般为NULL。
     */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
        return -1;
    }
    
    while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base, audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, &video_st);
        }
        else{
            encode_audio = !write_audio_frame(oc, &audio_st);
        }
    }
    
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    //av_write_trailer()用于写视频文件尾。
    av_write_trailer(oc);
    
    /* Close each codec. */
    if (have_video) {
        close_stream(oc, &video_st);
    }
    
    if (have_audio) {
        close_stream(oc, &audio_st);
    }
    
    if (!(fmt->flags & AVFMT_NOFILE)) {
        /* Close the output file. */
        avio_closep(&oc->pb);
    }
    
    /* free the stream */
    avformat_free_context(oc);
    
    return 0;
}
