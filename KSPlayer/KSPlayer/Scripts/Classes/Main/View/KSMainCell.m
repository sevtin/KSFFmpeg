//
//  KSMainCell.m
//  KSPlayer
//
//  Created by Athena on 2020/5/5.
//  Copyright © 2020 saeipi. All rights reserved.
//

#import "KSMainCell.h"

@implementation KSMainCell

- (void)awakeFromNib {
    [super awakeFromNib];
    // Initialization code
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated {
    [super setSelected:selected animated:animated];

    // Configure the view for the selected state
}

static NSString *mainCellIdentifier = @"mainCellIdentifier";
+ (instancetype)initWithTableView:(UITableView *)tableView {
    KSMainCell *cell=[tableView dequeueReusableCellWithIdentifier:mainCellIdentifier];
    if (cell==nil) {
        cell = [[self alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:mainCellIdentifier];
    }
    return  cell;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    if (self = [super initWithStyle:style reuseIdentifier:reuseIdentifier]) {
        [self initializeKit];
    }
    return self;
}

- (void)initializeKit {
    
}

@end
