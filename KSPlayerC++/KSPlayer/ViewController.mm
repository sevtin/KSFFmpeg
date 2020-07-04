//
//  ViewController.m
//  KSPlayer
//
//  Created by saeipi on 2020/7/2.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "ViewController.h"
#include "KSDemux.h"
#include "KSDemuxThread.h"
#include "KSAudioThread.h"
#include "KSVideoThread.h"
#import "KSProtocol.h"
#import "KSPlayerView.h"

#include "libavcodec/avcodec.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    char *url = "http://www.w3school.com.cn/i/movie.mp4";
    
    KSDemux *demux = new KSDemux();
    demux->read();
    demux->clear();
    demux->close();
    demux->open(url);
    
    //KSDemuxThread *demuxThread = new KSDemuxThread();
    KSAudioThread *audioThread = new KSAudioThread();
    KSVideoThread *videoThread = new KSVideoThread();
    audioThread->open(demux->copyAudioPar(), demux->sample_rate, demux->channels);
    KSVideoWidget *videoWidget = new KSVideoWidget();
    videoThread->open(demux->copyVideoPar(), videoWidget, demux->width, demux->height);
    
    KSPlayerView *playerView = [[KSPlayerView alloc] initWithFrame:CGRectMake(0, 0, 320, 240)];
    playerView.backgroundColor = [UIColor redColor];
    videoWidget->playerView = playerView;
    [self.view addSubview:videoWidget->playerView];
    
}

class KSVideoWidget: public KSVideoProtocol {
public:
    KSPlayerView *playerView;
    int width;
    int height;
    
    void initSize(int width, int height) override {
        width = width;
        height = height;
        NSLog(@"width: %d, height: %d",width,height);
    }
    
    void repaint(AVFrame *frame) override {
        [playerView displayYUVI420Data:frame->data[0] width:width height:height];
        NSLog(@"repaint(AVFrame *frame) override");
    }
};

@end
