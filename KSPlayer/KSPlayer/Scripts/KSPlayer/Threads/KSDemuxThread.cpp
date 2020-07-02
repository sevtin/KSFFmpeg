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
        AVPacket *pkt = demux->read();
        if (!pkt) {
            mux.unlock();
            msleep(5);
            continue;
        }
        KSMediaType type = demux->mediaType(pkt);
        //判断数据是视频
        if (type == KSMEDIA_TYPE_VIDEO) {
            v_thread->push(pkt);
        }
        else if (type == KSMEDIA_TYPE_AUDIO) {
            a_thread->push(pkt);
        }
        mux.unlock();
    }
}

bool KSDemuxThread::open(const char *url, KSVideoProtocol *protocol) {
    if (!url) {
        return false;
    }
    
    mux.lock();
    if (!demux) {
        demux = new KSDemux();
    }
    if (!v_thread) {
        v_thread = new KSVideoThread();
    }
    if (!a_thread) {
        a_thread = new KSAudioThread();
    }
    
    //打开解封装
    bool ret = demux->open(url);
    if (!ret) {
        mux.unlock();
        cout << "demux->Open(url) failed!" << endl;
        return false;
    }
    //打开视频解码器和处理线程
    if (!v_thread->open(demux->copyVideoPar(), protocol, demux->width, demux->height)) {
        mux.unlock();
        cout << "vt->Open failed!" << endl;
        return false;
    }
    
    //打开音频解码器和处理线程
    if (!a_thread->open(demux->copyAudioPar(), demux->sample_rate, demux->channels)) {
        mux.unlock();
        cout << "at->Open failed!" << endl;
        return false;
    }
    mux.unlock();
    return true;
}

//启动所有线程
void KSDemuxThread::start() {
    mux.lock();
    if (v_thread) {
        
    }
    if (a_thread) {
        
    }
    mux.unlock();
}

KSDemuxThread::KSDemuxThread(){
    
}

KSDemuxThread::~KSDemuxThread() {
    
}
