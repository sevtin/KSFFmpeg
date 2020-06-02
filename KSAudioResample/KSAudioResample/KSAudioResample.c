//
//  KSAudioResample.c
//  KSAudioResample
//
//  Created by saeipi on 2020/5/18.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSAudioResample.h"

static int rec_status = 0;

void update_status(int status) {
    rec_status = status;
}

SwrContext* init_swr() {
    SwrContext *swr_ctx = NULL;
    printf("AV_CH_LAYOUT_STEREO: %d",AV_CH_LAYOUT_STEREO);
    /* channel, number */
    swr_ctx = swr_alloc_set_opts(NULL,//ctx
                                 AV_CH_LAYOUT_STEREO,//输出channel布局
                                 AV_SAMPLE_FMT_S16,//输出的采样格式
                                 44100,//采样率
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

void record_audio() {
    int ret = 0;
    char errors[1024] = {0,};
    
    /* 重采样缓冲区 */
    uint8_t **src_data = NULL;
    int src_linesize = 0;
    
    uint8_t **dst_data = NULL;
    int dst_linesize = 0;
    
    /* ctx */
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;
    
    /* pakcet */
    AVPacket pkt;
    
    /* ffmpeg -f avfoundation -list_devices true -i "" */
    /* [[video device]:[audio device]] */
    /* 只访问麦克风 */
    char *devicename = ":0";
    
    /* 解复用器对象: 输入文件容器格式*/
    AVInputFormat *iformat = NULL;
    
    /* 重采样结构体 */
    SwrContext *swr_ctx = NULL;
    
    /* set log level */
    av_log_set_level(AV_LOG_DEBUG);
    
    /* start record */
    rec_status = 1;
    
    /* register audio device */
    avdevice_register_all();
    
    /* get format*/
    /* 根据输入格式的简称找到AVInputFormat */
    /* 需要开启Audio访问权限*/
    iformat = av_find_input_format("avfoundation");
    if (!iformat) {
        av_log(NULL, AV_LOG_ERROR, "AVInputFormat初始化失败");
        return;
    }
    
    /* open device */
    if ((ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options)) < 0) {
        av_strerror(ret,errors,1024);
        fprintf(stderr, "Failed to open audio device, [%d]%s\n", ret, errors);
        return;
    }
    /* 释放参数 */
    av_dict_free(&options);
    
    /* create file */
    /* 需要开启文件访问权限 */
    char *out_path = "/Users/saeipi/Downloads/File/audio.pcm";
    FILE *out_file = fopen(out_path, "wb+");
    if (!out_file) {
        av_log(NULL, AV_LOG_ERROR, "打开输出文件失败");
        goto KSFAULT;
    }
    
    swr_ctx = init_swr();
    if (!swr_ctx) {
        av_log(NULL, AV_LOG_ERROR, "重采样初始化失败");
        goto KSFAULT;
    }
    /* 4096/4=1024/2=512 */
    /* 创建输入缓冲区 */
    av_samples_alloc_array_and_samples(&src_data,//输出缓冲区地址
                                       &src_linesize,//缓冲区的大小
                                       2,//通道个数
                                       512,//单通道采样个数
                                       AV_SAMPLE_FMT_FLT,
                                       0);//采样格式
    
    
    /* 创建输出缓冲区 */
    av_samples_alloc_array_and_samples(&dst_data,//输出缓冲区地址
                                       &dst_linesize,//缓冲区的大小
                                       2,//通道个数
                                       512,//单通道采样个数
                                       AV_SAMPLE_FMT_S16,//采样格式
                                       0);
    
    /* read data from device */
    while ((ret = av_read_frame(fmt_ctx, &pkt)) == 0 && rec_status) {
        av_log(NULL, AV_LOG_INFO, "packet size is %d(%p)\n",pkt.size,pkt.data);
        
        /* 进行内存拷贝，按字节拷贝的 */
        memcpy((void *)src_data[0], (void *)pkt.data, pkt.size);
        
        /* 重采样 */
        swr_convert(swr_ctx,//重采样的上下文
                    dst_data,//输出结果缓冲区
                    512,//每个通道的采样数
                    (const uint8_t **)src_data,//输入缓冲区
                    512);//输入单个通道的采样数
        
        /* write file */
        /* fwrite(pkt.data, 1, pkt.size, outfile); */
        fwrite(dst_data[0], 1, dst_linesize, out_file);
        fflush(out_file);
        /* release pkt */
        av_packet_unref(&pkt);
    }
    
KSFAULT:
    if (out_file) {
        fclose(out_file);
    }
    /* 释放输入输出缓冲区 */
    if (src_data) {
        av_freep(&src_data[0]);
        av_freep(&src_data);
    }
    if (dst_data) {
        av_freep(&dst_data[0]);
        av_freep(&dst_data);
    }
    /* 释放重采样的上下文 */
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    av_log(NULL, AV_LOG_DEBUG, "finish!\n");
}

#if 0
int main(int argc, char *argv[])
{
    record_audio();
    return 0;
}
#endif
