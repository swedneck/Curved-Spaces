//	CurvedSpacesSpaceChoiceController.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import <UIKit/UIKit.h>
#import "GeometryGamesPopover.h"

@protocol CurvedSpacesSpaceChoiceDelegate
- (BOOL)prefersStatusBarHidden;
- (void)userDidChooseSpace:(NSString *)aGeneratorFileRelativePath;
- (void)spaceNeedsDodecahedralAlignmentAdjustment;
- (void)userDidCancelSpaceChoice;
@end

@interface CurvedSpacesSpaceChoiceController : UITableViewController
	<UITableViewDataSource, UITableViewDelegate, GeometryGamesPopover>

- (id)initWithDelegate:(id<CurvedSpacesSpaceChoiceDelegate>)aDelegate spaceChoice:(NSString *)aGeneratorFileRelativePath;
- (BOOL)prefersStatusBarHidden;
- (void)viewWillAppear:(BOOL)animated;

//	GeometryGamesPopover
- (void)adaptNavBarForHorizontalSize:(UIUserInterfaceSizeClass)aHorizontalSizeClass;

//	UITableViewDataSource
- (NSInteger)numberOfSectionsInTableView:(UITableView *)aTableView;
- (NSString *)tableView:(UITableView *)aTableView titleForHeaderInSection:(NSInteger)aSection;
- (NSInteger)tableView:(UITableView *)aTableView numberOfRowsInSection:(NSInteger)aSection;
- (UITableViewCell *)tableView:(UITableView *)aTableView cellForRowAtIndexPath:(NSIndexPath *)anIndexPath;

//	UITableViewDelegate
- (NSIndexPath *)tableView:(UITableView *)aTableView willSelectRowAtIndexPath:(NSIndexPath *)anIndexPath;

@end
