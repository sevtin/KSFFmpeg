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
    KSVideoCallback *protocol = new KSVideoCallback();
    videoThread->open(demux->copyVideoPar(), protocol, demux->width, demux->height);
    
}

class KSVideoCallback: public KSVideoProtocol {
    void initSize(int width, int height) override {
        NSLog(@"initSize(int width, int height) override");
    }
    
    void repaint(AVFrame *frame) override {
        NSLog(@"repaint(AVFrame *frame) override");
    }
};

@end
