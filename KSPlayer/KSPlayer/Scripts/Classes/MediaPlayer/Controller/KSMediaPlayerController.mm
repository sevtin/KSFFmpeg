//
//  KSMediaPlayerController.m
//  KSPlayer
//
//  Created by Athena on 2020/5/5.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "KSMediaPlayerController.h"
#import "XDXAVParseHandler.h"
#import "XDXPreviewView.h"
#import "XDXVideoDecoder.h"
#import "XDXFFmpegVideoDecoder.h"
#import "XDXSortFrameHandler.h"

#import "XDXFFmpegAudioDecoder.h"
#import "XDXAduioDecoder.h"
#import "XDXAudioQueuePlayer.h"
#import <AVFoundation/AVFoundation.h>
#import "XDXQueueProcess.h"

int kXDXBufferSize = 4096;

@interface KSMediaPlayerController ()<XDXVideoDecoderDelegate,XDXFFmpegVideoDecoderDelegate, XDXSortFrameHandlerDelegate,XDXFFmpegAudioDecoderDelegate>

@property (nonatomic,weak   ) XDXPreviewView      *previewView;
@property (nonatomic, assign) BOOL                isH265File;
@property (nonatomic, assign) BOOL                isUseFFmpeg;
@property (strong, nonatomic) XDXSortFrameHandler *sortHandler;

@end

@implementation KSMediaPlayerController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self initializeKit];
    [self initializeArgs];
    
    [self startDecodeByFFmpegWithIsH265Data:self.isH265File];
}

- (void)initializeKit {
    XDXPreviewView *previewView = [[XDXPreviewView alloc] initWithFrame:self.view.frame];
    [self.view addSubview:previewView];
    _previewView                = previewView;
}

- (void)initializeArgs {
    // Set it to select decode method
    self.isUseFFmpeg          = YES;
    
    self.isH265File           = NO;
    self.sortHandler          = [[XDXSortFrameHandler alloc] init];
    self.sortHandler.delegate = self;
}

- (void)initializeAudio {
    // Final Audio Player format : This is only for the FFmpeg to decode.
    AudioStreamBasicDescription ffmpegAudioFormat = {
        .mSampleRate         = 48000,
        .mFormatID           = kAudioFormatLinearPCM,
        .mChannelsPerFrame   = 2,
        .mFormatFlags        = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked,
        .mBitsPerChannel     = 16,
        .mBytesPerPacket     = 4,
        .mBytesPerFrame      = 4,
        .mFramesPerPacket    = 1,
    };
    
    // Final Audio Player format : This is only for audio converter format.
    AudioStreamBasicDescription systemAudioFormat = {
        .mSampleRate         = 48000,
        .mFormatID           = kAudioFormatLinearPCM,
        .mChannelsPerFrame   = 1,
        .mFormatFlags        = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked,
        .mBitsPerChannel     = 16,
        .mBytesPerPacket     = 2,
        .mBytesPerFrame      = 2,
        .mFramesPerPacket    = 1,
    };

    // Configure Audio Queue Player
    [[XDXAudioQueuePlayer getInstance] configureAudioPlayerWithAudioFormat:self.isUseFFmpeg ? &ffmpegAudioFormat : &systemAudioFormat bufferSize:kXDXBufferSize];
    [[XDXAudioQueuePlayer getInstance] startAudioPlayer];
    
}
- (void)startDecodeByVTSessionWithIsH265Data:(BOOL)isH265 {
    NSString *path = [[NSBundle mainBundle] pathForResource:isH265 ? @"testh265" : @"testh264"  ofType:@"MOV"];
    XDXAVParseHandler *parseHandler = [[XDXAVParseHandler alloc] initWithPath:path];
    XDXVideoDecoder *decoder = [[XDXVideoDecoder alloc] init];
    decoder.delegate = self;
    [parseHandler startParseWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, struct XDXParseVideoDataInfo *videoInfo, struct XDXParseAudioDataInfo *audioInfo) {
        if (isFinish) {
            [decoder stopDecoder];
            return;
        }
        
        if (isVideoFrame) {
            [decoder startDecodeVideoData:videoInfo];
        }
    }];
}

- (void)startDecodeByFFmpegWithIsH265Data:(BOOL)isH265 {
    NSString *path                      = @"rtmp://202.69.69.180:443/webcast/bshdlive-pc";
    XDXAVParseHandler *parseHandler     = [[XDXAVParseHandler alloc] initWithPath:path];
    AVFormatContext *formatContext      = [parseHandler getFormatContext];

    XDXFFmpegVideoDecoder *decoder      = [[XDXFFmpegVideoDecoder alloc] initWithFormatContext:formatContext videoStreamIndex:[parseHandler getVideoStreamIndex]];
    decoder.delegate                    = self;
    /*
    XDXFFmpegAudioDecoder *audioDecoder = [[XDXFFmpegAudioDecoder alloc] initWithFormatContext:formatContext audioStreamIndex:[parseHandler getAudioStreamIndex]];
    audioDecoder.delegate               = self;
    */
    [parseHandler startParseGetAVPackeWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, AVPacket packet) {
        if (isFinish) {
            [decoder stopDecoder];
            //[audioDecoder stopDecoder];
            return;
        }
        
        if (isVideoFrame) {
            [decoder startDecodeVideoDataWithAVPacket:packet];
            //[audioDecoder startDecodeAudioDataWithAVPacket:packet];
        }
    }];
    
}

#pragma mark - Decode Callback
- (void)getVideoDecodeDataCallback:(CMSampleBufferRef)sampleBuffer isFirstFrame:(BOOL)isFirstFrame {
    if (self.isH265File) {
        // Note : the first frame not need to sort.
        if (isFirstFrame) {
            [self.sortHandler cleanLinkList];
            CVPixelBufferRef pix = CMSampleBufferGetImageBuffer(sampleBuffer);
            [self.previewView displayPixelBuffer:pix];
            return;
        }
        
        [self.sortHandler addDataToLinkList:sampleBuffer];
    }else {
        CVPixelBufferRef pix = CMSampleBufferGetImageBuffer(sampleBuffer);
        [self.previewView displayPixelBuffer:pix];
    }
}

-(void)getDecodeVideoDataByFFmpeg:(CMSampleBufferRef)sampleBuffer {
    CVPixelBufferRef pix = CMSampleBufferGetImageBuffer(sampleBuffer);
    [self.previewView displayPixelBuffer:pix];
}


#pragma mark - Sort Callback
- (void)getSortedVideoNode:(CMSampleBufferRef)sampleBuffer {
    int64_t pts = (int64_t)(CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) * 1000);
    static int64_t lastpts = 0;
    NSLog(@"Test marigin - %lld",pts - lastpts);
    lastpts = pts;
    
    [self.previewView displayPixelBuffer:CMSampleBufferGetImageBuffer(sampleBuffer)];
}
//--------------------------------------------------------------------
- (void)startDecodeByFFmpeg {
    NSString *path = @"rtmp://202.69.69.180:443/webcast/bshdlive-pc";//[[NSBundle mainBundle] pathForResource:@"test" ofType:@"MOV"];
    XDXAVParseHandler *parseHandler = [[XDXAVParseHandler alloc] initWithPath:path];
    AVFormatContext *formatContext  = [parseHandler getFormatContext];

    XDXFFmpegVideoDecoder *decoder      = [[XDXFFmpegVideoDecoder alloc] initWithFormatContext:formatContext videoStreamIndex:[parseHandler getVideoStreamIndex]];
    decoder.delegate                    = self;
    /*
    XDXFFmpegAudioDecoder *audioDecoder = [[XDXFFmpegAudioDecoder alloc] initWithFormatContext:formatContext audioStreamIndex:[parseHandler getAudioStreamIndex]];
    audioDecoder.delegate = self;
     */
    [parseHandler startParseGetAVPackeWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, AVPacket packet) {
        if (isFinish) {
            //[audioDecoder stopDecoder];
            [decoder stopDecoder];
            return;
        }
        if (!isVideoFrame) {
            //[audioDecoder startDecodeAudioDataWithAVPacket:packet];
            [decoder startDecodeVideoDataWithAVPacket:packet];
        }
    }];
}

#pragma mark - XDXFFmpegAudioDecoderDelegate
- (void)getDecodeAudioDataByFFmpeg:(void *)data size:(int)size pts:(int64_t)pts isFirstFrame:(BOOL)isFirstFrame {
    // Put audio data from audio file into audio data queue
    [self addBufferToWorkQueueWithAudioData:data size:size];
    
    // control rate
    usleep(16.8*1000);
}

- (void)addBufferToWorkQueueWithAudioData:(void *)data size:(int)size {
    XDXCustomQueueProcess *audioBufferQueue = [XDXAudioQueuePlayer getInstance]->_audioBufferQueue;
    XDXCustomQueueNode *node = audioBufferQueue->DeQueue(audioBufferQueue->m_free_queue);
    if (node == NULL) {
        NSLog(@"XDXCustomQueueProcess addBufferToWorkQueueWithSampleBuffer : Data in , the node is NULL !");
        return;
    }
    
    node->size = size;
    memcpy(node->data, data, size);
    audioBufferQueue->EnQueue(audioBufferQueue->m_work_queue, node);
    
    NSLog(@"Test Data in ,  work size = %d, free size = %d !",audioBufferQueue->m_work_queue->size, audioBufferQueue->m_free_queue->size);
}

@end
