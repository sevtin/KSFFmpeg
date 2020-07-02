//
//  KSProtocol.h
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
struct AVFrame;

class KSVideoProtocol {
public:
    virtual void initSize(int width, int height) = 0;
    virtual void repaint(AVFrame *frame) = 0;
};
