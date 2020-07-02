//
//  KSVideoThread.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//
#pragma once
#include "KSMediaThread.h"

class KSVideoThread: public KSMediaThread {
public:
    //打开，不管成功与否都清理
    virtual bool open(AVCodecParameters *par,KSVideoProtocol *protocol,int width,int height);
    virtual void run();
};
