//
//  encode_video.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/7.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "encode_video.h"
#include <stdlib.h>
#include <string.h>

#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"

/* check that a given sample format is supported by the encoder */
static int encode_video_execute(AVCodecContext *enc_ctx, AVFrame *frame, AVPacket *pkt, FILE *outfile) {
    int ret;
    /* send the frame to the encoder */
    if (frame) {
        printf("Send frame %3"PRId64"\n", frame->pts);
    }
    //负责将未压缩的AVFrame音视频数据给编码器。
    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return -1;
    }
    
    while (ret >= 0) {
        /*
        接收编码后的AVPacket
        */
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        }
        else if (ret < 0) {
            fprintf(stderr, "Error during encoding\n");
            return -1;
        }
        printf("Write packet %3"PRId64" (size=%5d)\n", pkt->pts, pkt->size);
        /*
        把ptr所指向的数组中的数据写入到给定流stream中。
        size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)

        ptr-- 这是指向要被写入的元素数组的指针。
        size-- 这是要被写入的每个元素的大小，以字节为单位。
        nmemb-- 这是元素的个数，每个元素的大小为 size 字节。
        stream-- 这是指向 FILE 对象的指针，该 FILE 对象指定了一个输出流。
        */
        fwrite(pkt->data, 1, pkt->size, outfile);
        //av_packet_unref():减少引用计数
        av_packet_unref(pkt);
    }
    return 0;
}

int encode_video_port(char *dst_url, char *cname) {
    const char *filename, *codec_name;
    const AVCodec *codec;
    AVCodecContext *cdc_ctx = NULL;
    int i, ret, x, y;
    FILE *file;
    AVFrame *frame;
    AVPacket *pkt;
    uint8_t endcode[] = { 0, 0, 1,0xb7 };
    
    if (!dst_url || !cname) {
        return -1;
    }
    
    filename = dst_url;
    codec_name = cname;
    
    /* find the codec_name encoder */
    codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        fprintf(stderr, "Codec '%s' not found\n", codec_name);
        return -1;
    }
    
    cdc_ctx = avcodec_alloc_context3(codec);
    if (!cdc_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }
    
    pkt = av_packet_alloc();
    if (!pkt) {
        return -1;
    }
    
    /* put sample parameters */
    cdc_ctx->bit_rate = 400000;//码率
    /* resolution must be a multiple of two */
    cdc_ctx->width = 352;
    cdc_ctx->height = 288;
    
    /* frames per second */
    cdc_ctx->time_base = (AVRational){1, 25};//时间单位
    cdc_ctx->framerate = (AVRational){25, 1};//帧速率
    
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    //gop_size:IDR-GOP大小，即最大I帧间隔，多少个I帧之间有一个IDR帧
    cdc_ctx->gop_size = 10;
    //max_b_frames:最大连续B帧数
    cdc_ctx->max_b_frames = 1;
    //pix_fmt:图像像素格式
    cdc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    
    if (codec->id == AV_CODEC_ID_H264) {
        //设置参数
        av_opt_set(cdc_ctx->priv_data, "preset", "slow", 0);
    }
    
    /* open it */
    //打开编解码器
    ret = avcodec_open2(cdc_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        goto ksfault;
    }
    
    file = fopen(filename, "wb");
    if (!file) {
        fprintf(stderr, "Could not open %s\n", filename);
        goto ksfault;
    }
    
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        goto ksfault;
    }
    
    frame->format = cdc_ctx->pix_fmt;
    frame->width = cdc_ctx->width;
    frame->height = cdc_ctx->height;
    
    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        goto ksfault;
    }
    
    /* encode 1 second of video */
    for (i = 0; i < 25; i++) {
        fflush(stdout);
        /* make sure the frame data is writable */
        /*
        av_frame_make_writable：确保AVFrame是可写的，尽可能避免数据的复制。
        如果AVFrame不是是可写的，将分配新的buffer和复制数据。
        */
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            goto ksfault;
        }
        /* prepare a dummy image */
        /* Y */
        for (y = 0; y < cdc_ctx->height; y++) {
            for (x = 0; x < cdc_ctx->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }
        /* Cb and Cr */
        for (y = 0; y < cdc_ctx->height/2; y++) {
            for (x = 0; x < cdc_ctx->width/2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 +x + i * 5;
            }
        }
        
        frame->pts = i;
        /* encode the image */
        encode_video_execute(cdc_ctx, frame, pkt, file);
    }
ksfault:
    /* flush the encoder */
    if (cdc_ctx && pkt && file) {
        encode_video_execute(cdc_ctx, NULL, pkt, file);
    }
    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), file);
    
    fclose(file);
    avcodec_free_context(&cdc_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    
    return 0;
}
