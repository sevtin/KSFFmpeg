//
//  KSVideoPlayerController.m
//  KSPlayer
//
//  Created by Athena on 2020/5/5.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "KSVideoPlayerController.h"
#import "XDXAVParseHandler.h"
#import "XDXPreviewView.h"
#import "XDXVideoDecoder.h"
#import "XDXFFmpegVideoDecoder.h"
#import "XDXSortFrameHandler.h"

@interface KSVideoPlayerController ()<XDXVideoDecoderDelegate,XDXFFmpegVideoDecoderDelegate, XDXSortFrameHandlerDelegate>

@property (nonatomic,weak) XDXPreviewView          *previewView;
@property (nonatomic, assign) BOOL                    isH265File;
@property (strong, nonatomic) XDXSortFrameHandler     *sortHandler;

@end

@implementation KSVideoPlayerController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    [self initializeKit];
    [self initializeArgs];
    
    [self startDecodeByFFmpegWithIsH265Data:self.isH265File];
}

- (void)initializeKit {
    XDXPreviewView *previewView = [[XDXPreviewView alloc] initWithFrame:self.view.frame];
    [self.view addSubview:previewView];
    _previewView = previewView;
}

- (void)initializeArgs {
    self.isH265File = NO;
    self.sortHandler = [[XDXSortFrameHandler alloc] init];
    self.sortHandler.delegate = self;
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
    NSString *path = @"rtmp://202.69.69.180:443/webcast/bshdlive-pc";//[[NSBundle mainBundle] pathForResource:isH265 ? @"testh265" : @"testh264" ofType:@"MOV"];
    XDXAVParseHandler *parseHandler = [[XDXAVParseHandler alloc] initWithPath:path];
    XDXFFmpegVideoDecoder *decoder = [[XDXFFmpegVideoDecoder alloc] initWithFormatContext:[parseHandler getFormatContext] videoStreamIndex:[parseHandler getVideoStreamIndex]];
    decoder.delegate = self;
    [parseHandler startParseGetAVPackeWithCompletionHandler:^(BOOL isVideoFrame, BOOL isFinish, AVPacket packet) {
        if (isFinish) {
            [decoder stopDecoder];
            return;
        }
        
        if (isVideoFrame) {
            [decoder startDecodeVideoDataWithAVPacket:packet];
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

@end
