//
//  hw_decode.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/9.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "hw_decode.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type) {
    int err = 0;
    //打开指定类型的设备并为其创建AVHWDeviceContext(创建硬件设备相关的上下文信息)
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    /*
     创建对AVBuffer的新引用。
     AVBufferRef *av_buffer_ref(AVBufferRef *buf)
     {
         AVBufferRef *ret = av_mallocz(sizeof(*ret));

         if (!ret)
             return NULL;

         *ret = *buf;

         atomic_fetch_add_explicit(&buf->buffer->refcount, 1, memory_order_relaxed);

         return ret;
     }
     
     av_buffer_ref()处理如下：
     a) *ret = *buf;一句将buf各成员值赋值给ret中对应成员，buf和ret将共用同一份AVBuffer缓冲区
     b) atomic_fetch_add_explicit(...);一句将AVBuffer缓冲区引用计数加1
     注意此处的关键点：共用缓冲区(缓冲区不拷贝)，缓冲区引用计数加1
     */
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt) {
            return *p;
        }
    }
	fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet) {
    AVFrame *frame = NULL, *sw_frame = NULL;
    AVFrame *tmp_frame = NULL;
    uint8_t *buffer = NULL;
    int size;
    int ret = 0;
    
    //将带有压缩数据的数据包发送到解码器
    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return ret;
    }
    
    while (1) {
        if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
            goto ksfail;
        }
        
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
        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            av_frame_free(&sw_frame);
            return 0;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
            goto ksfail;
        }
        if (frame->format == hw_pix_fmt) {
            /* retrieve data from GPU to CPU */
            /* 从GPU检索数据到CPU */
            // 从GPU取出解码的数据
            if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
                goto ksfail;
            }
            tmp_frame = sw_frame;
        }
        else{
            tmp_frame = frame;
        }
        //通过指定像素格式、图像宽、图像高来计算所需的内存大小
        /*
         int av_image_get_buffer_size(enum AVPixelFormat pix_fmt, int width, int height, int align);
         align:此参数是设定内存对齐的对齐数，也就是按多大的字节进行内存对齐。比如设置为1，表示按1字节对齐，那么得到的结果就是与实际的内存大小一样。再比如设置为4，表示按4字节对齐。也就是内存的起始地址必须是4的整倍数。
         */
        size = av_image_get_buffer_size(tmp_frame->format,
                                        tmp_frame->width,
                                        tmp_frame->height,
                                        1);
        buffer = av_malloc(size);
        if (!buffer) {
            fprintf(stderr, "Can not alloc buffer\n");
            ret = AVERROR(ENOMEM);
            goto ksfail;
        }
        
        /*
         int av_image_copy_to_buffer(uint8_t *dst, int dst_size,
         const uint8_t * const src_data[4], const int src_linesize[4],
         enum AVPixelFormat pix_fmt, int width, int height, int align);
         
        将图像数据从图像复制到缓冲区。
        
        av_image_get_buffer_size（）可用于计算所需的大小
        用于填充缓冲区。
        
        @param dst 将图像数据复制到的缓冲区
        @param dst_size dst 的字节大小
        @param src_data 指针包含源图像数据
        @param src_linesize 调整src_data中图像的大小
        @param pix_fmt 源图像的像素格式
        @param width 源图像的宽度（以像素为单位）
        @param height 源图像的高度（以像素为单位）
        @param 对齐dst的假定行大小对齐
        @返回写入dst的字节数或负值
        （错误代码）错误
        */
        ret = av_image_copy_to_buffer(buffer,
                                      size,
                                      (const uint8_t * const *)tmp_frame->data,
                                      (const int *)tmp_frame->linesize,
                                      tmp_frame->format,
                                      tmp_frame->width,
                                      tmp_frame->height,
                                      1);
        if (ret < 0) {
            fprintf(stderr, "Can not copy image to buffer\n");
            goto ksfail;
        }
        
        if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
            fprintf(stderr, "Failed to dump raw data.\n");
            goto ksfail;
        }
        
    ksfail:
        /*
         AVFrame对象必须调用av_frame_alloc()在堆上分配，注意此处指的是AVFrame对象本身，AVFrame对象必须调用av_frame_free()进行销毁。
         AVFrame通常只需分配一次，然后可以多次重用，每次重用前应调用av_frame_unref()将frame复位到原始的干净可用的状态。
         */
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_freep(&buffer);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int hw_decode_port(char *src_url, char *dst_url, char *device_type) {
    AVFormatContext *input_ctx = NULL;
    int video_stream, ret;
    AVStream *video = NULL;
    AVCodecContext *decoder_ctx = NULL;
    AVCodec *decoder = NULL;
    AVPacket packet;
    enum AVHWDeviceType type;
    int i;
    
    //确定解码器类型
    //MacOS和iOS可以固定写videotoolbox
    type = av_hwdevice_find_type_by_name(device_type);
    if (type == AV_HWDEVICE_TYPE_NONE) {
        fprintf(stderr, "Device type %s is not supported.\n", device_type);
        fprintf(stderr, "Available device types:");
        //遍历支持的设备类型。
        while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
            fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
        }
        fprintf(stderr, "\n");
        return -1;
    }
    
    /* open the input file */
    //打开输入文件
    if (avformat_open_input(&input_ctx, src_url, NULL, NULL) != 0) {
        fprintf(stderr, "Cannot open input file '%s'\n", src_url);
        return -1;
    }
    
    //读取一部分视音频数据并且获得一些相关的信息
    if (avformat_find_stream_info(input_ctx, NULL) < 0) {
        fprintf(stderr, "Cannot find input stream information.\n");
        return -1;
    }
    
    /* find the video stream information */
    //查找视频流信息
    ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ret < 0) {
        fprintf(stderr, "Cannot find a video stream in the input file\n");
        return -1;
    }
    
    video_stream = ret;
    
    for (i = 0; ; i++) {
        //检索编解码器支持的硬件配置。
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n",
                    decoder->name, av_hwdevice_get_type_name(type));
            return -1;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }
    
    //初始化编解码器上下文
    if (!(decoder_ctx = avcodec_alloc_context3(decoder))) {
        return AVERROR(ENOMEM);
    }
    
    video = input_ctx->streams[video_stream];
    //codecpar包含了大部分解码器相关的信息，这里是直接从AVCodecParameters复制到AVCodecContext
    //将AVCodecParameters结构体中码流参数拷贝到AVCodecContext结构体中，并且重新拷贝一份extradata内容，涉及到的视频的关键参数有format, width, height, codec_type等
    if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0) {
        return -1;
    }
    
    decoder_ctx->get_format = get_hw_format;
    
    if (hw_decoder_init(decoder_ctx, type) < 0) {
        return -1;
    }
    
    if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
        fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
        return -1;
    }
    
    /* open the file to dump raw data */
    output_file = fopen(dst_url, "w+");
    
    /* actual decoding and dump the raw data */
    while (ret >= 0) {
        //依次读取数据包
        if ((ret = av_read_frame(input_ctx, &packet)) < 0) {
            break;
        }
        if (video_stream == packet.stream_index) {
            ret = decode_write(decoder_ctx, &packet);
        }
        //减少引用计数
        av_packet_unref(&packet);
    }
    
    /* flush the decoder */
    packet.data = NULL;
    packet.size = 0;
    ret = decode_write(decoder_ctx, &packet);
    av_packet_unref(&packet);
    
    if (output_file) {
        fclose(output_file);
    }
    avcodec_free_context(&decoder_ctx);
    avformat_close_input(&input_ctx);
    av_buffer_unref(&hw_device_ctx);
    
    return 0;
}
