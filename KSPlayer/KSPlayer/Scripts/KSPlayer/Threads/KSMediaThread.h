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

class KSMediaThread {
public:
    //最大队列
    int max_list = 100;
    bool is_exit = false;
    
    void push(AVPacket *pkt);
    void msleep(int ms);
    virtual void run() = 0;
    
    KSMediaThread();
    virtual ~KSMediaThread();
protected:
    std::list <AVPacket *> packs;
    std::mutex mux;
    KSDecode *decode = NULL;
    KSVideoProtocol *protocol = NULL;
};
