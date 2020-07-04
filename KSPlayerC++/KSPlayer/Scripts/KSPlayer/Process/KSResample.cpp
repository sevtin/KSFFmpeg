//
//  KSResample.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSResample.h"
#include <iostream>
using namespace std;

extern "C" {
    #include "libswresample/swresample.h"
    #include "libavcodec/avcodec.h"
}

//输出参数和输入参数一致除了采样格式，输出为S16
bool KSResample::open(AVCodecParameters *par, bool isClearPar) {
    if (!par) {
        return false;
    }
    mux.lock();
    
    //如果actx为NULL会分配空间
    swr_ctx = swr_alloc_set_opts(swr_ctx,
                                 av_get_default_channel_layout(2),//输出格式
                                 (AVSampleFormat)out_sample_fmt,//输出样本格式 1 AV_SAMPLE_FMT_S16
                                 par->sample_rate,//输出采样率
                                 av_get_default_channel_layout(par->channels),//输入格式
                                 (AVSampleFormat)par->format,
                                 par->sample_rate,
                                 0, NULL);
    if (isClearPar) {
        avcodec_parameters_free(&par);
    }
    int ret = swr_init(swr_ctx);
    mux.unlock();
    if (ret != 0) {
        cout << "swr_init  failed! :" << av_err2str(ret) << endl;
        return false;
    }
    return true;
}

void KSResample::close() {
    mux.lock();
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    mux.unlock();
}

//返回重采样后大小,不管成功与否都释放indata空间
int KSResample::resample(AVFrame *indata,unsigned char *data) {
    if (!indata) {
        return 0;
    }
    if (!data) {
        av_frame_free(&indata);
        return 0;
    }
    uint8_t *datas[2] = {0};
    datas[0] = data;
    int ret = swr_convert(swr_ctx,
                          datas,
                          indata->nb_samples,//输出
                          (const uint8_t**)indata->data,
                          indata->nb_samples);//输入
    if (ret <= 0) {
        return ret;
    }
    int outsize = ret * indata->channels * av_get_bytes_per_sample((AVSampleFormat)out_sample_fmt);
    return outsize;
}
