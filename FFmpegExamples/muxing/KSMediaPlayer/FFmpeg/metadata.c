//
//  metadata.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/10.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "metadata.h"

#include "libavformat/avformat.h"
#include "libavutil/dict.h"

int metadata_port(char *src_url)
{
    AVFormatContext *fmt_ctx = NULL;
    AVDictionaryEntry *tag = NULL;
    int ret;

    if (!src_url) {
        return 1;
    }

    if ((ret = avformat_open_input(&fmt_ctx, src_url, NULL, NULL)))
        return ret;

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        printf("%s=%s\n", tag->key, tag->value);
    }
    
    avformat_close_input(&fmt_ctx);
    return 0;
}
