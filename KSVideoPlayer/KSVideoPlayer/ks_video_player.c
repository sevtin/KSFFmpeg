//
//  ks_video_player.c
//  KSVideoPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "ks_video_player.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_video.h"
#include "SDL2/SDL_render.h"
#include "SDL2/SDL_rect.h"

//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
//Break
#define BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit  = 0;
Uint32 thread_delay = 0;

int refresh_video(void *opaque) {
    thread_exit = 0;
    while (thread_exit == 0) {
        SDL_Event sdl_event;
        sdl_event.type = REFRESH_EVENT;
        SDL_PushEvent(&sdl_event);
        SDL_Delay(thread_delay);
    }
    thread_exit    = 0;
    SDL_Event sdl_event;
    sdl_event.type = BREAK_EVENT;
    SDL_PushEvent(&sdl_event);
    return 0;
}

int play_video(char *url) {
    /* FFmpeg */
    AVFormatContext *fmt_ctx   = NULL;
    AVCodec *codec             = NULL;
    AVCodecContext *cdc_ctx    = NULL;
    AVPacket *packet           = NULL;
    AVFrame *frame             = NULL;
    AVFrame *frame_yuv         = NULL;
    struct SwsContext *sws_ctx;
    int ret                    = 0;
    int video_stream_index     = -1;
    int got_picture            = 0;
    int frame_rate             = 0;
    uint8_t *out_buffer;
    int ctx_w                  = 640;
    int ctx_h                  = 360;
    
    /* SDL */
    SDL_Window *sdl_window     = NULL;
    SDL_Renderer *sdl_renderer = NULL;
    SDL_Texture *sdl_texture   = NULL;
    SDL_Rect sdl_rect;
    SDL_Thread *sdl_thread     = NULL;
    SDL_Event sdl_event;
    
    /* 文件*/
    if (!url) {
        return -1;
    }
    /* -------------------------------- FFmpeg Lib --------------------------------*/
    /* 1、注册组件 */
    av_register_all();
    /* 2、打开输入 */
    ret = avformat_open_input(&fmt_ctx,
                              url,
                              NULL,
                              NULL);
    if (ret != 0) {
        return -1;
    }
    /* 3、获取流媒体信息 */
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        return -1;
    }
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        /* 4、获取视频流index */
        if (fmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        return -1;
    }

    /* 5、获取视频解码上下文 */
    AVStream *stream = fmt_ctx->streams[video_stream_index];
    frame_rate       = stream->avg_frame_rate.num/stream->avg_frame_rate.den;
    thread_delay     = 1000/frame_rate;
    cdc_ctx          = stream->codec;

    /* 6、获取视频解码器 */
    codec = avcodec_find_decoder(cdc_ctx->codec_id);
    if (!codec) {
        return -1;
    }
    /* 7、打开解码器 */
    ret = avcodec_open2(cdc_ctx, codec, NULL);
    if (ret != 0) {
        return -1;
    }
    
    /* 8、初始化数据包 */
    packet = av_malloc(sizeof(AVPacket));
    /* 9、初始化帧数据 */
    frame = av_frame_alloc();
    frame_yuv = av_frame_alloc();
    
    /* 9、初始化缓冲区 */
    out_buffer = av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P,
                                              cdc_ctx->width,
                                              cdc_ctx->height));
    /* 10、填充缓冲区 */
    avpicture_fill((AVPicture *)frame_yuv, out_buffer,
                   AV_PIX_FMT_YUV420P,
                   cdc_ctx->width,
                   cdc_ctx->height);
    
    ctx_w = cdc_ctx->width;
    ctx_h = cdc_ctx->height;
    /* 11、视频像素数据格式转换上下文 */
    /**
     * Allocate and return an SwsContext. You need it to perform
     * scaling/conversion operations using sws_scale().
     *
     * @param srcW the width of the source image
     * @param srcH the height of the source image
     * @param srcFormat the source image format
     * @param dstW the width of the destination image
     * @param dstH the height of the destination image
     * @param dstFormat the destination image format
     * @param flags specify which algorithm and options to use for rescaling
     * @param param extra parameters to tune the used scaler
     *              For SWS_BICUBIC param[0] and [1] tune the shape of the basis
     *              function, param[0] tunes f(1) and param[1] f´(1)
     *              For SWS_GAUSS param[0] tunes the exponent and thus cutoff
     *              frequency
     *              For SWS_LANCZOS param[0] tunes the width of the window function
     * @return a pointer to an allocated context, or NULL in case of error
     * @note this function is to be removed after a saner alternative is
     *       written
     */
    sws_ctx = sws_getContext(ctx_w,
                             ctx_h,
                             cdc_ctx->pix_fmt,
                             ctx_w,
                             ctx_h,
                             AV_PIX_FMT_YUV420P,
                             SWS_BICUBIC,
                             NULL,
                             NULL,
                             NULL);
    
    /* -------------------------------- SDL2 Lib --------------------------------*/
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return - 1;
    }
    
    sdl_window = SDL_CreateWindow("KSPlayer",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  ctx_w,
                                  ctx_h,
                                  SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    
    if (!sdl_window) {
        printf("SDL: could not create window - exiting:%s\n",SDL_GetError());
        return -1;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);

    sdl_texture  = SDL_CreateTexture(sdl_renderer,
                                    SDL_PIXELFORMAT_IYUV,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    ctx_w,
                                    ctx_h);

    sdl_rect.x   = 0;
    sdl_rect.y   = 0;
    sdl_rect.w   = ctx_w;
    sdl_rect.h   = ctx_w;

    sdl_thread   = SDL_CreateThread(refresh_video, NULL, NULL);

    while (1) {
        SDL_WaitEvent(&sdl_event);
        if (sdl_event.type == REFRESH_EVENT) {
            if (av_read_frame(fmt_ctx, packet) >= 0) {
                if (packet->stream_index == video_stream_index) {
                    /* 12、解码视频  */
                    if (avcodec_decode_video2(cdc_ctx, frame, &got_picture, packet) < 0) {
                        return -1;
                    }
                    if (got_picture) {
                        
                        /**
                        * Scale the image slice in srcSlice and put the resulting scaled
                        * slice in the image in dst. A slice is a sequence of consecutive
                        * rows in an image.
                        *
                        * Slices have to be provided in sequential order, either in
                        * top-bottom or bottom-top order. If slices are provided in
                        * non-sequential order the behavior of the function is undefined.
                        *
                        * @param c         the scaling context previously created with
                        *                  sws_getContext()
                        * @param srcSlice  the array containing the pointers to the planes of
                        *                  the source slice
                        * @param srcStride the array containing the strides for each plane of
                        *                  the source image
                        * @param srcSliceY the position in the source image of the slice to
                        *                  process, that is the number (counted starting from
                        *                  zero) in the image of the first row of the slice
                        * @param srcSliceH the height of the source slice, that is the number
                        *                  of rows in the slice
                        * @param dst       the array containing the pointers to the planes of
                        *                  the destination image
                        * @param dstStride the array containing the strides for each plane of
                        *                  the destination image
                        * @return          the height of the output slice
                        */
                        sws_scale(sws_ctx,
                                  (const uint8_t* const*)frame->data,
                                  frame->linesize,
                                  0,
                                  cdc_ctx->height,
                                  frame_yuv->data,
                                  frame_yuv->linesize);
                        
                        SDL_UpdateTexture(sdl_texture,
                                          NULL,
                                          frame_yuv->data[0],
                                          frame_yuv->linesize[0]);
                        
                        SDL_RenderClear(sdl_renderer);
                        SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
                        SDL_RenderPresent(sdl_renderer);
                    }
                }
            }
            av_free_packet(packet);
        }
        else if(sdl_event.type==SDL_WINDOWEVENT){
            //If Resize
            SDL_GetWindowSize(sdl_window,&ctx_w,&ctx_h);
        }else if(sdl_event.type==SDL_QUIT){
            thread_exit=1;
        }else if(sdl_event.type==BREAK_EVENT){
            break;
        }
    }
    
    sws_freeContext(sws_ctx);
    
    SDL_Quit();
    
    av_frame_free(&frame_yuv);
    av_frame_free(&frame);
    avcodec_close(cdc_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}

