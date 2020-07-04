//
//  KSDemux.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSDemux.h"
#include <iostream>
using namespace std;
extern "C" {
    #include "libavformat/avformat.h"
}

static double r2d(AVRational r) {
    return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

//打开媒体文件，或者流媒体 rtmp http rstp
bool KSDemux::open(const char *url) {
    if (!url) {
        return false;
    }
    close();
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "max_delay", "500", 0);
    
    mux.lock();
    int ret = avformat_open_input(&fmt_ctx, url, NULL, &opts);
    av_dict_free(&opts);
    if (ret != 0) {
        cout << "open " << url << " failed! :" << av_err2str(ret) << endl;
        return false;
    }
    
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        return false;
    }
    //int total_ms = fmt_ctx->duration / (AV_TIME_BASE / 1000);
    av_dump_format(fmt_ctx, 0, url, 0);
    
    video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index == -1) {
        avformat_close_input(&fmt_ctx);
        return false;
    }
    audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index == -1) {
        avformat_close_input(&fmt_ctx);
        return false;
    }
    //视屏
    AVStream *video_stream = fmt_ctx->streams[video_index];
    width = video_stream->codecpar->width;
    height = video_stream->codecpar->height;
    
    //音频
    AVStream *audio_stream = fmt_ctx->streams[audio_index];
    sample_rate = audio_stream->codecpar->sample_rate;
    channels = audio_stream->codecpar->channels;
    
    mux.unlock();
    return true;
}

//清空读取缓存
void KSDemux::clear() {
    mux.lock();
    if (!fmt_ctx) {
        mux.unlock();
        return;
    }
    avformat_flush(fmt_ctx);
    mux.unlock();
}

void KSDemux::close() {
    mux.lock();
    if (!fmt_ctx) {
        mux.unlock();
        return;
    }
    avformat_close_input(&fmt_ctx);
    total_ms = 0;
    mux.unlock();
}

//空间需要调用者释放 ，释放AVPacket对象空间，和数据空间 av_packet_free
AVPacket* KSDemux::read() {
    mux.lock();
    
    if (!fmt_ctx) {
        mux.unlock();
        return NULL;
    }
    AVPacket *pkt = av_packet_alloc();
    int ret = av_read_frame(fmt_ctx, pkt);
    if (ret != 0) {
        mux.unlock();
        av_packet_free(&pkt);
        return NULL;
    }
    pkt->pts = pkt->pts*(1000 * (r2d(fmt_ctx->streams[pkt->stream_index]->time_base)));
    pkt->dts = pkt->dts*(1000 * (r2d(fmt_ctx->streams[pkt->stream_index]->time_base)));
    
    mux.unlock();
    return pkt;
}

KSMediaType KSDemux::mediaType(AVPacket *pkt) {
    if (!pkt) {
        return KSMEDIA_TYPE_UNKNOWN;
    }
    if (pkt->stream_index == video_index) {
        return KSMEDIA_TYPE_VIDEO;
    }
    return KSMEDIA_TYPE_AUDIO;
}

AVCodecParameters* KSDemux::copyStreamPar(int index) {
    mux.lock();
    if (!fmt_ctx) {
        mux.unlock();
        return NULL;
    }
    AVCodecParameters *par = avcodec_parameters_alloc();
    avcodec_parameters_copy(par, fmt_ctx->streams[index]->codecpar);
    mux.unlock();
    return par;
}

//获取视频参数  返回的空间需要清理  avcodec_parameters_free
AVCodecParameters* KSDemux::copyVideoPar() {
    return copyStreamPar(video_index);
}

//获取音频参数  返回的空间需要清理 avcodec_parameters_free
AVCodecParameters* KSDemux::copyAudioPar() {
    return copyStreamPar(audio_index);
}

//seek 位置 pos 0.0 ~1.0
bool KSDemux::seek(double pos){
    mux.lock();
    if (!fmt_ctx) {
        mux.unlock();
        return false;
    }
    avformat_flush(fmt_ctx);
    long long seek_pos = 0;
    seek_pos = fmt_ctx->streams[video_index]->duration * pos;
    int ret = av_seek_frame(fmt_ctx, video_index, seek_pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
    mux.unlock();
    if (ret < 0) {
        return false;
    }
    return true;
}

KSDemux::KSDemux() {
    static bool is_first = true;
    static std::mutex dmux;
    dmux.lock();
    if (is_first) {
        av_register_all();
        avformat_network_init();
        is_first = false;
    }
    dmux.unlock();
}

KSDemux::~KSDemux() {
    
}
