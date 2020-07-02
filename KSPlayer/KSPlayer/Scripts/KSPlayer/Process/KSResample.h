//
//  KSResample.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
struct AVCodecParameters;
struct AVFrame;
struct SwrContext;
#include <mutex>

class KSResample {
public:
    //输出参数和输入参数一致除了采样格式，输出为S16 ,会释放par
    virtual bool open(AVCodecParameters *par, bool isClearPar = false);
    virtual void close();
    //返回重采样后大小,不管成功与否都释放indata空间
    virtual int resample(AVFrame *indata,unsigned char *data);
    
    //默认AV_SAMPLE_FMT_S16
    int out_sample_fmt = 1;
protected:
    std::mutex mux;
    SwrContext *swr_ctx = NULL;
};
