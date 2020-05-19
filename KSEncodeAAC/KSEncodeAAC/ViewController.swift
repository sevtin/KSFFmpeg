//
//  ViewController.swift
//  KSAudioResample
//
//  Created by saeipi on 2020/5/18.
//  Copyright © 2020 saeipi. All rights reserved.
//

import Cocoa

class ViewController: NSViewController {

    var recStatus: Bool = false;
    var thread: Thread?
    var btnAction: NSButton?
    
    override func viewDidLoad() {
        super.viewDidLoad()

        self.view.setFrameSize(NSSize(width: 320, height: 240))
        btnAction = NSButton.init(title: "开始录制", target: self, action: #selector(buttonClick))
        btnAction?.bezelStyle = .rounded
        btnAction?.setButtonType(.pushOnPushOff)
        btnAction?.frame = NSRect(x: 320/2-50, y: 240/2-15, width: 100, height: 30)
        self.view.addSubview(btnAction!)
    }

    override var representedObject: Any? {
        didSet {
        // Update the view, if already loaded.
        }
    }
    
    @objc func buttonClick() {
        self.recStatus = !self.recStatus
        if self.recStatus {
            thread = Thread.init(target: self, selector: #selector(recordAudio), object: nil);
            thread?.start()
            btnAction?.title = "停止录制"
        }
        else{
            update_status(0)
            btnAction?.title = "开始录制"
        }
    }
    
    @objc func recordAudio() {
        record_audio()
    }

}

