//
//  KSMediaThread.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSMediaThread.h"

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

