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
    
    //视频流
    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
        /*
         avcodec_decode_video2()的作用是解码一帧视频数据。输入一个压缩编码的结构体AVPacket，输出一个解码后的结构体AVFrame。该函数的声明位于libavcodec\avcodec.h
         int avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr, const AVPacket *avpkt);
         待解码的数据保存在avpkt->data中，大小为avpkt->size；解码完成后，picture用于保存输出图像数据。

         该方法的各个参数：
         AVCodecContext *avctx：编解码上下文环境，定义了编解码操作的一些细节；
         AVFrame *picture：输出参数；传递到该方法的对象本身必须在外部由av_frame_alloc()分配空间，而实际解码过后的数据储存区将由AVCodecContext.get_buffer2()分配；AVCodecContext.refcounted_frames表示该frame的引用计数，当这个值为1时，表示有另外一帧将该帧用作参考帧，而且参考帧返回给调用者；当参考完成时，调用者需要调用av_frame_unref()方法解除对该帧的参考；av_frame_is_writable()可以通过返回值是否为1来验证该帧是否可写。
         int *got_picture_ptr：该值为0表明没有图像可以解码，否则表明有图像可以解码；
         const AVPacket *avpkt：输入参数，包含待解码数据。
         */
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
            /**
             * Copy image in src_data to dst_data.拷贝图像从源数据to目标数据
             *
             * @param dst_linesizes linesizes for the image in dst_data
             * @param src_linesizes linesizes for the image in src_data
             
            void av_image_copy(uint8_t *dst_data[4], int dst_linesizes[4],
                               const uint8_t *src_data[4], const int src_linesizes[4],
                               enum AVPixelFormat pix_fmt, int width, int height);
            */
            av_image_copy(video_dst_data,
                          video_dst_linesize,
                          (const uint8_t **)(frame->data),
                          frame->linesize,
                          pix_fmt,
                          width,
                          height);
            
            /* write to rawvideo file */
            /*
            把ptr所指向的数组中的数据写入到给定流stream中。
            size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)

            ptr-- 这是指向要被写入的元素数组的指针。
            size-- 这是要被写入的每个元素的大小，以字节为单位。
            nmemb-- 这是元素的个数，每个元素的大小为 size 字节。
            stream-- 这是指向 FILE 对象的指针，该 FILE 对象指定了一个输出流。
            */
            fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
        }
    }
    else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */
        /* int avcodec_decode_audio4(AVCodecContext *avctx, AVFrame *frame,
        int *got_frame_ptr, const AVPacket *avpkt);
         音频解码API avcodec_decode_audio4在新版中已废弃，替换为使用更为简单的avcodec_send_packet和avcodec_receive_frame
         
         *@param avctx 编解码器上下文
         *@param [out] frame用于存储解码音频样本的AVFrame
         *@param [out] got_frame_ptr如果没有帧可以解码则为零，否则为非零
         *@param [in] avpkt包含输入缓冲区的输入AVPacket
         *@return 如果在解码期间发生错误，则返回否定错误代码，否则返回从输入AVPacket消耗的字节数。
         */
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
            /*
             av_get_bytes_per_sample: 获取每个sample中的字节数。
             
             如果音频，样本：s16;采样率：44100；声道：2。
             av_get_bytes_per_sample(s16) == 2;
             1：假设从麦克风或者文件读出来的通过av_read_frame得到一个数据总量是88200个字节。
             这个88200个字节是和帧无关的数据量。
             
             1) AAC:
             nb_samples和frame_size = 1024
             一帧数据量：1024*2*av_get_bytes_per_sample(s16) = 4096个字节。
             会编码：88200/(1024*2*av_get_bytes_per_sample(s16)) = 21.5帧数据
             2) MP3:
             nb_samples和frame_size = 1152
             一帧数据量：1152*2*av_get_bytes_per_sample(s16) = 4608个字节。
             MP3:则会编码：88200/(1152*2*av_get_bytes_per_sample(s16)) = 19.1帧数据
             */
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
            /*
             
             */
            fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
        }
    }
    
	/* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame && refcount) {
        /*
         AVFrame通常只需分配一次，然后可以多次重用，每次重用前应调用av_frame_unref()将frame复位到原始的干净可用的状态。
         */
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
    
    /*
     获取音视频/字幕的流索引stream_index
     找到最好的视频流，通过FFmpeg的一系列算法，找到最好的默认的音视频流，主要用于播放器等需要程序自行识别选择视频流播放。
     */
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
        // 获取数据量对应的解码器
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        
        /* Allocate a codec context for the decoder */
        // 为解码器分配编解码器上下文
        *dec_ctx = avcodec_alloc_context3(dec);//需要使用avcodec_free_context释放
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        /* Copy codec parameters from input stream to output codec context */
        // 将编解码器参数从输入流复制到输出编解码器上下文
        /*
         历史说明
             以往FFmpeg版本中保存视音频流信息参数是AVStream结构体中的AVCodecContext字段。当前FFmpeg版本是3.4，新的版本已经将AVStream结构体中的AVCodecContext字段定义为废弃属性。因此无法像以前旧的版本直接通过如下的代码获取到AVCodecContext结构体参数：
         AVCodecContext* pAVCodecContext = avcodec_alloc_context3(NULL);
         pAVCodecContext = pAVFormatContext->streams[videoStream]->codec;
             当前版本保存视音频流信息的结构体AVCodecParameters，FFmpeg提供了函数avcodec_parameters_to_context将音频流信息拷贝到新的AVCodecContext结构体中

         函数说明
             将AVCodecParameters结构体中码流参数拷贝到AVCodecContext结构体中，并且重新拷贝一份extradata内容，涉及到的视频的关键参数有format, width, height, codec_type等，这些参数在优化avformat_find_stream_info函数的时候，手动指定该参数通过InitDecoder函数解码统一指定H264，分辨率是1920*1080
         */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }
        /* Init the decoders, with or without reference counting */
        // 在有或没有参考计数的情况下初始化解码器
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

// 获取采样格式
static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt) {
    int i;
    struct sample_fmt_entry {
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
    /* #define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0) */
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
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
        ret = -1;
        goto ksfault;
    }
    
    /* retrieve stream information */
    //检索流信息
    //读取一部分视音频数据并且获得一些相关的信息
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        ret = -1;
        goto ksfault;
    }
    /* open input file, and allocate format context */
    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        video_dst_file = fopen(video_dst_filename, "wb");
        if (!video_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
            ret = -1;
            goto ksfault;
        }
        
        /* allocate image where the decoded image will be put */
        width = video_dec_ctx->width;
        height = video_dec_ctx->height;
        pix_fmt = video_dec_ctx->pix_fmt;
        
        /*
         ffmpeg中av_image_alloc():照指定的宽、高、像素格式来分析图像内存。
         参数说明：
           pointers[4]：保存图像通道的地址。如果是RGB，则前三个指针分别指向R,G,B的内存地址。第四个指针保留不用
           linesizes[4]：保存图像每个通道的内存对齐的步长，即一行的对齐内存的宽度，此值大小等于图像宽度。
           w:              要申请内存的图像宽度。
           h:              要申请内存的图像高度。
           pix_fmt:        要申请内存的图像的像素格式。
           align:          用于内存对齐的值。
           返回值：所申请的内存空间的总大小。如果是负值，表示申请失败。
         */
        ret = av_image_alloc(video_dst_data,
                             video_dst_linesize,
                             width,
                             height,
                             pix_fmt,
                             1);
        
        if (ret < 0) {
            fprintf(stderr, "Could not allocate raw video buffer\n");
            ret = -1;
            goto ksfault;
        }
        video_dst_bufsize = ret;
    }
    
    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        audio_dst_file = fopen(audio_dst_filename, "wb");
        if (!audio_dst_file) {
            fprintf(stderr, "Could not open destination file %s\n", audio_dst_filename);
            ret = -1;
            goto ksfault;
        }
    }
    
     /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    if (!audio_stream && !video_stream) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        ret = -1;
        goto ksfault;
    }
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto ksfault;
    }
    
    /* initialize packet, set data to NULL, let the demuxer fill it 初始化数据包，将数据设置为NULL，让多路分配器填充它 */
    /*
     void av_init_packet(AVPacket *pkt):初始化packet的值为默认值，该函数不会影响data引用的数据缓存空间和size，需要单独处理。
     int av_new_packet(AVPacket *pkt, int size):av_init_packet的增强版，不但会初始化字段，还为data分配了存储空间
     AVPacket *av_packet_alloc(void):创建一个AVPacket，将其字段设为默认值（data为空，没有数据缓存空间）。
     */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    if (video_stream) {
        printf("Demuxing video from file '%s' into '%s'\n", src_filename, video_dst_filename);
    }
    if (audio_stream) {
        printf("Demuxing audio from file '%s' into '%s'\n", src_filename, audio_dst_filename);
    }
    
    /* read frames from the file */
    /*
     int av_read_frame(AVFormatContext *s, AVPacket *pkt);
        s：输入的AVFormatContext
        pkt：输出的AVPacket(这个值不能传NULL，必须是一个空间)
        返回值：return 0 is OK, <0 on error or end of file
     */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        do {
            ret = decode_packet(&got_frame, 0);
            if (ret < 0) {
                break;
            }
            pkt.data += ret;
            pkt.size -= ret;
        }while (pkt.size > 0);
        /*
         av_packet_ref():增加引用计数
         av_packet_unref():减少引用计数
         */
        av_packet_unref(&orig_pkt);
    }
    
    pkt.data = NULL;
    pkt.size = 0;
    do{
        decode_packet(&got_frame, 1);
    }while (got_frame);
    printf("Demuxing succeeded.\n");
    
    if (video_stream) {
        printf("Play the output video file with the command:\n"
               "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
               av_get_pix_fmt_name(pix_fmt), width, height,
               video_dst_filename);
    }
    if (audio_stream) {
        enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;
        int n_channels = audio_dec_ctx->channels;
        const char *fmt;
        
        //判断是否是 planar类型
        if (av_sample_fmt_is_planar(sfmt)) {
            const char *packed = av_get_sample_fmt_name(sfmt);
            printf("Warning: the sample format the decoder produced is planar "
            "(%s). This example will output the first channel only.\n",
            packed ? packed : "?");
            //根据格式名字（字符串）获取相应的枚举值
            sfmt = av_get_packed_sample_fmt(sfmt);
            n_channels = 1;
        }
        //获取采样格式
        if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0) {
            goto ksfault;
        }
        printf("Play the output audio file with the command:\n"
        "ffplay -f %s -ac %d -ar %d %s\n",
        fmt, n_channels, audio_dec_ctx->sample_rate,
        audio_dst_filename);
    }
    
ksfault:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    if (video_dst_file) {
        fclose(video_dst_file);
    }
    if (audio_dst_file) {
        fclose(audio_dst_file);
    }
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
    
    return ret < 0;
}
