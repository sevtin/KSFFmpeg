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
    int sampleRate = 44100;
    int sampleSize = 16;
    int channels = 2;
    
    virtual bool open() = 0;
    virtual void close() = 0;
    
    virtual bool write(const unsigned char *data, int datasize) = 0;
    
    virtual int getFree() = 0;
    
};
