//
//  KSVideoThread.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSVideoThread.h"
#include <iostream>
#include "KSDecode.h"
using namespace std;

//打开，不管成功与否都清理
bool KSVideoThread::open(AVCodecParameters *par,KSVideoProtocol *protocol,int width,int height) {
    if (!par) {
        return false;
    }
    mux.lock();
    this->protocol = protocol;
    if (protocol) {
        protocol->initSize(width, height);
    }
    if (!decode) {
        decode = new KSDecode();
    }
    bool ret = true;
    if (!decode->open(par)) {
        cout << "audio KSDecode open failed!" << endl;
        ret = false;
    }
    mux.unlock();
    return true;
}

void KSVideoThread::run() {
    while (!is_exit) {
        mux.lock();
        if (packs.empty() || !decode) {
            mux.unlock();
            msleep(2);
            continue;
        }
        
        AVPacket *pkt = packs.front();
        packs.pop_front();
        
        bool ret = decode->send(pkt);
        if (!ret) {
            mux.unlock();
            msleep(2);
            continue;
        }
        while (!is_exit) {
            AVFrame *frame = decode->receive();
            if (!frame) {
                break;
            }
            if (protocol) {
                protocol->repaint(frame);
            }
        }
        mux.unlock();
    }
}
