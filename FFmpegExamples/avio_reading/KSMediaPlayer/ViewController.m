//
//  ViewController.m
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//  

#import "ViewController.h"
#include "avio_reading.h"
@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    NSButton *playBtn = [NSButton buttonWithTitle:@"播放" target:self action:@selector(mediaPlayer)];
    [self.view addSubview:playBtn];
    playBtn.frame = CGRectMake(self.view.frame.size.width / 2 - 50, self.view.frame.size.height / 2 - 30, 100, 60);
    
}

- (void)mediaPlayer {
    char *url = "/Users/saeipi/Downloads/File/SOPSandwich.mp4";
    
}

- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
