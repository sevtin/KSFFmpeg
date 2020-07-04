//
//  KSAudioPlayerController.m
//  KSPlayer
//
//  Created by Athena on 2020/5/5.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "KSAudioPlayerController.h"
#import "XDXAVParseHandler.h"
#import "XDXFFmpegAudioDecoder.h"
#import "XDXAduioDecoder.h"
#import "XDXAudioQueuePlayer.h"
#import <AVFoundation/AVFoundation.h>
#import "XDXQueueProcess.h"

@interface KSAudioPlayerController ()<XDXFFmpegAudioDecoderDelegate>

@property (nonatomic, assign) BOOL isUseFFmpeg;

@end

@implementation KSAudioPlayerController

- (void)viewDidLoad {
    [super viewDidLoad];
    
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
    
    // Set it to select decode method
    self.isUseFFmpeg = YES;
    /*
    // Configure Audio Queue Player
    [[XDXAudioQueuePlayer getInstance] configureAudioPlayerWithAudioFormat:self.isUseFFmpeg ? &ffmpegAudioFormat : &systemAudioFormat bufferSize:kXDXBufferSize];
    [[XDXAudioQueuePlayer getInstance] startAudioPlayer];
    [self startDecodeByFFmpeg];
     */
}

- (IBAction)startParseDidClicked:(id)sender {
    if (self.isUseFFmpeg) {
        [self startDecodeByFFmpeg];
    }else {
        [self startDecodeByAudioConverter];
    }
}

- (void)startDecodeByFFmpeg {
    NSString *path = @"rtmp://202.69.69.180:443/webcast/bshdlive-pc";//[[NSBundle mainBundle] pathForResource:@"test" ofType:@"MOV"];
    XDXAVParseHandler *parseHandler = [[XDXAVParseHandler alloc] initWithPath:path];
    XDXFFmpegAudioDecoder *decoder = [[XDXFFmpegAudioDecoder alloc] initWithFormatContext:[parseHandler getFormatContext] audioStreamIndex:[parseHandler getAudioStreamIndex]];
    decoder.delegate = self;
    [parseHandler startParseGetAVPackeWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, AVPacket packet) {
        if (isFinish) {
            [decoder stopDecoder];
            return;
        }
        
        if (!isVideoFrame) {
            [decoder startDecodeAudioDataWithAVPacket:packet];
        }
    }];
}

- (void)startDecodeByAudioConverter {
    NSString *path = [[NSBundle mainBundle] pathForResource:@"test" ofType:@"MOV"];
    XDXAVParseHandler *parseHandler = [[XDXAVParseHandler alloc] initWithPath:path];
    
    // Origin file aac format
    AudioStreamBasicDescription audioFormat = {
        .mSampleRate         = 48000,
        .mFormatID           = kAudioFormatMPEG4AAC,
        .mChannelsPerFrame   = 2,
        .mFramesPerPacket    = 1024,
    };
    XDXAduioDecoder *audioDecoder = [[XDXAduioDecoder alloc] initWithSourceFormat:audioFormat
                                                                     destFormatID:kAudioFormatLinearPCM
                                                                       sampleRate:48000
                                                              isUseHardwareDecode:YES];
    
    
    [parseHandler startParseWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, struct XDXParseVideoDataInfo * _Nonnull videoInfo, struct XDXParseAudioDataInfo * _Nonnull audioInfo) {
        if (isFinish) {
            [audioDecoder freeDecoder];
            return;
        }
        
        if (!isVideoFrame) {
            [audioDecoder decodeAudioWithSourceBuffer:audioInfo->data
                                     sourceBufferSize:audioInfo->dataSize
                                      completeHandler:^(AudioBufferList * _Nonnull destBufferList, UInt32 outputPackets, AudioStreamPacketDescription * _Nonnull outputPacketDescriptions) {
                // Put audio data from audio file into audio data queue
                [self addBufferToWorkQueueWithAudioData:destBufferList->mBuffers->mData size:destBufferList->mBuffers->mDataByteSize];
                
                // control rate
                usleep(16.8*1000);
            }];
        }
    }];
}

#pragma mark - Decode Callback
- (void)getDecodeAudioDataByFFmpeg:(void *)data size:(int)size pts:(int64_t)pts isFirstFrame:(BOOL)isFirstFrame {
    // Put audio data from audio file into audio data queue
    [self addBufferToWorkQueueWithAudioData:data size:size];
    
    // control rate
    usleep(16.8*1000);
}


- (void)addBufferToWorkQueueWithAudioData:(void *)data  size:(int)size {
    XDXCustomQueueProcess *audioBufferQueue =  [XDXAudioQueuePlayer getInstance]->_audioBufferQueue;
    
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
