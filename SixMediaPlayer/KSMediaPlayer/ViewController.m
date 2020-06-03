//
//  ViewController.m
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//  李超笔记:http://www.imooc.com/article/91381

#import "ViewController.h"
#include "ks_media_player.h"
@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    NSButton *playBtn = [NSButton buttonWithTitle:@"播放" target:self action:@selector(mediaPlayer)];
    [self.view addSubview:playBtn];
    playBtn.frame = CGRectMake(self.view.frame.size.width / 2 - 50, self.view.frame.size.height / 2 - 30, 100, 60);
    
}

- (void)mediaPlayer {
    char *url = "/Users/saeipi/Downloads/File/SOPSandwich.mp4";
    media_player(url);
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
