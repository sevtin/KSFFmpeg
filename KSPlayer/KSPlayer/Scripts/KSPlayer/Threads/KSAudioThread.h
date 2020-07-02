//
//  KSAudioThread.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//
#pragma once
#include "KSMediaThread.h"
class KSAudioPlay;
class KSResample;

class KSAudioThread: public KSMediaThread {
public:
    //打开，不管成功与否都清理
    virtual bool open(AVCodecParameters *par,int sample_rate,int channels);
    virtual void push(AVPacket *pkt);
    virtual void run();
protected:
    KSAudioPlay *audio_play = NULL;
    KSResample *resample = NULL;
};
