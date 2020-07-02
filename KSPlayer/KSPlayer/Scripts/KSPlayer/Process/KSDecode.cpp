//
//  KSDecode.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSDecode.h"
#include <iostream>
using namespace std;

extern "C" {
    #include "libavcodec/avcodec.h"
}

void KSDecode::close() {
    mux.lock();
    if (avctx) {
        avcodec_close(avctx);
        avcodec_free_context(&avctx);
    }
    mux.unlock();
}

void KSDecode::clear() {
    mux.lock();
    if (avctx) {
        //清理解码缓冲
        avcodec_flush_buffers(avctx);
    }
    mux.unlock();
}

bool KSDecode::open(AVCodecParameters *par) {
    if (!par) {
        return false;
    }
    close();
    
    //解码器打开
    //找到解码器
    AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        cout << "can't find the codec id " << par->codec_id << endl;
        return false;
    }
    
    mux.lock();
    avctx = avcodec_alloc_context3(codec);
    //配置解码器上下文参数
    avcodec_parameters_to_context(avctx, par);
    avcodec_parameters_free(&par);
    //8线程解码
    avctx->thread_count = 8;
    
    //打开解码器上下文
    int ret = avcodec_open2(avctx, NULL, NULL);
    if (ret != 0) {
        avcodec_free_context(&avctx);
        mux.unlock();
        cout << "avcodec_open2 failed! :" << av_err2str(ret) << endl;
        return false;
    }
    mux.unlock();
    return true;
}

//发送到解码线程，不管成功与否都释放pkt空间（对象和媒体内容）
bool KSDecode::send(AVPacket *pkt) {
    if (!pkt || pkt->size <= 0 || !pkt->data) {
        return false;
    }
    
    mux.lock();
    if (!avctx) {
        mux.unlock();
        return false;
    }
    int ret = avcodec_send_packet(avctx, pkt);
    mux.unlock();
    av_packet_free(&pkt);
    if (ret != 0) {
        return false;
    }
    return true;
}

//获取解码数据，一次send可能需要多次receive，获取缓冲中的数据send NULL在receive多次
//每次复制一份，由调用者释放 av_frame_free
AVFrame* KSDecode::receive() {
    mux.lock();
    if (!avctx) {
        mux.unlock();
        return NULL;
    }
    
    AVFrame *frame = av_frame_alloc();
    int ret = avcodec_receive_frame(avctx, frame);
    mux.unlock();
    if (ret != 0) {
        av_frame_free(&frame);
        return NULL;
    }
    return frame;
}
