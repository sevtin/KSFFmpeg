//
//  KSAudioResample.h
//  KSAudioResample
//
//  Created by saeipi on 2020/5/18.
//  Copyright © 2020 saeipi. All rights reserved.
//

#ifndef KSAudioResample_h
#define KSAudioResample_h

#include <stdio.h>
#include "libavutil/avutil.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"

/* 更新状态*/
void update_status(int status);
/* 音频采样 */
void record_audio(void);

#endif /* KSAudioResample_h */
