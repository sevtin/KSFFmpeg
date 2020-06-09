//
//  hw_decode.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/9.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "hw_decode.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE out
