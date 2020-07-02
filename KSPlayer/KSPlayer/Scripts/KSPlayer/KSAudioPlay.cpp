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
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t   data_size;
    
    virtual int getFree() {
        mux.lock();
        mux.unlock();
        return 0;
    }
};
