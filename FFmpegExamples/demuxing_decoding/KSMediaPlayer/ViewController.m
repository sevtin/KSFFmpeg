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
@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    NSButton *playBtn = [NSButton buttonWithTitle:@"播放" target:self action:@selector(mediaPlayer)];
    [self.view addSubview:playBtn];
    playBtn.frame = CGRectMake(self.view.frame.size.width / 2 - 50, self.view.frame.size.height / 2 - 30, 100, 60);
    
}

- (void)mediaPlayer {
    char *src_url = "/Users/saeipi/Downloads/VideoFile/SOPSandwich.mp4";
    char *dst_video_url = "/Users/saeipi/Downloads/VideoFile/dst_video.mp4";
    char *dst_audio_url = "/Users/saeipi/Downloads/VideoFile/dst_audio.mp3";
    
    demuxing_decoding_port(src_url, dst_video_url, dst_audio_url);
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
