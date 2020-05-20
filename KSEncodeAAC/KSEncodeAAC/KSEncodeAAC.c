//
//  KSEncodeAAC.c
//  KSEncodeAAC
//
//  Created by saeipi on 2020/5/18.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSEncodeAAC.h"

static int rec_status = 0;

void update_status(int status) {
    rec_status = status;
}
/* encode audio data */
static void encode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt, FILE *output) {
    int ret = 0;
    
    /* 将数据送编码器 */
    ret = avcodec_send_frame(ctx, frame);
    
    /* 如果ret>=0说明数据设置成功 */
    while (ret >= 0) {
        /* 获取编码后的音频数据,如果成功，需要重复获取，直到失败为止 */
        ret = avcodec_receive_packet(ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        }else if (ret < 0) {
            printf("Error, encoding audio frame\n");
            return;
        }
        fwrite(pkt->data, 1, pkt->size, output);
        /* 冲洗流中的信息 */
        fflush(output);
    }
}


static AVCodecContext* open_coder() {
    /* 打开编码器 */
    AVCodec *codec = avcodec_find_encoder_by_name("libfdk_aac");
    if (!codec) {
        return NULL;
    }
    /* 创建 codec 上下文 */
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->sample_fmt = AV_SAMPLE_FMT_S16;//输入音频的采样大小
    codec_ctx->channel_layout = AV_CH_LAYOUT_STEREO;//输入音频的channel layout
    codec_ctx->channels = 2;//输入音频 channel 个数
    codec_ctx->sample_rate = 44100;//输入音频的采样率
    codec_ctx->bit_rate = 0;//AAC_LC: 128K, AAC HE: 64K, AAC HE V2: 32K
    codec_ctx->profile = FF_PROFILE_AAC_HE_V2;//
    
    /* 打开编码器 */
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        return NULL;
    }
    return codec_ctx;
}

static SwrContext* init_swr() {
    SwrContext *swr_ctx = NULL;
    swr_ctx = swr_alloc_set_opts(NULL,//ctx
                                 AV_CH_LAYOUT_STEREO,//输出channel布局
                                 AV_SAMPLE_FMT_S16,//输出的采样格式
                                 44100,
                                 AV_CH_LAYOUT_STEREO,//输入channel布局
                                 AV_SAMPLE_FMT_FLT,//输入的采样格式
                                 44100,//输入的采样率
                                 0,
                                 NULL);
    
    if (!swr_ctx) {
        return NULL;
    }
    if (swr_init(swr_ctx) < 0) {
        return NULL;
    }
    
    return swr_ctx;
}

AVFormatContext* open_dev() {
    int ret = 0;
    char errors[1024] = {0,};
    /* ctx */
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    
    //[[video device]:[audio device]]
    char *devicename = ":0";
    AVInputFormat *iformat = NULL;
    
    /* get format */
    iformat = av_find_input_format("avfoundation");
    if (!iformat) {
        return NULL;
    }
    
    /* open device */
    if ((ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options))) {
        av_strerror(ret, errors, 1024);
        fprintf(stderr, "Failed to open audio device, [%d]%s\n", ret, errors);
        return NULL;
    }
    return fmt_ctx;
}

static AVFrame* create_frame() {
    AVFrame *frame = NULL;
    
    /* 音频输入数据 */
    frame = av_frame_alloc();
    if (!frame) {
        printf("Error, No Memory!\n");
        goto KSFAULT;
    }
    
    frame->nb_samples = 512;//单通道一个音频帧的采样数
    frame->format = AV_SAMPLE_FMT_S16;//每个采样的大小
    frame->channel_layout = AV_CH_LAYOUT_STEREO;//channel layout
    /* alloc inner memory */
    /* align:建议在此处传递0 */
    av_frame_get_buffer(frame, 0);//512 * 2 * = 2048
    
    if (!frame->data[0]) {
        printf("Error, Failed to alloc buf in frame!\n");
        /* 内存泄漏 */
        goto KSFAULT;
    }
    return frame;
    
KSFAULT:
    if (frame) {
        av_frame_free(&frame);
    }
    return NULL;
}

static void alloc_data_resample(uint8_t ***src_data, int *src_linesize,uint8_t ***dst_data,int *dst_linesize) {
    /* 4096/4=1024/2=512 */
    /* 创建输入缓冲区 */
    av_samples_alloc_array_and_samples(src_data,//输出缓冲区地址
                                       src_linesize,//缓冲区的大小
                                       2,//通道个数
                                       512,//单通道采样个数
                                       AV_SAMPLE_FMT_FLT,//采样格式
                                       0);
    /* 创建输出缓冲区 */
    av_samples_alloc_array_and_samples(dst_data,//输出缓冲区地址
                                       dst_linesize,//缓冲区的大小
                                       2,//通道个数
                                       512,//单通道采样个数
                                       AV_SAMPLE_FMT_S16,//采样格式
                                       0);
}

static void free_data_resample(uint8_t **src_data,uint8_t **dst_data){
    if (src_data) {
        av_freep(&src_data[0]);
        av_freep(&src_data);
    }
    if (dst_data) {
        av_freep(&dst_data[0]);
        av_freep(&dst_data);
    }
}

static void read_data_encode(AVFormatContext *fmt_ctx, AVCodecContext *cdc_ctx, SwrContext *swr_ctx, FILE *outfile) {
    int ret = 0;
    
    /* pakcet*/
    AVPacket pkt;
    AVFrame *frame = NULL;
    AVPacket *newpkt = NULL;
    
    /* 重采样缓冲区 */
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    
    frame = create_frame();
    if (!frame) {
        goto KSFAULT;
    }
    
    /* 分配编码后的数据空间 */
    newpkt = av_packet_alloc();
    if (!newpkt) {
        printf("Error, Failed to alloc buf in frame!\n");
        goto KSFAULT;
    }
    
    /* 分配重采样输入/输出缓冲区 */
    alloc_data_resample(&src_data, &src_linesize, &dst_data, &dst_linesize);
    
    /* read data from device */
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && rec_status) {
        /* 进行内存拷贝，按字节拷贝的 */
        /* void *memcpy(void *dst, const void *src, size_t n) */
        memcpy((void *)src_data[0], (void *)pkt.data, pkt.size);
        /* 重采样 */
        swr_convert(swr_ctx,//重采样的上下文
                    dst_data,//输出结果缓冲区
                    512,//每个通道的采样数
                    (const uint8_t **)src_data,//输入缓冲区
                    512);//输入单个通道的采样数
        /* 将重采样的数据拷贝到 frame 中 */
        memcpy((void *)frame->data[0], dst_data[0], dst_linesize);
        /* encode */
        encode(cdc_ctx, frame, newpkt, outfile);
        /* release pkt */
        av_packet_unref(&pkt);
    }
    /* 强制将编码器缓冲区中的音频进行编码输出 */
    encode(cdc_ctx, NULL, newpkt, outfile);
KSFAULT:
    /* 释放 AVFrame 和 AVPacket */
    if (frame) {
        av_frame_free(&frame);
    }
    if (newpkt) {
        av_packet_free(&newpkt);
    }
    /* 释放重采样缓冲区*/
    free_data_resample(src_data, dst_data);
}

void record_audio() {
    /* context */
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *cdc_ctx = NULL;
    SwrContext *swr_ctx = NULL;
    
    av_log_set_level(AV_LOG_DEBUG);
    
    /* register audio device */
    avdevice_register_all();
    
    /* start record */
    rec_status = 1;
    
    /* create file */
    char *outpath = "/Users/saeipi/Downloads/File/audio.aac";
    FILE *outfile = fopen(outpath, "wb+");
    if (!outfile) {
        printf("Error, Failed to open file!\n");
        goto KSFAULT;
    }
    /* 打开设备 */
    fmt_ctx = open_dev();
    if (!fmt_ctx) {
        printf("Error, Failed to open device!\n");
        goto KSFAULT;
    }
    
    /* 打开编码器上下文 */
    cdc_ctx = open_coder();
    if (!cdc_ctx) {
        printf("Error, 打开编码器上下文失败!\n");
        goto KSFAULT;
    }
    /* 初始化重采样上下文 */
    swr_ctx = init_swr();
    if (!swr_ctx) {
        printf("Error, Failed to alloc buf in frame!\n");
        goto KSFAULT;
    }
    
    /* encode */
    read_data_encode(fmt_ctx, cdc_ctx, swr_ctx, outfile);
    
KSFAULT:
    /* 释放重采样的上下文 */
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    if (cdc_ctx) {
        avcodec_free_context(&cdc_ctx);
    }
    
    /* close device and release ctx*/
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    
    if (outfile) {
        /* close file */
        fclose(outfile);
    }
    av_log(NULL, AV_LOG_DEBUG, "finish!\n");
}

#if 0
int main(int argc, char *argv[])
{
    rec_audio();
    return 0;
}
#endif
