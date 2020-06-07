//
//  encode_video.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/7.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "encode_video.h"
#include <stdlib.h>
#include <string.h>

#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/imgutils.h"

/* check that a given sample format is supported by the encoder */

static void encode(AVCodecContext *enc_ctx,AVFrame *frame,AVPacket *pkt,FILE *outfile) {
    int ret;
}
