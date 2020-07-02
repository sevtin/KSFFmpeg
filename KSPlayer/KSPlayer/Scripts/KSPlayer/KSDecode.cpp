//
//  KSDecode.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSDecode.hpp"
#include <thread>
#include <iostream>
using namespace std;

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}

int player(const char *url)
{
    cout << "Test Demux FFmpeg.club" << endl;
    const char *path = url;
    //初始化封装库
    av_register_all();
    
    //初始化网络库 （可以打开rtsp rtmp http 协议的流媒体视频）
    avformat_network_init();
    
    //注册解码器
    avcodec_register_all();
    return 0;
}
