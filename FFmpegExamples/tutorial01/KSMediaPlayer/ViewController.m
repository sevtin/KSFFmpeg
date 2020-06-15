//
//  ViewController.m
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//  

#import "ViewController.h"
#include "decode_audio.h"
#include "decode_video.h"
#include "demuxing_decoding.h"
#include "encode_audio.h"
#include "encode_video.h"
#include "extract_mvs.h"
#include "hw_decode.h"
#include "metadata.h"
#include "tutorial01.h"
#include "tutorial02.h"

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    NSButton *playBtn = [NSButton buttonWithTitle:@"执行" target:self action:@selector(mediaPlayer)];
    [self.view addSubview:playBtn];
    playBtn.frame = CGRectMake(self.view.frame.size.width / 2 - 50, self.view.frame.size.height / 2 - 30, 100, 60);
    
}

- (void)mediaPlayer {
    /*
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    char *dst_video_url = "/Users/saeipi/Downloads/VideoFile/dst_video.mp4";
    char *dst_audio_url = "/Users/saeipi/Downloads/VideoFile/dst_audio.mp3";
    
    demuxing_decoding_port(src_url, dst_video_url, dst_audio_url);
     */
    
    /*
    char *dst_url = "/Users/saeipi/Downloads/VideoFile/encode_audio_out.mp3";
    encode_audio_port(dst_url);
    */
    
    /*
    //转出视频需要ffplay播放
    char *dst_url = "/Users/saeipi/Downloads/VideoFile/encode_video_out.mp4";
    encode_video_port(dst_url, "libx264");
     */
    
    /*
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    extract_mvs_port(src_url);
    */
    
    /*
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    char *dst_url = "/Users/saeipi/Downloads/VideoFile/hw_decode_audio_out.mp4";
    
    hw_decode_port(src_url, dst_url, "videotoolbox");
     */
    
    /*
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    metadata_port(src_url);
     */
    
    /*
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    char *dst_url = "/Users/saeipi/Downloads/VideoFile/tutorial_SOPSandwich.mp4";
    tutorial01_port(src_url, dst_url, "videotoolbox");
     */
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    char *dst_url = "/Users/saeipi/Downloads/VideoFile/tutorial_SOPSandwich.mp4";
    tutorial02_port(src_url, dst_url, "videotoolbox");
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
