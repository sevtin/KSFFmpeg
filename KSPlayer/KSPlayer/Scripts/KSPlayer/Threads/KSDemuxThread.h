//
//  KSDemuxThread.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
#include "KSProtocol.h"
#include "KSThread.h"
class KSDemux;
class KSVideoThread;
class KSAudioThread;

class KSDemuxThread: public KSThread {
public:
    //创建对象并打开
    virtual bool open(const char *url, KSVideoProtocol *protocol);
    //启动所有线程
    virtual void start();
    void run();
    KSDemuxThread();
    virtual ~KSDemuxThread();
    
protected:
    KSDemux *demux = NULL;
    KSVideoThread *v_thread = NULL;
    KSAudioThread *a_thread = NULL;
};
