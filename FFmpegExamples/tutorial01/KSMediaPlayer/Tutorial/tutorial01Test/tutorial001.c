//
//  tutorial001.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/15.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "tutorial001.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/hwcontext.h"
#include "libavutil/imgutils.h"

static int decode_write_video(AVCodecContext *avctx, AVPacket *packet, int hw_pix_fmt, FILE *output_file) {
    AVFrame *frame = NULL;
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
        if (!(frame = av_frame_alloc()) ) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
            goto KSEND;
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
            return 0;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
            goto KSEND;
        }

        //通过指定像素格式、图像宽、图像高来计算所需的内存大小
        /*
         int av_image_get_buffer_size(enum AVPixelFormat pix_fmt, int width, int height, int align);
         align:此参数是设定内存对齐的对齐数，也就是按多大的字节进行内存对齐。比如设置为1，表示按1字节对齐，那么得到的结果就是与实际的内存大小一样。再比如设置为4，表示按4字节对齐。也就是内存的起始地址必须是4的整倍数。
         */
        size = av_image_get_buffer_size(frame->format,
                                        frame->width,
                                        frame->height,
                                        1);
        buffer = av_malloc(size);
        if (!buffer) {
            fprintf(stderr, "Can not alloc buffer\n");
            ret = AVERROR(ENOMEM);
            goto KSEND;
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
                                      (const uint8_t * const *)frame->data,
                                      (const int *)frame->linesize,
                                      frame->format,
                                      frame->width,
                                      frame->height,
                                      1);
        if (ret < 0) {
            fprintf(stderr, "Can not copy image to buffer\n");
            goto KSEND;
        }
        
        if ((ret = (int)fwrite(buffer, 1, size, output_file)) < 0) {
            fprintf(stderr, "Failed to dump raw data.\n");
            goto KSEND;
        }
        
    KSEND:
        /*
         AVFrame对象必须调用av_frame_alloc()在堆上分配，注意此处指的是AVFrame对象本身，AVFrame对象必须调用av_frame_free()进行销毁。
         AVFrame通常只需分配一次，然后可以多次重用，每次重用前应调用av_frame_unref()将frame复位到原始的干净可用的状态。
         */
        av_frame_free(&frame);
        av_freep(&buffer);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

int tutorial001_port(char *src_url, char *dst_url, char *device_type) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVStream *st = NULL;
    AVCodec *pCodec = NULL;
    AVPacket pkt;
    AVFrame *pFrame;
    int i, ret, size;
    int videoStream;
    int hw_pix_fmt;
    uint8_t *buffer;
    
    FILE *dst_file = NULL;
    
    if (!src_url || !dst_url) {
        return -1;
    }
    
    av_register_all();
    
    // Open video file
    // 数读取文件的头部并且把信息保存到我们给的AVFormatContext结构体中
    // 最后三个参数用来指定特殊的文件格式，缓冲大小和格式参数，但如果把它们设置为空 NULL 或者 0，libavformat 将自动检测这些参数。
    // 这个函数只是检测了文件的头部
    ret = avformat_open_input(&pFormatCtx, src_url, NULL, NULL);
    if (ret != 0) {
        printf(" Couldn’t open file");
        return -1;
    }
    
    // Retrieve stream information
    // 检查在文件中的流的信息
    ret = avformat_find_stream_info(pFormatCtx, NULL);
    if (ret < 0) {
        goto KSEND;
    }
    av_dump_format(pFormatCtx, 0, src_url, 0);
    // 现在 pFormatCtx->streams 仅仅是一组大小为 pFormatCtx->nb_streams 的指针，所以让我们先跳过它直到我们找到一个视频流。
    videoStream = -1;
    ret = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input type '%d'\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO), AVMEDIA_TYPE_VIDEO);
        goto KSEND;
    }
    videoStream = ret;
    
    // Get a pointer to the codec context for the video stream
    st = pFormatCtx->streams[videoStream];
    // 获取数据流对应的解码器
    pCodec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!pCodec) {
        goto KSEND;
    }
    
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        goto KSEND;
    }
    
    if ((ret = avcodec_parameters_to_context(pCodecCtx, st->codecpar)) < 0) {
        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return ret;
    }
    
    // Open codec
    ret = avcodec_open2(pCodecCtx, pCodec, NULL);
    if (ret < 0) {
        goto KSEND;
    }
    
    // Allocate video frame
    pFrame = av_frame_alloc();
    if (!pFrame) {
        goto KSEND;
    }
    /*
     因为我们准备输出保存 24 位 RGB 色的 PPM 文件，我们必需把帧的格式从原来的转换为 RGB。FFMPEG 将
     为我们做这些转换。在大多数项目中（包括我们的这个）我们都想把原始的帧转换成一个特定的格式。让我们先
     为转换来申请一帧的内存。
     
    pFrameRGB = av_frame_alloc();
    if (!pFrameRGB) {
        goto KSEND;
    }
    */
    
    enum AVHWDeviceType type = av_hwdevice_find_type_by_name(device_type);
    for (i = 0; ; i++) {
        //检索编解码器支持的硬件配置。
        const AVCodecHWConfig *config = avcodec_get_hw_config(pCodec, i);
        if (!config) {
            goto KSEND;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }
    dst_file = fopen(dst_url, "wb+");
    
    while (ret >= 0) {
        //依次读取数据包
        if ((ret = av_read_frame(pFormatCtx, &pkt)) < 0) {
            break;
        }
        if (videoStream == pkt.stream_index) {
            decode_write_video(pCodecCtx, &pkt, hw_pix_fmt, dst_file);
        }
        //减少引用计数
        av_packet_unref(&pkt);
    }
    
    
    /*
    while (av_read_frame(pFormatCtx, &pkt) == 0) {
        avcodec_send_packet(pCodecCtx, &pkt);
        while (1) {
            ret = avcodec_receive_frame(pCodecCtx, pFrame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            }
            else if (ret < 0) {
                fprintf(stderr, "Error while decoding\n");
                goto KSEND;
            }
            size = av_image_get_buffer_size(pFrame->format,
                                            pFrame->width,
                                            pFrame->height,
                                            1);
            buffer = av_malloc(size);
            ret = av_image_copy_to_buffer(buffer, size, (const uint8_t * const *)pFrame->data,
                                          (const int *)pFrame->linesize,
                                          hw_pix_fmt, pFrame->width,
                                          pFrame->height,
                                          1);
            if (ret < 0) {
                goto KSEND;
            }
            
            ret = (int)fwrite(buffer, 1, size, dst_file);
            if (ret < 0) {
                goto KSEND;
            }
            break;
        }
    }*/
    
KSEND:
    return 0;
}
