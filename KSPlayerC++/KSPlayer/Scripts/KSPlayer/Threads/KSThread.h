//
//  KSThread.hpp
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#pragma once
#include <mutex>

class KSThread {
public:
    bool is_exit = false;
    virtual void msleep(int ms);
    KSThread();
    virtual ~KSThread();
    
protected:
    std::mutex mux;
};
