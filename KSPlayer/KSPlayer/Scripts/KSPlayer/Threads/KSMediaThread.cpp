//
//  KSMediaThread.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSMediaThread.h"
#include <thread>

void KSMediaThread::msleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void KSMediaThread::push(AVPacket *pkt) {
    if (!pkt) {
        return;
    }
    //阻塞
    while (!is_exit) {
        mux.lock();
        if (packs.size() < max_list) {
            packs.push_back(pkt);
            mux.unlock();
            break;
        }
        mux.unlock();
        msleep(2);
    }
}

KSMediaThread::KSMediaThread() {
    
}

KSMediaThread::~KSMediaThread() {
    is_exit = true;
}
