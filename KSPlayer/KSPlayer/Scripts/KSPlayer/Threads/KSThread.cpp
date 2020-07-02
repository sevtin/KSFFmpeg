//
//  KSThread.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSThread.h"
#include <thread>

void KSThread::msleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

KSThread::KSThread() {
    
}

KSThread::~KSThread() {
    is_exit = true;
}
