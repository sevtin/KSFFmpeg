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
    //媒体总时长（毫秒）
    int total_ms = 0;
    int width = 0;
    int height = 0;
    int sample_rate = 0;
    int channels = 0;
    
    //打开媒体文件，或者流媒体 rtmp http rstp
    virtual bool open(const char *url);
    //清空读取缓存
    virtual void clear();
    virtual void close();
    //空间需要调用者释放 ，释放AVPacket对象空间，和数据空间 av_packet_free
    virtual AVPacket* read();
    virtual KSMediaType mediaType(AVPacket *pkt);
    //获取视频参数  返回的空间需要清理  avcodec_parameters_free
    virtual AVCodecParameters* copyVideoPar();
    //获取音频参数  返回的空间需要清理 avcodec_parameters_free
    virtual AVCodecParameters* copyAudioPar();
    //seek 位置 pos 0.0 ~1.0
    virtual bool seek(double pos);
    
    KSDemux();
    virtual ~KSDemux();
    
protected:
    std::mutex mux;
    //解封装上下文
    AVFormatContext *fmt_ctx = NULL;
    //音视频索引，读取时区分音视频
    int video_index = -1;
    int audio_index = -1;
    
    AVCodecParameters* copyStreamPar(int index);
};
