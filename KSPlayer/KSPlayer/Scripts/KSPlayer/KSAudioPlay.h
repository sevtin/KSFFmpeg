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
    
    //带有纯虚函数的类，只能被继承，不能够被实例化
    //打开音频播放
    virtual bool open() = 0;
    virtual void close() = 0;
    //播放音频
    virtual bool write(const unsigned char *data, int data_size) = 0;
    virtual int getFree() = 0;
    
    static KSAudioPlay* shared();
    KSAudioPlay();
    virtual ~KSAudioPlay();
};
