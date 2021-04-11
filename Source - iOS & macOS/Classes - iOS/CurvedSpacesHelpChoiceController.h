//	CurvedSpacesHelpChoiceController.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import <UIKit/UIKit.h>
#import "GeometryGamesPopover.h"


@protocol CurvedSpacesHelpChoiceDelegate
- (BOOL)prefersStatusBarHidden;
- (void)dismissHelp;
@end


@interface CurvedSpacesHelpChoiceController : UITableViewController
	<UITableViewDataSource, UITableViewDelegate, GeometryGamesPopover>

- (id)initWithDelegate:(id<CurvedSpacesHelpChoiceDelegate>)aDelegate;
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
