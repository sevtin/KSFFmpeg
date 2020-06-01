//
//  ViewController.m
//  KSMediaPlayer
//
//  Created by saeipi on 2020/6/1.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "ViewController.h"
#include "ks_media_player.h"
@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    char *argv[2];
    argv[1] = "/Users/saeipi/Downloads/File/Sandwich.mp4";
    media_player(2, &argv);
    // Do any additional setup after loading the view.
    
}


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
