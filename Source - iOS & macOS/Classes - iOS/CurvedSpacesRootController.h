//	CurvedSpacesRootController.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "GeometryGamesGraphicsViewController.h"
#import "CurvedSpacesSpaceChoiceController.h"
#import "CurvedSpacesOptionsChoiceController.h"
#import "CurvedSpacesGraphicsViewiOS.h"
#import "CurvedSpacesHelpChoiceController.h"
#import <CoreMotion/CMMotionManager.h>


@interface CurvedSpacesRootController : GeometryGamesGraphicsViewController
	<CurvedSpacesSpaceChoiceDelegate, CurvedSpacesOptionsChoiceDelegate,
	CurvedSpacesGraphicsViewDelegate, CurvedSpacesHelpChoiceDelegate>

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil motionManager:(CMMotionManager *)aMotionManager;
- (BOOL)prefersStatusBarHidden;
- (void)viewDidLoad;

- (void)viewSafeAreaInsetsDidChange;
- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection;

- (void)simulateHelpButtonTap;

- (void)animationTimerFired:(CADisplayLink *)sender;

//	CurvedSpacesSpaceChoiceDelegate
- (void)userDidChooseSpace:(NSString *)aGeneratorFileRelativePath;
- (void)userDidCancelSpaceChoice;

//	CurvedSpacesOptionsChoiceDelegate
- (void)userDidChooseOptionCenterpiece:(CenterpieceType)aCenterpiece;
- (void)userDidChooseOptionFog:(bool)aFogChoice;
- (void)userDidChooseOptionColorCoding:(bool)aColorCodingChoice;
- (void)userDidChooseOptionVertexFigures:(bool)aVertexFiguresChoice;
- (void)userDidChooseOptionObserver:(bool)anObserverChoice;
- (void)userDidCancelOptionsChoice;

//	CurvedSpacesGraphicsViewDelegate
- (void)refreshSpeedSlider;

//	CurvedSpacesHelpChoiceDelegate
- (void)dismissHelp;

@end
