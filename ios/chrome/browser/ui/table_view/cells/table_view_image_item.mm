// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"

#include "base/i18n/rtl.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewImageItem

@synthesize cellAccessibilityIdentifier = _cellAccessibilityIdentifier;
@synthesize image = _image;
@synthesize title = _title;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewImageCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  TableViewImageCell* cell =
      base::mac::ObjCCastStrict<TableViewImageCell>(tableCell);
  if (self.image) {
    cell.imageView.hidden = NO;
    cell.imageView.image = self.image;
  } else {
    // No image. Hide imageView.
    cell.imageView.hidden = YES;
  }

  cell.titleLabel.text = self.title;
  cell.accessibilityIdentifier = self.cellAccessibilityIdentifier;
  cell.chevronImageView.hidden = self.hideChevron;
  cell.imageView.backgroundColor = styler.tableViewBackgroundColor;
  cell.titleLabel.backgroundColor = styler.tableViewBackgroundColor;
  if (styler.cellTitleColor)
    cell.titleLabel.textColor = styler.cellTitleColor;
}

@end

@implementation TableViewImageCell
@synthesize imageView = _imageView;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _imageView = [[UIImageView alloc] init];
    _titleLabel = [[UILabel alloc] init];

    // The favicon image is smaller than its UIImageView's bounds, so center
    // it.
    _imageView.contentMode = UIViewContentModeCenter;
    [_imageView setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisHorizontal];

    // Set font size using dynamic type.
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    // Disclosure ImageView.
    _chevronImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
    [_chevronImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    // TODO(crbug.com/870841): Use default accessory type.
    if (base::i18n::IsRTL())
      _chevronImageView.transform = CGAffineTransformMakeRotation(M_PI);

    // Horizontal stack view holds imageView, title, and disclosureView.
    UIStackView* horizontalStack =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _imageView, _titleLabel, _chevronImageView
        ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.distribution = UIStackViewDistributionFill;
    horizontalStack.alignment = UIStackViewAlignmentCenter;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:horizontalStack];

    [NSLayoutConstraint activateConstraints:@[
      // Horizontal Stack constraints.
      [horizontalStack.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [horizontalStack.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [horizontalStack.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewVerticalSpacing],
      [horizontalStack.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kTableViewVerticalSpacing],
    ]];
  }
  return self;
}

@end
