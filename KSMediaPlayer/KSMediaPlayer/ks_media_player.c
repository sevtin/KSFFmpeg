//
//  ks_media_player.c
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//

#include "ks_media_player.h"
#include <assert.h>
#include <math.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000 //channels(2) * data_size(2) * sample_rate(48000)

#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0

#define SAMPLE_CORRECTION_PERCENT_MAX 10
#define AUDIO_DIFF_AVG_NB 20

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT (SDL_USEREVENT + 1)

#define VIDEO_PICTURE_QUEUE_SIZE 1

#define DEFAULT_AV_SYNC_TYPE AV_SYNC_AUDIO_MASTER //AV_SYNC_VIDEO_MASTER

typedef struct PacketQueue {
    //队列头，队列尾
    AVPacketList *first_pkt, *last_pkt;
    //队列中有多少个包
    int nb_packets;
    //对了存储空间大小
    int size;
    //互斥量：加锁，解锁用
    SDL_mutex *mutex;
    //条件变量：同步用
    SDL_cond *cond;
} PacketQueue;


typedef struct VideoPicture {
    AVPicture *bmp;//YUV数据
    int width, height; /* source height & width */
    int allocated;
    double pts;
} VideoPicture;

typedef struct VideoState {
    
    //multi-media file
    char            filename[1024];
    //多媒体文件格式上下文
    AVFormatContext *pFormatCtx;
    int             videoStream, audioStream;
    
    //sync
    int             av_sync_type;
    double          external_clock; /* external clock base */
    int64_t         external_clock_time;
    
    double          audio_diff_cum; /* used for AV difference average computation */
    double          audio_diff_avg_coef;
    double          audio_diff_threshold;
    int             audio_diff_avg_count;
    
    //音频正在播放的时间
    double          audio_clock;
    //下次要回调的timer是多少
    double          frame_timer;
    //上一帧播放视频帧的pts
    double          frame_last_pts;
    //上一帧播放视频帧增加的delay
    double          frame_last_delay;
    //下一帧，将要播放视频帧的pts时间
    double          video_clock; ///<pts of last decoded frame / predicted pts of next decoded frame
    double          video_current_pts; ///<current displayed pts (different from video_clock if frame fifos are used)
    int64_t         video_current_pts_time;  ///<time (av_gettime) at which we updated video_current_pts - used to have running video pts
    
    //audio
    //音频流
    AVStream        *audio_st;
    //音频解码上下文
    AVCodecContext  *audio_ctx;
    //音频队列
    PacketQueue     audioq;
    //解码后的音频缓冲区
    uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    //缓冲区大小
    unsigned int    audio_buf_size;
    //现在已经使用了多少字节
    unsigned int    audio_buf_index;
    //解码后的音频帧
    AVFrame         audio_frame;
    //解码之前的音频包
    AVPacket        audio_pkt;
    //解码前音频包数据的指针
    uint8_t         *audio_pkt_data;
    //解码后音频数据大小
    int             audio_pkt_size;
    
    int             audio_hw_buf_size;
    
    //video
    AVStream        *video_st;
    AVCodecContext  *video_ctx;
    PacketQueue     videoq;
    //视频裁剪上下文
    struct SwsContext *video_sws_ctx;
    //音频重采样上下文，因为音频设备的参数是固定的，所以需要重采样
    struct SwrContext *audio_swr_ctx;
    //解码后的视频帧队列
    VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
    //pictq_size：解码后的队列大小，pictq_rindex：取的位置，pictq_windex：存的位置
    int             pictq_size, pictq_rindex, pictq_windex;
    //视频帧队列锁
    SDL_mutex       *pictq_mutex;
    //视频帧队列信号量
    SDL_cond        *pictq_cond;
    //解复用线程
    SDL_Thread      *parse_tid;
    //解码线程
    SDL_Thread      *video_tid;
    //退出：1退出
    int             quit;
} VideoState;

SDL_mutex    *text_mutex;
SDL_Window   *win = NULL;
SDL_Renderer *renderer;
SDL_Texture  *texture;

//Old 属性 Start
VideoState *global_video_state;
//Old 属性 End

//同步方案
enum {
    AV_SYNC_AUDIO_MASTER,
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_MASTER,
};

//音频参数设置，例如采样率，采样格式，双声道
struct SwrContext *audio_convert_ctx = NULL;

//typedef struct PacketQueue {
//    AVPacketList *first_pkt, *last_pkt;
//    int nb_packets;
//    int size;
//    SDL_mutex *mutex;
//    SDL_cond *cond;
//} PacketQueue;

PacketQueue audioq;

int quit = 0;

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
    
    AVPacketList *pkt1;
    if(av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    
    SDL_LockMutex(q->mutex);
    
    if (!q->last_pkt) {
        q->first_pkt = pkt1;
    }else{
        q->last_pkt->next = pkt1;
    }
    
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);
    
    SDL_UnlockMutex(q->mutex);
    return 0;
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;
    
    SDL_LockMutex(q->mutex);
    
    for(;;) {
        
        if(quit) {
            ret = -1;
            break;
        }
        
        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size) {
    
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    static AVFrame frame;
    
    int len1, data_size = 0;
    
    for(;;) {
        //有未解码的数据
        while(audio_pkt_size > 0) {
            int got_frame = 0;
            len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
            if(len1 < 0) {
                //解码失败
                /* if error, skip frame */
                audio_pkt_size = 0;
                break;
            }
            //更新音频数据大小
            audio_pkt_data += len1;
            //减去已经解码的音频长度
            audio_pkt_size -= len1;
            data_size = 0;
            if(got_frame) {
                //fprintf(stderr, "channels:%d, nb_samples:%d, sample_fmt:%d \n", aCodecCtx->channels, frame.nb_samples, aCodecCtx->sample_fmt);
                /*
                 data_size = av_samples_get_buffer_size(NULL,
                 aCodecCtx->channels,
                 frame.nb_samples,
                 aCodecCtx->sample_fmt,
                 1);
                 */
                data_size = 2 * 2 * frame.nb_samples;
                if (data_size <= buf_size) {
                    return -1;
                }
                //转成声卡识别的声音
                swr_convert(audio_convert_ctx,
                            &audio_buf,
                            MAX_AUDIO_FRAME_SIZE*3/2,
                            (const uint8_t **)frame.data,
                            frame.nb_samples);
                
                //memcpy(audio_buf, frame.data[0], data_size);
            }
            if(data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if(pkt.data)
            av_free_packet(&pkt);
        
        if(quit) {
            return -1;
        }
        //当音频队列中没有数据时，退出
        if(packet_queue_get(&audioq, &pkt, 1) < 0) {
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }
}
//userdata：需要解码的数据，stream：声卡驱动的缓冲区，的一个地址，len：需要的长度，stream的大小
void audio_callback(void *userdata, Uint8 *stream, int len) {
    
    AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
    int len1, audio_size;
    
    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;
    
    //声卡驱动的缓冲区空间 > 0
    while(len > 0) {
        //audio_buf_index >= audio_buf_size：缓冲区已经没有数据
        if(audio_buf_index >= audio_buf_size) {
            //音频解码，解码成功后会拿到解码后数据的大小
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
            if(audio_size < 0) {
                //解码失败后，加一个静默音
                /* If error, output silence */
                audio_buf_size = 1024; // arbitrary?
                memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if(len1 > len)
            len1 = len;
        fprintf(stderr, "index=%d, len1=%d, len=%d\n",
                audio_buf_index,
                len,
                len1);
        //将解码后audio_buf的数据拷贝到stream中去
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

int media_player(char *url) {
    
    int            ret = -1;
    int             i, videoStream, audioStream;
    
    AVFormatContext *pFormatCtx = NULL;
    
    //for video decode
    AVCodecContext  *pCodecCtxOrig = NULL;
    AVCodecContext  *pCodecCtx = NULL;
    AVCodec         *pCodec = NULL;
    
    struct SwsContext *sws_ctx = NULL;
    
    AVPicture      *pict = NULL;
    AVFrame         *pFrame = NULL;
    AVPacket        packet;
    int             frameFinished;
    
    //音频解码上下文
    //for audio decode
    AVCodecContext  *aCodecCtxOrig = NULL;
    AVCodecContext  *aCodecCtx = NULL;
    AVCodec         *aCodec = NULL;
    
    
    int64_t in_channel_layout;
    int64_t out_channel_layout;
    
    //for video render
    int          w_width = 640;
    int           w_height = 480;
    
    int             pixformat;
    SDL_Rect        rect;
    
    SDL_Window      *win;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    
    //for event
    SDL_Event       event;
    
    //播放音频的参数设置，wanted_spec：我们想要的参数，spec老的参数
    //for audio
    SDL_AudioSpec   wanted_spec, spec;
    
    if(!url) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: command <file>");
        return ret;
    }
    
    // Register all formats and codecs
    av_register_all();
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize SDL - %s\n", SDL_GetError());
        return ret;
    }
    
    // Open video file
    if(avformat_open_input(&pFormatCtx, url, NULL, NULL)!=0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open multi-media file");
        goto __FAIL; // Couldn't open file
    }
    
    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find stream information ");
        goto __FAIL;
    }
    
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, url, 0);
    
    // Find the first video stream
    videoStream=-1;
    audioStream=-1;
    
    for(i=0; i<pFormatCtx->nb_streams; i++) {
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO &&
           videoStream < 0) {
            videoStream=i;
        }
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
           audioStream < 0) {
            audioStream=i;
        }
    }
    
    if(videoStream==-1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, " Didn't find a video stream ");
        goto __FAIL; // Didn't find a video stream
    }
    
    if(audioStream==-1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, " Didn't find a audio stream ");
        goto __FAIL; // Didn't find a video stream
    }
    //获取音频解码器上下文
    aCodecCtxOrig=pFormatCtx->streams[audioStream]->codec;
    //获取音频解码器
    aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
    if(!aCodec) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported codec! ");
        goto __FAIL; // Didn't find a video stream
    }
    //创建音频解码器上下文
    // Copy context
    aCodecCtx = avcodec_alloc_context3(aCodec);
    //将原始的音频解码器上下文进行拷贝，防止操作原始的上下文
    if(avcodec_copy_context(aCodecCtx, aCodecCtxOrig) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't copy codec context! ");
        goto __FAIL; // Didn't find a video stream
    }
    
    //设置想要的音频播放参数
    // Set audio settings from codec info
    wanted_spec.freq = aCodecCtx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = aCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = aCodecCtx;
    //打开音频设备
    if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio device - %s!", SDL_GetError());
        goto __FAIL;
    }
    
    avcodec_open2(aCodecCtx, aCodec, NULL);
    //初始化音频队列，读取音频包后不是马上解码，而是存储到音频队列中，当声卡需要数据的时候，调用回调函数，回调函数再从队列中取出一个个音频包，再进行解码，解码后的数据再拷贝到音频驱动中去
    
    packet_queue_init(&audioq);
    
    in_channel_layout = av_get_default_channel_layout(aCodecCtx->channels);
    out_channel_layout = in_channel_layout; //AV_CH_LAYOUT_STEREO;
    fprintf(stderr, "in layout:%lld, out layout:%lld \n", in_channel_layout, out_channel_layout);
    
    //初始化重采样上下文
    audio_convert_ctx = swr_alloc_set_opts(NULL,
                                           out_channel_layout,//输出的声道
                                           AV_SAMPLE_FMT_S16,//输出的采样大小
                                           aCodecCtx->sample_rate,//输出的采样率
                                           in_channel_layout,//输入的声道
                                           aCodecCtx->sample_fmt,//输入的采样格式
                                           aCodecCtx->sample_rate,//输出的采样率
                                           0,
                                           NULL);
    //swr_init(audio_convert_ctx);
    //开始播放
    SDL_PauseAudio(0);
    
    // Get a pointer to the codec context for the video stream
    pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
    
    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec==NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported codec!");
        goto __FAIL;
    }
    
    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to copy context of codec!");
        goto __FAIL;
    }
    
    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio decoder!");
        goto __FAIL;
    }
    
    // Allocate video frame
    pFrame=av_frame_alloc();
    
    w_width = pCodecCtx->width;
    w_height = pCodecCtx->height;
    
    fprintf(stderr, "width:%d, height:%d\n", w_width, w_height);
    
    win = SDL_CreateWindow("Media Player",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           w_width, w_height,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!win){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window!");
        goto __FAIL;
    }
    
    renderer = SDL_CreateRenderer(win, -1, 0);
    if(!renderer){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create renderer!");
        goto __FAIL;
    }
    
    pixformat = SDL_PIXELFORMAT_IYUV;
    texture = SDL_CreateTexture(renderer,
                                pixformat,
                                SDL_TEXTUREACCESS_STREAMING,
                                w_width,
                                w_height);
    if(!texture){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Texture!");
        goto __FAIL;
    }
    
    // initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                             pCodecCtx->height,
                             pCodecCtx->pix_fmt,
                             pCodecCtx->width,
                             pCodecCtx->height,
                             AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR,
                             NULL,
                             NULL,
                             NULL);
    
    pict = (AVPicture*)malloc(sizeof(AVPicture));
    avpicture_alloc(pict,
                    AV_PIX_FMT_YUV420P,
                    pCodecCtx->width,
                    pCodecCtx->height);
    
    // Read frames and save first five frames to disk
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            
            // Did we get a video frame?
            if(frameFinished) {
                
                // Convert the image into YUV format that SDL uses
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pict->data, pict->linesize);
                
                SDL_UpdateYUVTexture(texture, NULL,
                                     pict->data[0], pict->linesize[0],
                                     pict->data[1], pict->linesize[1],
                                     pict->data[2], pict->linesize[2]);
                
                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &rect);
                SDL_RenderPresent(renderer);
                
                av_free_packet(&packet);
            }
        } else if(packet.stream_index==audioStream) { //for audio
            packet_queue_put(&audioq, &packet);
        } else {
            av_free_packet(&packet);
        }
        
        // Free the packet that was allocated by av_read_frame
        SDL_PollEvent(&event);
        switch(event.type) {
            case SDL_QUIT:
                quit = 1;
                goto __QUIT;
                break;
            default:
                break;
        }
        
    }
    
__QUIT:
    ret = 0;
    
__FAIL:
    // Free the YUV frame
    if(pFrame){
        av_frame_free(&pFrame);
    }
    
    // Close the codecs
    if(pCodecCtxOrig){
        avcodec_close(pCodecCtxOrig);
    }
    
    if(pCodecCtx){
        avcodec_close(pCodecCtx);
    }
    
    if(aCodecCtxOrig) {
        avcodec_close(aCodecCtxOrig);
    }
    
    if(aCodecCtx) {
        avcodec_close(aCodecCtx);
    }
    
    // Close the video file
    if(pFormatCtx){
        avformat_close_input(&pFormatCtx);
    }
    
    if(pict){
        avpicture_free(pict);
        free(pict);
    }
    
    if(win){
        SDL_DestroyWindow(win);
    }
    
    if(renderer){
        SDL_DestroyRenderer(renderer);
    }
    
    if(texture){
        SDL_DestroyTexture(texture);
    }
    
    SDL_Quit();
    
    return ret;
}

void alloc_picture(void *userdata) {

  VideoState *is = (VideoState *)userdata;
  VideoPicture *vp;

  vp = &is->pictq[is->pictq_windex];
  if(vp->bmp) {//free space if vp->pict is not NULL
    avpicture_free(vp->bmp);
    free(vp->bmp);
  }

  // Allocate a place to put our YUV image on that screen
  SDL_LockMutex(text_mutex);
  vp->bmp = (AVPicture*)malloc(sizeof(AVPicture));
  if(vp->bmp){
    avpicture_alloc(vp->bmp,
            AV_PIX_FMT_YUV420P,
            is->video_ctx->width,
            is->video_ctx->height);
  }
  SDL_UnlockMutex(text_mutex);

  vp->width = is->video_ctx->width;
  vp->height = is->video_ctx->height;
  vp->allocated = 1;

}

int queue_picture(VideoState *is, AVFrame *pFrame) {

  VideoPicture *vp;
  int dst_pix_fmt;
  AVPicture pict;

  /* wait until we have space for a new pic */
  SDL_LockMutex(is->pictq_mutex);
  while(is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&
    !is->quit) {
    SDL_CondWait(is->pictq_cond, is->pictq_mutex);
  }
  SDL_UnlockMutex(is->pictq_mutex);

  if(is->quit){
    fprintf(stderr, "quit from queue_picture....\n");
    return -1;
  }

  // windex is set to 0 initially
  vp = &is->pictq[is->pictq_windex];

  /*
  fprintf(stderr, "vp.width=%d, vp.height=%d, video_ctx.width=%d, video_ctx.height=%d\n",
          vp->width,
          vp->height,
          is->video_ctx->width,
          is->video_ctx->height);
  */

  /* allocate or resize the buffer! */
  if(!vp->bmp ||
     vp->width != is->video_ctx->width ||
     vp->height != is->video_ctx->height) {

    vp->allocated = 0;
    alloc_picture(is);
    if(is->quit) {
      fprintf(stderr, "quit from queue_picture2....\n");
      return -1;
    }
  }

  /* We have a place to put our picture on the queue */

  if(vp->bmp) {

    // Convert the image into YUV format that SDL uses
    sws_scale(is->video_sws_ctx, (uint8_t const * const *)pFrame->data,
          pFrame->linesize, 0, is->video_ctx->height,
          vp->bmp->data, vp->bmp->linesize);
    
    /* now we inform our display thread that we have a pic ready */
    if(++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
      is->pictq_windex = 0;
    }
    SDL_LockMutex(is->pictq_mutex);
    is->pictq_size++;
    SDL_UnlockMutex(is->pictq_mutex);
  }
  return 0;
}

//视频解码线程
int video_thread(void *arg) {
  VideoState *is = (VideoState *)arg;
  AVPacket pkt1, *packet = &pkt1;
  int frameFinished;
  AVFrame *pFrame;

  pFrame = av_frame_alloc();

  for(;;) {
    //从视频队列中取出视频AVPacket包
    if(packet_queue_get(&is->videoq, packet, 1) < 0) {
      // means we quit getting packets
      break;
    }

    // Decode video frame
    avcodec_decode_video2(is->video_ctx, pFrame, &frameFinished, packet);

    // Did we get a video frame?
    if(frameFinished) {
      //视频帧放入解码后的视频队列中去
      if(queue_picture(is, pFrame) < 0) {
    break;
      }
    }

    av_free_packet(packet);
  }
  av_frame_free(&pFrame);
  return 0;
}

//通过stream_index找到流的信息
int stream_component_open(VideoState *is, int stream_index) {

  int64_t in_channel_layout, out_channel_layout;

  AVFormatContext *pFormatCtx = is->pFormatCtx;
  AVCodecContext *codecCtx = NULL;
  AVCodec *codec = NULL;
  SDL_AudioSpec wanted_spec, spec;

  if(stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
    return -1;
  }

  codec = avcodec_find_decoder(pFormatCtx->streams[stream_index]->codec->codec_id);
  if(!codec) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }

  codecCtx = avcodec_alloc_context3(codec);
  if(avcodec_copy_context(codecCtx, pFormatCtx->streams[stream_index]->codec) != 0) {
    fprintf(stderr, "Couldn't copy codec context");
    return -1; // Error copying codec context
  }


  if(codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
    // Set audio settings from codec info
    wanted_spec.freq = codecCtx->sample_rate;//采样率
    wanted_spec.format = AUDIO_S16SYS;//采样格式
    wanted_spec.channels = codecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;//采样个数，每秒钟有多少个采样
    wanted_spec.callback = audio_callback;//回调函数
    wanted_spec.userdata = is;//回调函数参数
    
    if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
      fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
      return -1;
    }
  }

  if(avcodec_open2(codecCtx, codec, NULL) < 0) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }

  switch(codecCtx->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
  //音频流index
    is->audioStream = stream_index;
    is->audio_st = pFormatCtx->streams[stream_index];//具体的音频流
    is->audio_ctx = codecCtx;
    is->audio_buf_size = 0;
    is->audio_buf_index = 0;
    //设置音频包
    memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
    //音频队列初始化
    packet_queue_init(&is->audioq);
    //播放声音
    SDL_PauseAudio(0);

    in_channel_layout=av_get_default_channel_layout(is->audio_ctx->channels);
    out_channel_layout = in_channel_layout;
    //重采样
    is->audio_swr_ctx = swr_alloc_set_opts(NULL,
                       out_channel_layout,
                       AV_SAMPLE_FMT_S16,
                       is->audio_ctx->sample_rate,
                       in_channel_layout,
                       is->audio_ctx->sample_fmt,
                       is->audio_ctx->sample_rate,
                       0,
                       NULL);

    fprintf(stderr, "swr opts: out_channel_layout:%lld, out_sample_fmt:%d, out_sample_rate:%d, in_channel_layout:%lld, in_sample_fmt:%d, in_sample_rate:%d",
                    out_channel_layout,
            AV_SAMPLE_FMT_S16,
            is->audio_ctx->sample_rate,
            in_channel_layout,
            is->audio_ctx->sample_fmt,
            is->audio_ctx->sample_rate);
    break;

  case AVMEDIA_TYPE_VIDEO:
    is->videoStream = stream_index;
    is->video_st = pFormatCtx->streams[stream_index];
    is->video_ctx = codecCtx;
    packet_queue_init(&is->videoq);
    //创建视频解码线程
    is->video_tid = SDL_CreateThread(video_thread, "video_thread", is);
    is->video_sws_ctx = sws_getContext(is->video_ctx->width,
                 is->video_ctx->height,
                 is->video_ctx->pix_fmt,
                 is->video_ctx->width,
                 is->video_ctx->height,
                 AV_PIX_FMT_YUV420P,
                 SWS_BILINEAR,
                 NULL, NULL, NULL);
    break;
  default:
    break;
  }

  return 0;
}

void create_window(int width, int height) {
    win = SDL_CreateWindow("Media Player",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           width,
                           height,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    renderer = SDL_CreateRenderer(win, -1, 0);
    
    Uint32 pixformat = SDL_PIXELFORMAT_IYUV;
    texture = SDL_CreateTexture(renderer,
                                pixformat,
                                SDL_TEXTUREACCESS_STREAMING,
                                width,
                                height);
}

int decode_thread(void *arg) {

  Uint32 pixformat;

  VideoState *is = (VideoState *)arg;
  AVFormatContext *pFormatCtx = NULL;
  AVPacket pkt1, *packet = &pkt1;

  int i;
  int video_index = -1;
  int audio_index = -1;

  is->videoStream = -1;
  is->audioStream = -1;

  global_video_state = is;

  // Open video file
  if(avformat_open_input(&pFormatCtx, is->filename, NULL, NULL)!=0)
    return -1; // Couldn't open file

  is->pFormatCtx = pFormatCtx;
  
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    return -1; // Couldn't find stream information
  
  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, is->filename, 0);
  
  // Find the first video stream
  for(i=0; i<pFormatCtx->nb_streams; i++) {
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO &&
       video_index < 0) {
      video_index=i;
    }
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO &&
       audio_index < 0) {
      audio_index=i;
    }
  }

  if(audio_index >= 0) {
    stream_component_open(is, audio_index);
  }
  if(video_index >= 0) {
    stream_component_open(is, video_index);
  }

  if(is->videoStream < 0 || is->audioStream < 0) {
    fprintf(stderr, "%s: could not open codecs\n", is->filename);
    goto fail;
  }

  fprintf(stderr, "video context: width=%d, height=%d\n", is->video_ctx->width, is->video_ctx->height);

  // main decode loop
  for(;;) {

    if(is->quit) {
      //发送信号量，告诉等待线程，等待线程判断是否quit
      SDL_CondSignal(is->videoq.cond);
      SDL_CondSignal(is->audioq.cond);
      break;
    }

    // seek stuff goes here
    if(is->audioq.size > MAX_AUDIOQ_SIZE ||
       is->videoq.size > MAX_VIDEOQ_SIZE) {
      SDL_Delay(10);
      continue;
    }

    if(av_read_frame(is->pFormatCtx, packet) < 0) {
      if(is->pFormatCtx->pb->error == 0) {
    SDL_Delay(100); /* no error; wait for user input */
    continue;
      } else {
    break;
      }
    }

    // Is this a packet from the video stream?
    if(packet->stream_index == is->videoStream) {
      //放入视频队列
      packet_queue_put(&is->videoq, packet);
      fprintf(stderr, "put video queue, size :%d\n", is->videoq.nb_packets);
    } else if(packet->stream_index == is->audioStream) {
      //放入音频队列
      packet_queue_put(&is->audioq, packet);
      fprintf(stderr, "put audio queue, size :%d\n", is->audioq.nb_packets);
    } else {
      av_free_packet(packet);
    }

  }

  /* all done - wait for it */
  while(!is->quit) {
    SDL_Delay(100);
  }

 fail:
  if(1){
    SDL_Event event;
    event.type = FF_QUIT_EVENT;//发信号，退出
    event.user.data1 = is;
    SDL_PushEvent(&event);
  }

  return 0;
}

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
  SDL_Event event;
  event.type = FF_REFRESH_EVENT;
  event.user.data1 = opaque;
  SDL_PushEvent(&event);
  return 0; /* 0 means stop timer */
}

static void schedule_refresh(VideoState *is, int delay) {
  //触发主线程
  SDL_AddTimer(delay, sdl_refresh_timer_cb, is);
}

void video_display(VideoState *is) {

  SDL_Rect rect;
  VideoPicture *vp;
  float aspect_ratio;
  int w, h, x, y;
  int i;

  vp = &is->pictq[is->pictq_rindex];
  if(vp->bmp) {

    if(is->video_ctx->sample_aspect_ratio.num == 0) {
      aspect_ratio = 0;
    } else {
      aspect_ratio = av_q2d(is->video_ctx->sample_aspect_ratio) *
    is->video_ctx->width / is->video_ctx->height;
    }

    if(aspect_ratio <= 0.0) {
      aspect_ratio = (float)is->video_ctx->width /
    (float)is->video_ctx->height;
    }

    /*
    h = screen->h;
    w = ((int)rint(h * aspect_ratio)) & -3;
    if(w > screen->w) {
      w = screen->w;
      h = ((int)rint(w / aspect_ratio)) & -3;
    }
    x = (screen->w - w) / 2;
    y = (screen->h - h) / 2;
    */

    SDL_UpdateYUVTexture( texture, NULL,
                          vp->bmp->data[0], vp->bmp->linesize[0],
                          vp->bmp->data[1], vp->bmp->linesize[1],
                          vp->bmp->data[2], vp->bmp->linesize[2]);
    
    rect.x = 0;
    rect.y = 0;
    rect.w = is->video_ctx->width;
    rect.h = is->video_ctx->height;

    SDL_LockMutex(text_mutex);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_RenderPresent(renderer);
    SDL_UnlockMutex(text_mutex);

  }
}

//主线程！！！
void video_refresh_timer(void *userdata) {

  VideoState *is = (VideoState *)userdata;
  VideoPicture *vp;
  
  if(is->video_st) {
    //视频解码数据为0,则递归查询数据
    if(is->pictq_size == 0) {
      schedule_refresh(is, 1); //if the queue is empty, so we shoud be as fast as checking queue of picture
    } else {
      vp = &is->pictq[is->pictq_rindex];
      /* Now, normally here goes a ton of code
     about timing, etc. we're just going to
     guess at a delay for now. You can
     increase and decrease this value and hard code
     the timing - but I don't suggest that ;)
     We'll learn how to do it for real later.
      */
      //递归40毫秒播一帧
      schedule_refresh(is, 40);
      
      /* show the picture! */
      //展示当前帧
      video_display(is);
      
      /* update queue for next picture! */
      if(++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
    is->pictq_rindex = 0;
      }
      SDL_LockMutex(is->pictq_mutex);
      is->pictq_size--;

      SDL_CondSignal(is->pictq_cond);
      SDL_UnlockMutex(is->pictq_mutex);
    }
  } else {
    schedule_refresh(is, 100);
  }
}

int media_player1(char *url) {
    int           ret = -1;
    SDL_Event       event;
    VideoState      *is;
    
    if(!url) {
        fprintf(stderr, "Usage: test <file>\n");
        return -1;
    }
    
    //big struct, it's core
    //分配空间
    is = av_mallocz(sizeof(VideoState));
    
    // Register all formats and codecs
    av_register_all();
    
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    create_window(640,360);
    
    text_mutex = SDL_CreateMutex();
    
    av_strlcpy(is->filename, url, sizeof(is->filename));
    
    //创建解码视频帧队列线程锁和信号量
    is->pictq_mutex = SDL_CreateMutex();
    is->pictq_cond = SDL_CreateCond();
    
    //set timer
    //每40秒渲染一次视频帧
    schedule_refresh(is, 40);
    
    //创建解复用线程
    is->parse_tid = SDL_CreateThread(decode_thread, "decode_thread", is);
    if(!is->parse_tid) {
        av_free(is);
        goto __FAIL;
    }
    
    for(;;) {
        
        SDL_WaitEvent(&event);
        switch(event.type) {
            case FF_QUIT_EVENT:
            case SDL_QUIT:
                fprintf(stderr, "receive a QUIT event: %d\n", event.type);
                is->quit = 1;
                //SDL_Quit();
                //return 0;
                goto __QUIT;
                break;
            case FF_REFRESH_EVENT:
                //fprintf(stderr, "receive a refresh event: %d\n", event.type);
                video_refresh_timer(event.user.data1);
                break;
            default:
                break;
        }
    }
    
__QUIT:
    ret = 0;
    
    
__FAIL:
    SDL_Quit();
    return ret;
}
