//
//  ViewController.m
//  KSVideoPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "ViewController.h"
#include "ks_video_player.h"
@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    play_video("/Users/saeipi/Downloads/File/Sandwich.mp4");
    // Do any additional setup after loading the view.
}


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
