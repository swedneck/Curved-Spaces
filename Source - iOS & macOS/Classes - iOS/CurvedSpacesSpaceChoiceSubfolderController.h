//	CurvedSpacesSpaceChoiceSubfolderController.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesSpaceChoiceController.h"
#import "GeometryGamesPopover.h"
#import "GeometryGamesUtilities-Common.h"	//	for Char16


@interface CurvedSpacesSpaceChoiceSubfolderController : UITableViewController
	<UITableViewDataSource, UITableViewDelegate, GeometryGamesPopover>

- (id)initWithDelegate:(id<CurvedSpacesSpaceChoiceDelegate>)aDelegate
		subfolder:(const Char16 *)aSubfolderName spaceChoice:(NSString *)aGeneratorFileRelativePath;
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
