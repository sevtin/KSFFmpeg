//
//  KSDemuxThread.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSDemuxThread.h"
#include <iostream>
#include "KSDemux.h"
#include "KSVideoThread.h"
#include "KSAudioThread.h"
using namespace std;

void KSDemuxThread::run() {
    while (!is_exit) {
        mux.lock();
        if (!demux) {
            mux.unlock();
            msleep(10);
            continue;
        }
        mux.unlock();
    }
}
