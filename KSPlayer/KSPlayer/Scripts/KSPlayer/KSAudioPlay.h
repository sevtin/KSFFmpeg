//
//  KSAudioPlay.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

//#ifndef KSAudioPlay_hpp
//#define KSAudioPlay_hpp
//
//#include <stdio.h>
//
//
//
//#endif /* KSAudioPlay_hpp */

class KSAudioPlay {
public:
    int sample_rate = 44100;
    int sample_fmt = 16;
    int channels = 2;
    
    virtual bool open();
    virtual void close();
    
    virtual bool write(const unsigned char *data, int data_size);
    
    virtual int getFree() = 0;
    
};
