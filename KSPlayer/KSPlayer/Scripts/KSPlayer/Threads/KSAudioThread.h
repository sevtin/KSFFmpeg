//
//  KSAudioThread.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//
#pragma once
#include <pthread.h>
#include <mutex>
#include <list>
struct AVCodecParameters;
class KSDecode;
class KSAudioPlay;
class KSResample;
class AVPacket;

class KSAudioThread {
public:
    //最大队列
    int max_list = 100;
    bool is_exit = false;
    
    //打开，不管成功与否都清理
    virtual bool open(AVCodecParameters *par,int sample_rate,int channels);
    virtual void push(AVPacket *pkt);
    void run();
    
    KSAudioThread();
    virtual ~KSAudioThread();
    
protected:
    std::list <AVPacket *> packs;
    std::mutex mux;
    KSDecode *decode = NULL;
    KSAudioPlay *audio_play = NULL;
    KSResample *resample = NULL;
};
