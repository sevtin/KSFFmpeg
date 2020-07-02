//
//  KSAudioPlay.cpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "KSAudioPlay.h"
#include <mutex>
#include "libavcodec/avcodec.h"

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

class KSSuperAudioPlay:public KSAudioPlay {
public:
    std::mutex mux;
    uint8_t audio_buf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    size_t   buf_size;
    
    virtual bool open() {
        close();
        
        return false;
    }
    
    virtual void close() {
        mux.lock();
        if (buf_size > 0) {
            free(audio_buf);
            buf_size = 0;
        }
        mux.unlock();
    }
    
    virtual bool write(const unsigned char *data, int data_size) {
        if (!data || data_size <= 0) {
            return false;
        }
        mux.lock();
        memcpy(audio_buf, data, data_size);
        buf_size += data_size;
        mux.unlock();
        return true;
    }
    
    
    virtual int getFree() {
        mux.lock();
        if (buf_size <= 0) {
            mux.lock();
        }
        mux.unlock();
        return 0;
    }
};
