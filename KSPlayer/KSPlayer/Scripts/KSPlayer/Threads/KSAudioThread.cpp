//
//  KSAudioThread.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSAudioThread.h"
#include <iostream>
#include "KSDecode.h"
#include "KSAudioPlay.h"
#include "KSResample.h"
using namespace std;

bool KSAudioThread::open(AVCodecParameters *par,int sample_rate,int channels) {
    if (!par) {
        return false;
    }
    mux.lock();
    if (!decode) {
        decode = new KSDecode();
    }
    if (!resample) {
        resample = new KSResample();
    }
    
    audio_play->sample_rate = sample_rate;
    audio_play->channels = channels;
    
    if (!audio_play) {
        audio_play = KSAudioPlay::shared();
    }
    
    bool ret = true;
    if (!resample->open(par, false)) {
        cout << "KSResample open failed!" << endl;
        ret = false;
    }
    if (!decode->open(par)) {
        cout << "audio KSDecode open failed!" << endl;
        ret = false;
    }
    mux.unlock();
    return true;
}

void KSAudioThread::push(AVPacket *pkt) {
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

void KSAudioThread::run() {
    unsigned char *pcm = new unsigned char[1024 * 1024 * 10];
    while (!is_exit) {
        mux.lock();
        //没有数据
        if (packs.empty() || !decode || !resample || !audio_play) {
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
        //一次send 多次receive
        while (!is_exit) {
            AVFrame *frame = decode->receive();
            if (!frame) {
                break;
            }
            //重采样
            int size = resample->resample(frame, pcm);
            while (!is_exit) {
                if (size <= 0) {
                    break;
                }
                //缓冲未播完，空间不够
                if (audio_play->getFree() < size) {
                    msleep(2);
                    continue;
                }
                audio_play->write(pcm, size);
                break;
            }
        }
        mux.unlock();
    }
    delete[] pcm;
}

