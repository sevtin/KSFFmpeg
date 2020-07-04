//
//  KSMediaThread.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
struct AVPacket;
struct AVCodecParameters;
class KSDecode;

#include <list>
#include <mutex>
#include "KSProtocol.h"
#include "KSThread.h"

class KSMediaThread: public KSThread {
public:
    //最大队列
    int max_list = 100;

    void push(AVPacket *pkt);
    virtual void run() = 0;

protected:
    std::list <AVPacket *> packs;
    KSDecode *decode = NULL;
    KSVideoProtocol *protocol = NULL;
};
