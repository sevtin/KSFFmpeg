//
//  tutorial01.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/11.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "tutorial01.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/hwcontext.h"
#include "libavutil/imgutils.h"

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

/// 打开编解码器上下文 返回0成功
/// @param fmt_ctx 格式上下文
/// @param dec_ctx inout参数:编解码器上下文
/// @param dec inout参数:编解码器
/// @param stream_index inout参数:流index
/// @param type 视屏/音频
static int open_codec_context(AVFormatContext *fmt_ctx,
                              AVCodecContext **dec_ctx,
                              AVCodec **dec,
                              int *stream_index,
                              enum AVMediaType type) {
    int ret;
    AVStream *st;
    AVDictionary *opts = NULL;
    /*
     获取音视频/字幕的流索引stream_index
     */
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input type '%d'\n", av_get_media_type_string(type), type);
        return ret;
    }
    else {
        *stream_index = ret;
        st = fmt_ctx->streams[*stream_index];
        
        //获取数据流对应的解码器
        *dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!*dec) {
            fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        
        //为解码器分配编解码器上下文
        *dec_ctx = avcodec_alloc_context3(*dec);//需要使用avcodec_free_context释放
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
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
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
            return ret;
        }
        /* Init the decoders, with or without reference counting */
        // 在有或没有参考计数的情况下初始化解码器
        if ((ret = avcodec_open2(*dec_ctx, *dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
    }
    av_dict_free(&opts);
    return 0;
}

static int decode_write_video(AVCodecContext *avctx, AVPacket *packet, int hw_pix_fmt, FILE *output_file) {
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

int tutorial01_port(char *src_url, char *dst_url, char *device_type) {
    // Initalizing these to NULL prevents segfaults!
    AVFormatContext   *pFormatCtx = NULL;
    int               i, ret;
    int               hw_pix_fmt;
    int               stream_index;
    AVCodecContext    *pCodecCtx = NULL;
    AVCodec           *pCodec = NULL;
    AVFrame           *pFrame = NULL;
    AVPacket          packet;
    uint8_t           *buffer = NULL;
    FILE              *output = NULL;
    
    if(!src_url || !dst_url) {
        printf("Please provide a movie file\n");
        return -1;
    }
    // Register all formats and codecs
    av_register_all();
    
    // Open video file
    if(avformat_open_input(&pFormatCtx, src_url, NULL, NULL) !=0 ) {
        return -1; // Couldn't open file
    }
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        return -1; // Couldn't find stream information
    }
    
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, src_url, 0);
    
    // Find the first video stream
    ret = open_codec_context(pFormatCtx, &pCodecCtx, &pCodec, &stream_index, AVMEDIA_TYPE_VIDEO);
    if (ret != 0) {
        goto ksend;
    }
    
    output = fopen(dst_url, "wb");
    if (!output) {
        goto ksend;
    }
    
    enum AVHWDeviceType type = av_hwdevice_find_type_by_name("videotoolbox");
    for (i = 0; ; i++) {
        //检索编解码器支持的硬件配置。
        const AVCodecHWConfig *config = avcodec_get_hw_config(pCodec, i);
        if (!config) {
            goto ksend;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }
    
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
        goto ksend;
    }
    pFrame = av_frame_alloc();
    /* actual decoding and dump the raw data */
    while (ret >= 0) {
        //依次读取数据包
        if ((ret = av_read_frame(pFormatCtx, &packet)) < 0) {
            break;
        }
        if (stream_index == packet.stream_index) {
            decode_write_video(pCodecCtx, &packet, hw_pix_fmt, output);
        }
        //减少引用计数
        av_packet_unref(&packet);
    }
    
ksend:
    fclose(output);
    
    // Free the RGB image
    av_free(buffer);

    // Free the YUV frame
    av_frame_free(&pFrame);
    
    // Close the codecs
    avcodec_close(pCodecCtx);
    
    // Close the video file
    avformat_close_input(&pFormatCtx);
    
    return 0;
}
