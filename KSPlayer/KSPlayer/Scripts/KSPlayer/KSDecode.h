//
//  KSDecode.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
struct AVCodecParameters;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
#include <mutex>

class KSDecode {
    
public:
    //打开解码器,不管成功与否都释放par空间
    virtual bool open(AVCodecParameters *par);
    
    //发送到解码线程，不管成功与否都释放pkt空间（对象和媒体内容）
    virtual bool send(AVPacket *pkt);
    
    //获取解码数据，一次send可能需要多次receive，获取缓冲中的数据send NULL在receive多次
    //每次复制一份，由调用者释放 av_frame_free
    virtual AVFrame* receive();
    virtual void close();
    virtual void clear();
    
protected:
    std::mutex mux;
    AVCodecContext *avctx = NULL;
};
