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
    
    char *url = "/Users/saeipi/Downloads/File/SOPSandwich.mp4";
    media_player(url);
    
}


- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];

    // Update the view, if already loaded.
}


@end
