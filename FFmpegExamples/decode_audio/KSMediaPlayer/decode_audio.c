//
//  decode_audio.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/6.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "decode_audio.h"
#include <stdlib.h>
#include <string.h>

#include "libavutil/frame.h"
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

static int decode_audio_execute(AVCodecContext *dec_ctx,AVPacket *pkt,AVFrame *frame,FILE *outfile) {
    int i,ch;
    int ret,data_size;
    
    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(dec_ctx, pkt);
    
    if (ret < 0) {
        fprintf(stderr, "Error submitting the packet to the decoder\n");
        return ret;
    }
    
    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return -1;
        }
        else if (ret < 0){
            fprintf(stderr, "Error during decoding\n");
            return ret;
        }
        data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
        if (data_size < 0) {
            /* This should not occur, checking just for paranoia */
            fprintf(stderr, "Failed to calculate data size\n");
            return -1;
        }
        for (i = 0; i < frame->nb_samples; i++) {
            for (ch = 0; ch < dec_ctx->channels; ch++) {
                fwrite(frame->data[ch] + data_size*i, 1, data_size, outfile);
            }
        }
    }
    
    return 0;
}

int decode_audio_port(char *inurl,char *outurl) {
    const char *outfilename,*filename;
    const AVCodec *codec;
    AVCodecContext *dec_ctx = NULL;
    AVCodecParserContext *parser = NULL;
    int len,ret;
    FILE *infile,*outfile;
    uint8_t inbuf[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t data_size;
    AVPacket *pkt;
    AVFrame *decoded_frame = NULL;
    
    if (!inurl || !outurl) {
        return -1;
    }
    
    filename = inurl;
    outfilename = outurl;
    
    pkt = av_packet_alloc();
    
    /* find the MPEG audio decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto ksfault;
    }
    
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
        goto ksfault;
    }
    
    dec_ctx = avcodec_alloc_context3(codec);
    if (!dec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        goto ksfault;
    }
    
    /* open it */
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto ksfault;
    }
    infile = fopen(filename, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", filename);
        goto ksfault;
    }
    outfile = fopen(outfilename, "wb");
    if (!outfile) {
        av_free(dec_ctx);
        goto ksfault;
    }
    
    /* decode until eof */
    data = inbuf;
    data_size = fread(inbuf, 1, AUDIO_INBUF_SIZE, infile);
    while (data_size > 0) {
        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
                fprintf(stderr, "Could not allocate audio frame\n");
                goto ksfault;
            }
        }
        
        ret = av_parser_parse2(parser,
                               dec_ctx,
                               &pkt->data,
                               &pkt->size,
                               data,
                               data_size,
                               AV_NOPTS_VALUE,
                               AV_NOPTS_VALUE,
                               0);
        if (ret < 0) {
            fprintf(stderr, "Error while parsing\n");
            goto ksfault;
        }
        
        data += ret;
        data_size -= ret;
        
        if (pkt->size) {
            decode_audio_execute(dec_ctx, pkt, decoded_frame, outfile);
        }
        if (data_size < AUDIO_REFILL_THRESH) {
            memmove(inbuf, data, data_size);
            data = inbuf;
            len = fread(data + data_size, 1, AUDIO_INBUF_SIZE - data_size, infile);
            
            if (len > 0) {
                data_size += len;
            }
        }
    }
    
ksfault:
    /* flush the decoder */
    pkt->data = NULL;
    pkt->size = 0;
    if (!dec_ctx && !pkt && !decoded_frame && !outfile) {
        decode_audio_execute(dec_ctx, pkt, decoded_frame, outfile);
    }
    
    fclose(infile);
    fclose(outfile);
    avcodec_free_context(&dec_ctx);
    av_parser_close(parser);
    
    av_frame_free(&decoded_frame);
    av_packet_free(&pkt);
    
    return ret;
}
