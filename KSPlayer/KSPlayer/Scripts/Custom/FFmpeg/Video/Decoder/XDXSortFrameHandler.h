//
//  XDXSortFrameHandler.h
//  XDXVideoDecoder
//
//  Created by 小东邪 on 2019/6/24.
//  Copyright © 2019 小东邪. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "XDXAVParseHandler.h"

@protocol XDXSortFrameHandlerDelegate <NSObject>

@optional

- (void)getSortedVideoNode:(CMSampleBufferRef)videoDataRef;

@end

@interface XDXSortFrameHandler : NSObject

@property (nonatomic, weak) id<XDXSortFrameHandlerDelegate> delegate;

- (void)addDataToLinkList:(CMSampleBufferRef)videoDataRef;
- (void)cleanLinkList;

@end
