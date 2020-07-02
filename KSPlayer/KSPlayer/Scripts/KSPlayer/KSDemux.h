//
//  KSDemux.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
#include <mutex>
struct AVFormatContext;
struct AVPacket;
struct AVCodecParameters;

enum KSMediaType {
    KSMEDIA_TYPE_UNKNOWN = -1,
    KSMEDIA_TYPE_VIDEO,
    KSMEDIA_TYPE_AUDIO
};

class KSDemux {
public:
    
    int total_ms = 0;
    int width = 0;
    int height = 0;
    int sample_rate = 0;
    int channels = 0;
    
    virtual bool open(const char *url);
    virtual void clear();
    virtual void close();
    virtual AVPacket* read();
    virtual KSMediaType mediaType(AVPacket *pkt);
    virtual AVCodecParameters* copyVideoPar();
    virtual AVCodecParameters* copyAudioPar();
    virtual bool seek(double pos);
    
    KSDemux();
    virtual ~KSDemux();
    
protected:
    std::mutex mux;
    AVFormatContext *fmt_ctx = NULL;
    int video_index = -1;
    int audio_index = -1;
    
    AVCodecParameters* copyStreamPar(int index);
};
