//
//  KSPlayerView.h
//  KSYuvPlayer
//
//  Created by saeipi on 2020/7/1.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface KSPlayerView : UIView

- (void)initKit;
- (void)displayYUVI420Data:(void *)data width:(GLuint)width height:(GLuint)height;

@end

