//	CurvedSpacesOptionsChoiceController.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import <UIKit/UIKit.h>
#import "GeometryGamesPopover.h"
#import "CurvedSpaces-Common.h"

@protocol CurvedSpacesOptionsChoiceDelegate
- (BOOL)prefersStatusBarHidden;
- (void)userDidChooseOptionCenterpiece:(CenterpieceType)aCenterpiece;
- (void)userDidChooseOptionFog:(bool)aFogChoice;
- (void)userDidChooseOptionColorCoding:(bool)aColorCodingChoice;
- (void)userDidChooseOptionVertexFigures:(bool)aVertexFiguresChoice;
- (void)userDidChooseOptionObserver:(bool)anObserverChoice;
- (void)userDidCancelOptionsChoice;
@end

@interface CurvedSpacesOptionsChoiceController : UITableViewController
	<UITableViewDataSource, UITableViewDelegate, GeometryGamesPopover>

- (id)initWithDelegate:(id<CurvedSpacesOptionsChoiceDelegate>)aDelegate 
	centerpiece:(CenterpieceType)aCenterpiece fog:(bool)aShowFog
	colorCoding:(bool)aShowColorCoding vertexFigures:(bool)aShowVertexFigures observer:(bool)aShowObserver;
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
