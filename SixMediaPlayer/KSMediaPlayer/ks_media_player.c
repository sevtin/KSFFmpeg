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

//同步方案
enum {
    AV_SYNC_AUDIO_MASTER,
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_MASTER,
};

/* Since we only have one decoding thread, the Big Struct
 can be global in case we need it. */
VideoState *global_video_state;

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
    
    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
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
        
        if(global_video_state->quit) {
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

double get_audio_clock(VideoState *is) {
    double pts;
    int hw_buf_size, bytes_per_sec, n;
    
    pts = is->audio_clock; /* maintained in the audio thread */
    hw_buf_size = is->audio_buf_size - is->audio_buf_index;
    bytes_per_sec = 0;
    n = is->audio_ctx->channels * 2;
    if(is->audio_st) {
        bytes_per_sec = is->audio_ctx->sample_rate * n;
    }
    if(bytes_per_sec) {
        pts -= (double)hw_buf_size / bytes_per_sec;
    }
    return pts;
}
double get_video_clock(VideoState *is) {
    double delta;
    
    delta = (av_gettime() - is->video_current_pts_time) / 1000000.0;
    return is->video_current_pts + delta;
}
double get_external_clock(VideoState *is) {
    return av_gettime() / 1000000.0;
}

double get_master_clock(VideoState *is) {
    if(is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        return get_video_clock(is);
    } else if(is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        return get_audio_clock(is);
    } else {
        return get_external_clock(is);
    }
}


/* Add or subtract samples to get a better sync, return new
 audio buffer size */
int synchronize_audio(VideoState *is, short *samples,
                      int samples_size, double pts) {
    int n;
    double ref_clock;
    
    n = 2 * is->audio_ctx->channels;
    
    if(is->av_sync_type != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int wanted_size, min_size, max_size /*, nb_samples */;
        
        ref_clock = get_master_clock(is);
        diff = get_audio_clock(is) - ref_clock;
        
        if(diff < AV_NOSYNC_THRESHOLD) {
            // accumulate the diffs
            is->audio_diff_cum = diff + is->audio_diff_avg_coef
            * is->audio_diff_cum;
            if(is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                is->audio_diff_avg_count++;
            } else {
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);
                if(fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_size = samples_size + ((int)(diff * is->audio_ctx->sample_rate) * n);
                    min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
                    max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);
                    if(wanted_size < min_size) {
                        wanted_size = min_size;
                    } else if (wanted_size > max_size) {
                        wanted_size = max_size;
                    }
                    if(wanted_size < samples_size) {
                        /* remove samples */
                        samples_size = wanted_size;
                    } else if(wanted_size > samples_size) {
                        uint8_t *samples_end, *q;
                        int nb;
                        
                        /* add samples by copying final sample*/
                        nb = (samples_size - wanted_size);
                        samples_end = (uint8_t *)samples + samples_size - n;
                        q = samples_end + n;
                        while(nb > 0) {
                            memcpy(q, samples_end, n);
                            q += n;
                            nb -= n;
                        }
                        samples_size = wanted_size;
                    }
                }
            }
        } else {
            /* difference is TOO big; reset diff stuff */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }
    return samples_size;
}

int audio_decode_frame(VideoState *is, uint8_t *audio_buf, int buf_size, double *pts_ptr) {
    
    int len1, data_size = 0;
    AVPacket *pkt = &is->audio_pkt;
    double pts;
    int n;
    
    
    for(;;) {
        while(is->audio_pkt_size > 0) {
            int got_frame = 0;
            len1 = avcodec_decode_audio4(is->audio_ctx, &is->audio_frame, &got_frame, pkt);
            if(len1 < 0) {
                /* if error, skip frame */
                is->audio_pkt_size = 0;
                break;
            }
            data_size = 0;
            if(got_frame) {
                /*
                 data_size = av_samples_get_buffer_size(NULL,
                 is->audio_ctx->channels,
                 is->audio_frame.nb_samples,
                 is->audio_ctx->sample_fmt,
                 1);
                 */
                data_size = 2 * is->audio_frame.nb_samples * 2;
                if (data_size <= buf_size) {
                    return -1;
                }
                swr_convert(is->audio_swr_ctx,
                            &audio_buf,
                            MAX_AUDIO_FRAME_SIZE*3/2,
                            (const uint8_t **)is->audio_frame.data,
                            is->audio_frame.nb_samples);
                
                //fwrite(audio_buf, 1, data_size, audiofd);
                memcpy(audio_buf, is->audio_frame.data[0], data_size);
            }
            is->audio_pkt_data += len1;
            is->audio_pkt_size -= len1;
            if(data_size <= 0) {
                /* No data yet, get more frames */
                continue;
            }
            pts = is->audio_clock;
            *pts_ptr = pts;
            n = 2 * is->audio_ctx->channels;
            is->audio_clock += (double)data_size /
            (double)(n * is->audio_ctx->sample_rate);
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if(pkt->data)
            av_free_packet(pkt);
        
        if(is->quit) {
            return -1;
        }
        /* next packet */
        if(packet_queue_get(&is->audioq, pkt, 1) < 0) {
            return -1;
        }
        is->audio_pkt_data = pkt->data;
        is->audio_pkt_size = pkt->size;
        /* if update, update the audio clock w/pts */
        if(pkt->pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_st->time_base)*pkt->pts;
        }
    }
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    
    VideoState *is = (VideoState *)userdata;
    int len1, audio_size;
    double pts;
    
    SDL_memset(stream, 0, len);
    
    while(len > 0) {
        if(is->audio_buf_index >= is->audio_buf_size) {
            /* We have already sent all our data; get more */
            audio_size = audio_decode_frame(is, is->audio_buf, sizeof(is->audio_buf), &pts);
            if(audio_size < 0) {
                /* If error, output silence */
                is->audio_buf_size = 1024 * 2 * 2;
                memset(is->audio_buf, 0, is->audio_buf_size);
            } else {
                audio_size = synchronize_audio(is, (int16_t *)is->audio_buf,
                                               audio_size, pts);
                is->audio_buf_size = audio_size;
            }
            is->audio_buf_index = 0;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if(len1 > len)
            len1 = len;
        SDL_MixAudio(stream,(uint8_t *)is->audio_buf + is->audio_buf_index, len1, SDL_MIX_MAXVOLUME);
        //memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
}

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = opaque;
    SDL_PushEvent(&event);
    return 0; /* 0 means stop timer */
}

/* schedule a video refresh in 'delay' ms */
static void schedule_refresh(VideoState *is, int delay) {
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
        
        SDL_UpdateYUVTexture( texture, NULL,
                             vp->bmp->data[0], vp->bmp->linesize[0],
                             vp->bmp->data[1], vp->bmp->linesize[1],
                             vp->bmp->data[2], vp->bmp->linesize[2]);
        
        rect.x = 0;
        rect.y = 0;
        rect.w = is->video_ctx->width;
        rect.h = is->video_ctx->height;
        SDL_LockMutex(text_mutex);
        SDL_RenderClear( renderer );
        SDL_RenderCopy( renderer, texture, NULL, &rect);
        SDL_RenderPresent( renderer );
        SDL_UnlockMutex(text_mutex);
        
    }
}

void video_refresh_timer(void *userdata) {
    
    VideoState *is = (VideoState *)userdata;
    VideoPicture *vp;
    double actual_delay, delay, sync_threshold, ref_clock, diff;
    
    if(is->video_st) {
        if(is->pictq_size == 0) {
            schedule_refresh(is, 1);
            //fprintf(stderr, "no picture in the queue!!!\n");
        } else {
            //fprintf(stderr, "get picture from queue!!!\n");
            vp = &is->pictq[is->pictq_rindex];
            
            is->video_current_pts = vp->pts;
            is->video_current_pts_time = av_gettime();
            delay = vp->pts - is->frame_last_pts; /* the pts from last time */
            if(delay <= 0 || delay >= 1.0) {
                /* if incorrect delay, use previous one */
                delay = is->frame_last_delay;
            }
            /* save for next time */
            is->frame_last_delay = delay;
            is->frame_last_pts = vp->pts;//更新展示pts的值
            
            /* update delay to sync to audio if not master source */
            if(is->av_sync_type != AV_SYNC_VIDEO_MASTER) {
                ref_clock = get_master_clock(is);
                diff = vp->pts - ref_clock;
                
                /* Skip or repeat the frame. Take delay into account
                 FFPlay still doesn't "know if this is the best guess." */
                sync_threshold = (delay > AV_SYNC_THRESHOLD) ? delay : AV_SYNC_THRESHOLD;
                if(fabs(diff) < AV_NOSYNC_THRESHOLD) {
                    if(diff <= -sync_threshold) {
                        delay = 0;
                    } else if(diff >= sync_threshold) {
                        delay = 2 * delay;
                    }
                }
            }
            is->frame_timer += delay;
            /* computer the REAL delay */
            actual_delay = is->frame_timer - (av_gettime() / 1000000.0);
            if(actual_delay < 0.010) {
                /* Really it should skip the picture instead */
                actual_delay = 0.010;
            }
            schedule_refresh(is, (int)(actual_delay * 1000 + 0.5));
            
            /* show the picture! */
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

void alloc_picture(void *userdata) {
    
    int ret;
    
    VideoState *is = (VideoState *)userdata;
    VideoPicture *vp;
    
    vp = &is->pictq[is->pictq_windex];
    if(vp->bmp) {
        
        // we already have one make another, bigger/smaller
        avpicture_free(vp->bmp);
        free(vp->bmp);
        
        vp->bmp = NULL;
    }
    
    // Allocate a place to put our YUV image on that screen
    SDL_LockMutex(text_mutex);
    
    vp->bmp = (AVPicture*)malloc(sizeof(AVPicture));
    ret = avpicture_alloc(vp->bmp, AV_PIX_FMT_YUV420P, is->video_ctx->width, is->video_ctx->height);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate temporary picture: %s\n", av_err2str(ret));
    }
    
    SDL_UnlockMutex(text_mutex);
    
    vp->width = is->video_ctx->width;
    vp->height = is->video_ctx->height;
    vp->allocated = 1;
    
}

int queue_picture(VideoState *is, AVFrame *pFrame, double pts) {
    
    VideoPicture *vp;
    
    /* wait until we have space for a new pic */
    SDL_LockMutex(is->pictq_mutex);
    while(is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&
          !is->quit) {
        SDL_CondWait(is->pictq_cond, is->pictq_mutex);
    }
    SDL_UnlockMutex(is->pictq_mutex);
    
    if(is->quit)
        return -1;
    
    // windex is set to 0 initially
    vp = &is->pictq[is->pictq_windex];
    
    /* allocate or resize the buffer! */
    if(!vp->bmp ||
       vp->width != is->video_ctx->width ||
       vp->height != is->video_ctx->height) {
        
        vp->allocated = 0;
        alloc_picture(is);
        if(is->quit) {
            return -1;
        }
    }
    
    /* We have a place to put our picture on the queue */
    if(vp->bmp) {
        //更新PTS！！！
        vp->pts = pts;
        
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

double synchronize_video(VideoState *is, AVFrame *src_frame, double pts) {
    
    double frame_delay;
    
    if(pts != 0) {
        /* if we have pts, set video clock to it */
        is->video_clock = pts;
    } else {
        /* if we aren't given a pts, set it to the clock */
        //使用上一次的is->video_clock作为pts
        pts = is->video_clock;
    }
    /* update the video clock */
    //用时间基计算每一帧的间隔
    frame_delay = av_q2d(is->video_ctx->time_base);
    /* if we are repeating a frame, adjust clock accordingly */
    //解码后的视频帧src_frame，他有个repeat_pict会重复的播放，repeat_pict：重复的次数
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    //is->video_clock:下一帧的pts
    is->video_clock += frame_delay;
    return pts;
}

int decode_video_thread(void *arg) {
    VideoState *is = (VideoState *)arg;
    AVPacket pkt1, *packet = &pkt1;
    int frameFinished;
    AVFrame *pFrame;
    double pts;
    
    // 存放解码后的数据帧
    pFrame = av_frame_alloc();
    
    for(;;) {
        if(packet_queue_get(&is->videoq, packet, 1) < 0) {
            // means we quit getting packets
            break;
        }
        pts = 0;
        
        // Decode video frame
        avcodec_decode_video2(is->video_ctx, pFrame, &frameFinished, packet);
        //av_frame_get_best_effort_timestamp(pFrame)：获取做合适的pts
        if((pts = av_frame_get_best_effort_timestamp(pFrame)) != AV_NOPTS_VALUE) {
        } else {
            pts = 0;
        }
        //时间基换算，换算成秒
        pts *= av_q2d(is->video_st->time_base);
        
        // Did we get a video frame?
        if(frameFinished) {
            //计算pts
            pts = synchronize_video(is, pFrame, pts);
            if(queue_picture(is, pFrame, pts) < 0) {
                break;
            }
        }
        av_free_packet(packet);
    }
    av_frame_free(&pFrame);
    return 0;
}

int stream_component_open(VideoState *is, int stream_index) {
    
    AVFormatContext *pFormatCtx = is->pFormatCtx;
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;
    SDL_AudioSpec wanted_spec, spec;
    
    if(stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
        return -1;
    }
    
    codecCtx = avcodec_alloc_context3(NULL);
    
    
    int ret = avcodec_parameters_to_context(codecCtx, pFormatCtx->streams[stream_index]->codecpar);
    if (ret < 0)
        return -1;
    
    codec = avcodec_find_decoder(codecCtx->codec_id);
    if(!codec) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
    
    
    if(codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
        
        // Set audio settings from codec info
        wanted_spec.freq = codecCtx->sample_rate;
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.channels = 2;//codecCtx->channels;
        wanted_spec.silence = 0;
        wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
        wanted_spec.callback = audio_callback;
        wanted_spec.userdata = is;
        
        fprintf(stderr, "wanted spec: channels:%d, sample_fmt:%d, sample_rate:%d \n",
                2, AUDIO_S16SYS, codecCtx->sample_rate);
        
        if(SDL_OpenAudio(&wanted_spec, &spec) < 0) {
            fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
            return -1;
        }
        is->audio_hw_buf_size = spec.size;
    }
    
    if(avcodec_open2(codecCtx, codec, NULL) < 0) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }
    
    switch(codecCtx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            is->audioStream = stream_index;
            is->audio_st = pFormatCtx->streams[stream_index];
            is->audio_ctx = codecCtx;
            is->audio_buf_size = 0;
            is->audio_buf_index = 0;
            memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
            packet_queue_init(&is->audioq);
            
            //Out Audio Param
            uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
            
            //AAC:1024  MP3:1152
            int out_nb_samples= is->audio_ctx->frame_size;
            //AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
            
            int out_sample_rate=is->audio_ctx->sample_rate;
            int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
            //Out Buffer Size
            /*
             int out_buffer_size=av_samples_get_buffer_size(NULL,
             out_channels,
             out_nb_samples,
             AV_SAMPLE_FMT_S16,
             1);
             */
            
            //uint8_t *out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
            int64_t in_channel_layout=av_get_default_channel_layout(is->audio_ctx->channels);
            
            struct SwrContext *audio_convert_ctx = swr_alloc_set_opts(NULL,
                                                                      out_channel_layout,
                                                                      AV_SAMPLE_FMT_S16,
                                                                      out_sample_rate,
                                                                      in_channel_layout,
                                                                      is->audio_ctx->sample_fmt,
                                                                      is->audio_ctx->sample_rate,
                                                                      0,
                                                                      NULL);
            fprintf(stderr, "swr opts: out_channel_layout:%lld, out_sample_fmt:%d, out_sample_rate:%d, in_channel_layout:%lld, in_sample_fmt:%d, in_sample_rate:%d",
                    out_channel_layout, AV_SAMPLE_FMT_S16, out_sample_rate, in_channel_layout, is->audio_ctx->sample_fmt, is->audio_ctx->sample_rate);
            
            is->audio_swr_ctx = audio_convert_ctx;
            
            SDL_PauseAudio(0);
            break;
        case AVMEDIA_TYPE_VIDEO:
            is->videoStream = stream_index;
            is->video_st = pFormatCtx->streams[stream_index];
            is->video_ctx = codecCtx;
            
            is->frame_timer = (double)av_gettime() / 1000000.0;
            is->frame_last_delay = 40e-3;
            is->video_current_pts_time = av_gettime();
            
            packet_queue_init(&is->videoq);
            //图像裁剪
            is->video_sws_ctx = sws_getContext(is->video_ctx->width,//原始宽
                                               is->video_ctx->height,//原始高
                                               is->video_ctx->pix_fmt,
                                               is->video_ctx->width,//输出宽
                                               is->video_ctx->height,//输出高
                                               AV_PIX_FMT_YUV420P,
                                               SWS_BILINEAR,
                                               NULL, NULL, NULL
                                               );
            is->video_tid = SDL_CreateThread(decode_video_thread, "decode_video_thread", is);
            break;
        default:
            break;
    }
    return 0;
}

void create_window(int width, int height) {
    //creat window from SDL
    win = SDL_CreateWindow("Media Player",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           width,
                           height,
                           SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);//SDL_WINDOW_RESIZABLE:可大可小
    if(!win) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }
    
    renderer = SDL_CreateRenderer(win, -1, 0);
    
    //IYUV: Y + U + V  (3 planes)
    //YV12: Y + V + U  (3 planes)
    //像素格式：YUV的数据
    Uint32 pixformat= SDL_PIXELFORMAT_IYUV;
    
    //create texture for render
    texture = SDL_CreateTexture(renderer,
                                pixformat,
                                SDL_TEXTUREACCESS_STREAMING,
                                width,
                                height);
}

int demux_thread(void *arg) {
    
    int err_code;
    char errors[1024] = {0,};
    
    VideoState *is = (VideoState *)arg;
    AVFormatContext *pFormatCtx;
    AVPacket pkt1, *packet = &pkt1;
    
    int video_index = -1;
    int audio_index = -1;
    int i;
    
    is->videoStream=-1;
    is->audioStream=-1;
    
    global_video_state = is;
    
    /* open input file, and allocate format context */
    if ((err_code=avformat_open_input(&pFormatCtx, is->filename, NULL, NULL)) < 0) {
        av_strerror(err_code, errors, 1024);
        fprintf(stderr, "Could not open source file %s, %d(%s)\n", is->filename, err_code, errors);
        return -1;
    }
    
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
    
    // main decode loop
    
    for(;;) {
        if(is->quit) {
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
            packet_queue_put(&is->videoq, packet);
        } else if(packet->stream_index == is->audioStream) {
            packet_queue_put(&is->audioq, packet);
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
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    return 0;
}

int media_player(char *url){
    
    
    SDL_Event       event;
    
    VideoState      *is;
    
    is = av_mallocz(sizeof(VideoState));
    
    if(!url) {
        fprintf(stderr, "Usage: test <file>\n");
        exit(1);
    }
    
    // Register all formats and codecs
    av_register_all();
    
    //视频 | 音频 | timer
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }
    create_window(640, 360);
    
    text_mutex = SDL_CreateMutex();
    
    av_strlcpy(is->filename, url, sizeof(is->filename));
    
    is->pictq_mutex = SDL_CreateMutex();
    is->pictq_cond = SDL_CreateCond();
    
    schedule_refresh(is, 40);
    
    is->av_sync_type = DEFAULT_AV_SYNC_TYPE;
    is->parse_tid = SDL_CreateThread(demux_thread,"demux_thread", is);
    if(!is->parse_tid) {
        av_free(is);
        return -1;
    }
    for(;;) {
        
        SDL_WaitEvent(&event);
        switch(event.type) {
            case FF_QUIT_EVENT:
            case SDL_QUIT:
                is->quit = 1;
                SDL_Quit();
                return 0;
                break;
            case FF_REFRESH_EVENT:
                video_refresh_timer(event.user.data1);
                break;
            default:
                break;
        }
    }
    
    return 0;
}
