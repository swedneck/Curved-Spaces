//	CurvedSpacesRootController.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesRootController.h"
#import "CurvedSpacesGraphicsViewiOS.h"
#import "CurvedSpaces-Common.h"	//	for LoadGeneratorFile()
#import "GeometryGamesModel.h"
#import "GeometryGamesUtilities-iOS.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"


#define SPEED_SLIDER_WIDTH	160.0


//	Privately-declared properties and methods
@interface CurvedSpacesRootController()

- (void)loadGraphicsView;
- (void)loadToolbar;

- (void)refreshToolbarContent;
- (void)userTappedToolbarButton:(id)sender;
- (void)userSlidSpeedSlider:(id)sender;

@end


@implementation CurvedSpacesRootController
{
	CMMotionManager				*itsMotionManager;	//	reference to single app-wide CMMotionManager

	CurvedSpacesGraphicsViewiOS	*itsCurvedSpacesGraphicsView;	//	same as itsMainView, but with a more specific type

	UIToolbar					*itsToolbar;

	UISlider					*itsSpeedSlider;
	UIBarButtonItem				*itsSpaceButton,
								*itsOptionsButton,
								*itsFlexibleSpace1,
								*itsSpeedSliderButton,
								*itsFlexibleSpace2,
								*itsExportButton,
								*itsHelpButton;
	
	NSString					*itsGeneratorFileRelativePath;	//	e.g. "Flat/Hantzsche Wendt.gen"
}


- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil motionManager:(CMMotionManager *)aMotionManager
{
	self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
	if (self != nil)
	{
		itsMotionManager				= aMotionManager;
		itsGeneratorFileRelativePath	= nil;	//	redundant, but makes our intentions clear

#ifdef SCREENSHOT_FOR_GEOMETRY_GAMES_CURVED_SPACES_PAGE
		[self openGeneratorFile:@"Basic/Mirrored Dodecahedron.gen"];	//	sets itsGeneratorFileRelativePath
#else
		[self openGeneratorFile:@"Basic/3-Torus.gen"];	//	sets itsGeneratorFileRelativePath
#endif
	}
	return self;
}

- (BOOL)prefersStatusBarHidden
{
#if (defined(MAKE_SCREENSHOTS) || defined(SCREENSHOT_FOR_GEOMETRY_GAMES_CURVED_SPACES_PAGE))
	return YES;
#else
	return NO;
#endif
}


- (void)viewDidLoad
{
	[super viewDidLoad];

	[self loadGraphicsView];
	[self loadToolbar];

#if (defined(MAKE_SCREENSHOTS) || defined(SCREENSHOT_FOR_GEOMETRY_GAMES_CURVED_SPACES_PAGE))
	//	A hack to get iOS to call viewSafeAreaInsetsDidChange
	//	even when the status bar is hidden.  See comment
	//		"Relying on viewSafeAreaInsetsDidChange is risky..."
	//	below.
	[self setAdditionalSafeAreaInsets:(UIEdgeInsets){16.0, 0.0, 0.0, 0.0}];
#endif
}

- (void)loadGraphicsView;
{
	CGRect	theTilingViewFrame;

	//	Letting itsMainView underlap itsToolbar can cause problems
	//	when a popover includes itsMainView as a passthrough view.
	//
	//		Fact #1:  UIKit tests the passthrough views for hits
	//		in the order in which they appear in thePassthroughViews array,
	//		ignoring the views' stacking order in the window itself.
	//
	//		Fact #2:  UIPopoverPresentationController's setBarButtonItem
	//		automatically adds all of a given button's siblings
	//		to the *end* of the list of passthrough views.
	//
	//	As a corollary of those two facts, if itsMainView underlaps the toolbar,
	//	it can steal taps from the other toolbar buttons while the Help popover is up,
	//	because even though itsMainView sits lower than the toolbar in the window's stacking order,
	//	it would come before the toolbar buttons on the list of passthrough views.
	//	If we could insert the various toolbar buttons at the beginning
	//	of the list of passthrough views, that would solve the problem,
	//	but we can't do that because UIBarButtonItem is not a subclass of UIView.
	//	To avoid this mess, don't let itsMainView underlap itsToolbar
	//	when running on iPad.  For simplicity and consistency, don't let it
	//	underlap on iPhone either.

//#warning Relying on viewSafeAreaInsetsDidChange is risky (see details in code).
//	Relying on viewSafeAreaInsetsDidChange works, but it's
//	a brittle solution that could break in the future.
//	(Even now, if we suppress the status bar on a notchless iPhone,
//	viewSafeAreaInsetsDidChange doesn't get called.)
//	Fortunately the SwiftUI version of this app
//	will avoid this problem altogether, which is why
//	I'm not going to fix it in this legacy code.

	theTilingViewFrame = [[self view] bounds];	//	tentative value, viewSafeAreaInsetsDidChange will override

	itsCurvedSpacesGraphicsView = [[CurvedSpacesGraphicsViewiOS alloc]
						initWithModel:	itsModel
						frame:			theTilingViewFrame	//	frame may get overridden in viewSafeAreaInsetsDidChange
						delegate:		self];
	itsMainView = itsCurvedSpacesGraphicsView;
	[itsMainView setUpGraphics];
	[itsMainView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
	
	[[self view] addSubview:itsMainView];
}

- (void)loadToolbar
{
	CGRect		theSliderFrame;
	ModelData	*md	= NULL;
	double		theUserSpeed;

//#warning Relying on viewSafeAreaInsetsDidChange is risky (see details in code).
//	Relying on viewSafeAreaInsetsDidChange works, but it's
//	a brittle solution that could break in the future.
//	(Even now, if we suppress the status bar on a notchless iPhone,
//	viewSafeAreaInsetsDidChange doesn't get called.)
//	Fortunately the SwiftUI version of this app
//	will avoid this problem altogether, which is why
//	I'm not going to fix it in this legacy code.

	//	Set itsToolbar's height here, and then wait to let
	//	viewSafeAreaInsetsDidChange set the rest of its frame.

	itsToolbar = [[UIToolbar alloc] initWithFrame:CGRectMake(0.0, 0.0, 32.0, 0.0)];
	[itsToolbar sizeToFit];	//	Sets itsToolbar's height to the default height for the device.
	[[self view] addSubview:itsToolbar];
	[itsToolbar setTranslucent:NO];	//	There's nothing underneath it.
	[itsToolbar setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin)];

	itsSpaceButton	= [[UIBarButtonItem alloc]
		initWithTitle:	LocalizationNotNeeded(@"dummy")	//	refreshToolbarContent will set title or image, as appropriate
		style:			UIBarButtonItemStylePlain
		target:			self
		action:			@selector(userTappedToolbarButton:)];
	
	itsOptionsButton	= [[UIBarButtonItem alloc]
		initWithImage:	[UIImage imageNamed:@"Toolbar Buttons/Options"]
		style:			UIBarButtonItemStylePlain
		target:			self
		action:			@selector(userTappedToolbarButton:)];

	itsFlexibleSpace1	= [[UIBarButtonItem alloc]
		initWithBarButtonSystemItem:	UIBarButtonSystemItemFlexibleSpace
		target:							self
		action:							@selector(userTappedToolbarButton:)];

	[itsModel lockModelData:&md];
	theUserSpeed = md->itsUserSpeed;
	[itsModel unlockModelData:&md];

	itsSpeedSlider = [[UISlider alloc] init];
	theSliderFrame = [itsSpeedSlider frame];
	theSliderFrame.size.width = SPEED_SLIDER_WIDTH;
	[itsSpeedSlider setFrame:theSliderFrame];
	[itsSpeedSlider setMinimumValue:-MAX_USER_SPEED];
	[itsSpeedSlider setMaximumValue:+MAX_USER_SPEED];
	[itsSpeedSlider setValue:theUserSpeed];
	[itsSpeedSlider addTarget:	self
						action:	@selector(userSlidSpeedSlider:)
			forControlEvents:	UIControlEventValueChanged];

	itsSpeedSliderButton = [[UIBarButtonItem alloc]
		initWithCustomView:itsSpeedSlider];

	itsFlexibleSpace2	= [[UIBarButtonItem alloc]
		initWithBarButtonSystemItem:	UIBarButtonSystemItemFlexibleSpace
		target:							self
		action:							@selector(userTappedToolbarButton:)];

	itsExportButton = [[UIBarButtonItem alloc]
		initWithBarButtonSystemItem:	UIBarButtonSystemItemAction
		target:							self
		action:							@selector(userTappedToolbarButton:)];
	[itsExportButton setStyle:UIBarButtonItemStylePlain];

	itsHelpButton		= [[UIBarButtonItem alloc]
		initWithTitle:	LocalizationNotNeeded(@"dummy")	//	refreshToolbarContent will set title or image, as appropriate
		style:			UIBarButtonItemStylePlain
		target:			self
		action:			@selector(userTappedToolbarButton:)];

	[itsToolbar	setItems:	@[
								itsSpaceButton,
								itsOptionsButton,
								itsFlexibleSpace1,
								itsSpeedSliderButton,
								itsFlexibleSpace2,
								itsExportButton,
								itsHelpButton
							]
				animated:	NO];
	
	[self refreshToolbarContent];
}


#ifdef MAKE_SCREENSHOTS

#pragma mark -
#pragma mark automate screenshots

- (void)viewDidAppear:(BOOL)animated
{
	NSString	*thePreferredLanguage;	//	user's preferred language at the iOS level

	static BOOL	theScreenshotsHaveBeenStarted	= false;
	
	[super viewDidAppear:animated];

	thePreferredLanguage = GetPreferredLanguage();

	//	The first time the view appears (and only the first time!)
	//	start a script running to generate all the various screenshots.
	if ( ! theScreenshotsHaveBeenStarted )
	{
		theScreenshotsHaveBeenStarted = YES;

		dispatch_after(
			dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0 * NSEC_PER_SEC)),
			dispatch_queue_create(
				"org.geometrygames.CrystalFlightScreenshotQueue",
				DISPATCH_QUEUE_SERIAL),
			^{
				NSArray<NSString *>	*theLanguages;
				unsigned int		theLanguageIndex,
									theScreenshotIndex;

				//	The screenshots for Japanese, Simplified Chinese and Traditional Chinese
				//	must get rendered while that language is set not only within Crystal Flight,
				//	but also in the Xcode scheme (which in effect means that Crystal Flight
				//	sees that langauge as the de facto iOS system language), so that the system
				//	will provide the correct font for the Unihan characters.
				//
				//	The screenshots for Arabic (and in principle for Farsi and Hebrew)
				//	can get rendered while the Xcode scheme's language is set to Arabic,
				//	to provide the desired right-to-left layout.
				//
				//	The screenshots for all other languages may get rendered
				//	while the Xcode scheme's language is set to a Western language
				//	(typically I use English).
				//
				if ([thePreferredLanguage isEqualToString:@"ja"])
					theLanguages = @[];	//	= @[@"ja"];	when the Japanese translation gets restored
				else
				if ([thePreferredLanguage isEqualToString:@"zs"])
					theLanguages = @[@"zs"];
				else
				if ([thePreferredLanguage isEqualToString:@"zt"])
					theLanguages = @[@"zt"];
				else
				if ([thePreferredLanguage isEqualToString:@"ar"])
					theLanguages = @[];	//	= @[@"ar", @"fa", @"he"];	if any of those translations were present
				else
					theLanguages = @[@"en", @"es", @"fr", @"pt"];	//	translations that require no special treatment
					//	Known bug with this approach:  Any UIBarButtonItems created
					//	with UIBarButtonSystemItemDone or UIBarButtonSystemItemCancel
					//	will appear in whatever language the app is running in (typically "en")
					//	rather than the language that gets set in the loop below.
					//	The easiest workaround, if we're willing to clutter up the source code,
					//	is to create the UIBarButtonItem using initWithTitle:GetLocalizedTextAsNSString(u"Done")
					//	instead of initWithBarButtonSystemItem:UIBarButtonSystemItemDone.
					//	A more general workaround would be to run the app in each language separately,
					//	just like we're already doing for ar, ja, zh-Hans and zh-Hant,
					//	but running each Western language separately as well.
					//	On the one hand, it would be a bit tedious to manually run them
					//	one language at a time, but on the other hand, when I write
					//	the SwiftUI versions of the Geometry Games apps, this could
					//	keep the new apps' internal structure simple by not having
					//	to bypass SwiftUI's built-in localization mechanism.

				for (theLanguageIndex = 0; theLanguageIndex < [theLanguages count]; theLanguageIndex++)
				{
					NSString	*theTwoLetterLanguageCode;

					theTwoLetterLanguageCode = theLanguages[theLanguageIndex];
					
					//	Use dispatch_sync, not dispatch_async.
					dispatch_sync(dispatch_get_main_queue(),
					^{
						Char16	theTwoLetterLanguageCodeAsCString[3] =
								{
									[theTwoLetterLanguageCode characterAtIndex:0],
									[theTwoLetterLanguageCode characterAtIndex:1],
									0
								};
						
						SetCurrentLanguage(theTwoLetterLanguageCodeAsCString);
						[self refreshToolbarContent];	//	refreshes other button titles
					});

					//	theScreenshotIndex determines the screenshot contents:
					//
					//		1 = mirrored dodecahedron
					//		2 = 3-torus
					//		3 = Poincaré dodecahedral space
					//		4 = sixth-turn space (with Help menu visible on iPad)
					//		5 = Help menu (iPhone only)
					//
					for (theScreenshotIndex = 1; theScreenshotIndex <= 5; theScreenshotIndex++)
					{
						//	On iPhone, the first four screenshots show no text,
						//	so they can be the same for all languages.
						//	Render them only when the language is English.
						if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone
						 && theScreenshotIndex <= 4
						 && ! [theTwoLetterLanguageCode isEqualToString:@"en"])
						{
							continue;
						}

						//	The iPad doesn't need screenshot 5.
						if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad
						 && theScreenshotIndex == 5)
						{
							continue;
						}
						
						//	Use dispatch_sync, not dispatch_async.
						dispatch_sync(dispatch_get_main_queue(),
						^{
							ModelData	*md	= NULL;
							NSString	*theGeneratorFileRelativePath;	//	e.g. "Flat/Sixth Turn.gen"

							switch (theScreenshotIndex)
							{
								case 1:	theGeneratorFileRelativePath = @"Basic/Mirrored Dodecahedron.gen";			break;
								case 2:	theGeneratorFileRelativePath = @"Basic/3-Torus.gen";						break;
								case 3:	theGeneratorFileRelativePath = @"Basic/Poincaré Dodecahedral Space.gen";	break;
								case 4:	theGeneratorFileRelativePath = @"Flat/Sixth Turn.gen";						break;
								
								//	Screenshots 5 gets created only for iPhone (not iPad),
								//	and the space itself is just barely visible.
								case 5:	theGeneratorFileRelativePath = @"Basic/3-Torus.gen";	break;
								
								default:
									GEOMETRY_GAMES_ABORT("Invalid value of theScreenshotIndex");
							}
							[self openGeneratorFile:theGeneratorFileRelativePath];	//	sets itsGeneratorFileRelativePath

							[self->itsModel lockModelData:&md];

							md->itsUserSpeed	= 0.0;
							md->itsShowObserver	= false;
							switch (theScreenshotIndex)
							{
								case 1:	//	mirrored dodecahedron
									md->itsUserBodyPlacement = (Matrix)
									{
										{
											{ 0.85065080835204077,  0.00000000000000000, -0.52573111211913215, -0.00000000000000009},
											{ 0.00000000000000000,  1.00000000000000000,  0.00000000000000000,  0.00000000000000000},
											{ 0.55698163425765956,  0.00000000000000000,  0.90121521533835924, -0.34988198761539352},
											{-0.18394384645949333,  0.00000000000000000, -0.29762739559285312,  1.05944202543494481}
										},
										ImagePositive
									};
									md->itsCenterpieceType	= CenterpieceGalaxy;
									md->itsAperture			= 0.9;
									md->itsFogFlag			= false;	//	tiling looks better without fog
									break;

								case 2:	//	3-torus
									md->itsUserBodyPlacement = (Matrix)
									{
										{
											{ 0.97863045556272443, -0.02636042563220162, -0.20393028074708280,  0.00000000000000000},
											{ 0.09774767128121026,  0.93216717492767154,  0.34858248772200912,  0.00000000000000000},
											{ 0.18090833094193118, -0.36106714880732826,  0.91482385727961402,  0.00000000000000000},
											{-0.20756188500132949,  0.32165787665331436, -0.35090210631137475,  1.00000000000000000}
										},
										ImagePositive
									};
									md->itsCenterpieceType	= CenterpieceEarth;
									md->itsAperture			= 0.98;
									md->itsFogFlag			= true;
									break;

								case 3:	//	Poincaré dodecahedral space
									md->itsUserBodyPlacement = (Matrix)
									{
										{
											{ 0.78077463410848946,  0.28132283226479599, -0.54735296906329145,  0.10794981258375463},
											{-0.15842160036386663,  0.95197646138348546,  0.24499277969647659, -0.09285446357888480},
											{ 0.60246192772046825, -0.11675164490458829,  0.75179225549004514, -0.24128216603097960},
											{ 0.04830313569783324,  0.03110200042382853,  0.27421099492796469,  0.95995197948231181}
										},
										ImagePositive
									};
									md->itsCenterpieceType	= CenterpieceGalaxy;
									md->itsAperture			= 0.83;
									md->itsFogFlag			= true;
									break;

								case 4:	//	sixth-turn space
									md->itsUserBodyPlacement = (Matrix)
									{
										{
											{ 0.99335169543493163,  0.01568347935978420, -0.11404576998608085,  0.00000000000000000},
											{ 0.01834053163385482,  0.95646201881367754,  0.29127998809779532,  0.00000000000000000},
											{ 0.11364873107929717, -0.29143513007534882,  0.94981552465835717,  0.00000000000000000},
											{-0.12205353496786986,  0.18109972353078846,  0.11240261729239021,  1.00000000000000000}
										},
										ImagePositive
									};
									md->itsCenterpieceType	= CenterpieceGyroscope;
									md->itsAperture			= 0.88;
									md->itsFogFlag			= true;
									break;

								case 5:	//	space is barely visible
									MatrixIdentity(&md->itsUserBodyPlacement);
									md->itsCenterpieceType	= CenterpieceNone;
									md->itsAperture			= 0.5;
									md->itsFogFlag			= true;
									break;

								default:
									GEOMETRY_GAMES_ABORT("Invalid value of theScreenshotIndex");
							}
							md->itsChangeCount++;

							[self->itsModel unlockModelData:&md];
							
							//	On the iPad, screenshot 4 shows a popover.
							if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad)
							{
								switch (theScreenshotIndex)
								{
									case 4:
										[self userTappedToolbarButton:self->itsHelpButton];
										break;
								}
							}

							//	We make screenshot 5 only on iPhone, not on iPad.
							switch (theScreenshotIndex)
							{
								case 5:
									[self userTappedToolbarButton:self->itsHelpButton];
									break;
							}
						});
						
						//	Caution:  These sleep calls are a complete hack!
						//	For non-release-quality code, though, they do the job.
						//	Release-quality code would need to devise some sort
						//	of scheme to wait/signal a semaphore.
						if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad)
							[NSThread sleepForTimeInterval:5.0];	//	loading takes longer on iPad simulator
						else
							[NSThread sleepForTimeInterval:3.0];	//	allow plenty of time for the Help page to load

						//	Save a screenshot.
						//	Use dispatch_sync, not dispatch_async.
						dispatch_sync(dispatch_get_main_queue(),
						^{
							const Char16	*theCurrentLanguage;
//							UIView			*theScreenshotView;
							NSString		*theScreenSize;
							UIImage			*theScreenshotImage;
							const Char16	*theLanguageCode;
							
							static Char16	theLanguagesAll[4] = u"all";

							theCurrentLanguage = GetCurrentLanguage();
							NSLog(@"drawing %C%C-%d", theCurrentLanguage[0], theCurrentLanguage[1], theScreenshotIndex);

							switch ((unsigned int)[[UIScreen mainScreen] bounds].size.height)
							{
								//	It's OK to provide screenshots only for
								//	the largest iPhone size with and without a home button, and
								//	the largest iPad size with and without a home button.
							//	case  480:	theScreenSize = @"3-5-inch";		break;
							//	case  568:	theScreenSize = @"4-0-inch";		break;
							//	case  667:	theScreenSize = @"4-7-inch";		break;
								case  736:	theScreenSize = @"phone-HomeBtn";	break;	//	5.5"
								case  896:	theScreenSize = @"phone-HomeInd";	break;	//	6.5"
							//	case 1024:	theScreenSize = @"pad";				break;
								case 1366:	//	12.9", iPad Pro 2nd or 4th generation
									if ([[[UIApplication sharedApplication] keyWindow] safeAreaInsets].bottom > 0.0)
										theScreenSize = @"pad-HomeInd";
									else
										theScreenSize = @"pad-HomeBtn";
									break;
								default:	theScreenSize = @"UNKNOWN-SIZE";	break;
							}
							
//							theScreenshotView = [[UIScreen mainScreen] snapshotViewAfterScreenUpdates:YES];
//							UIGraphicsBeginImageContextWithOptions([theScreenshotView bounds].size, YES, 0.0);
//							[theScreenshotView drawViewHierarchyInRect:[theScreenshotView bounds] afterScreenUpdates:YES];

							UIGraphicsBeginImageContextWithOptions([[[UIApplication sharedApplication] keyWindow] bounds].size, YES, 0.0);
							[[[UIApplication sharedApplication] keyWindow] drawViewHierarchyInRect:[[[UIApplication sharedApplication] keyWindow] bounds] afterScreenUpdates:YES];

							theScreenshotImage = UIGraphicsGetImageFromCurrentImageContext();
							UIGraphicsEndImageContext();

							if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone
							 && theScreenshotIndex <= 4)
							{
								//	As noted earlier, the first three iPhone screenshots
								//	contain no text, so we may render each one only once,
								//	and use it for all the languages.
								theLanguageCode = theLanguagesAll;
							}
							else
							{
								theLanguageCode = GetCurrentLanguage();
							}

							[UIImagePNGRepresentation(theScreenshotImage)
								writeToFile:	GetFullFilePath(
													[NSString stringWithFormat:@"CurvedSpaces-%S-%@-%d",
														theLanguageCode,
														theScreenSize,
														theScreenshotIndex],
													@".png")
								atomically:		YES];
						});

						//	Dismiss the presented view controller, if any.
						//	Without animation, of course.
						//	Use dispatch_sync, not dispatch_async.
						dispatch_sync(dispatch_get_main_queue(),
						^{
							if ([self presentedViewController] != nil)
								[self dismissViewControllerAnimated:NO completion:nil];
						});
						
						//	Caution:  These sleep calls are a complete hack!
						//	For non-release-quality code, though, they do the job.
						//	Release-quality code would need to devise some sort
						//	of scheme to wait/signal a semaphore.
						[NSThread sleepForTimeInterval:1.0];
					}
				}
				
				//	Use dispatch_sync, not dispatch_async.
				dispatch_sync(dispatch_get_main_queue(),
				^{
					ModelData	*md				= NULL;
					
					[self->itsModel lockModelData:&md];
					MatrixIdentity(&md->itsUserBodyPlacement);
					md->itsChangeCount++;
					[self->itsModel unlockModelData:&md];

					NSLog(@"done!");
				});
			});
	}
}

#endif	//	MAKE_SCREENSHOTS


#pragma mark -
#pragma mark size change

- (void)viewSafeAreaInsetsDidChange
{
	CGRect			theRootViewBounds;
	UIEdgeInsets	theSafeAreaInsets;
	CGFloat			theToolbarHeight,
					theToolbarOffset;	//	distance from top of toolbar to bottom of view
	CGRect			theTilingViewFrame,
					theToolbarFrame;
	
	[super viewSafeAreaInsetsDidChange];

	theRootViewBounds	= [[self view] bounds];
	theSafeAreaInsets	= [[self view] safeAreaInsets];
	theToolbarHeight	= [itsToolbar frame].size.height;
	theToolbarOffset	= theToolbarHeight + theSafeAreaInsets.bottom;	//	allow for home indicator

	//	itsMainView
	theTilingViewFrame				= theRootViewBounds;
	theTilingViewFrame.size.height	-= theToolbarOffset;
	[itsMainView setFrame:theTilingViewFrame];

	//	itsToolbar
	theToolbarFrame.origin.x	= theRootViewBounds.origin.x;
	theToolbarFrame.origin.y	= theRootViewBounds.size.height - theToolbarOffset;
	theToolbarFrame.size.width	= theRootViewBounds.size.width;
	theToolbarFrame.size.height	= theToolbarHeight;
	[itsToolbar setFrame:theToolbarFrame];
}

- (void)traitCollectionDidChange:(UITraitCollection *)previousTraitCollection
{
	//	Let the superclass take care of any presented views.
	[super traitCollectionDidChange:previousTraitCollection];
	
	//	Replace toolbar text buttons with image buttons, or vice versa, as needed.
	[self refreshToolbarContent];
}



#pragma mark -
#pragma mark toolbar

- (void)refreshToolbarContent
{
	//	A text label is clearer than an icon for the Help button,
	//	but in a horizontally compact layout there might not be enough space for it.
	
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
	{
		//	Use icons for the Space and Help buttons.

		[itsSpaceButton	setImage:[UIImage imageNamed:@"Toolbar Buttons/Space"]	];
		[itsSpaceButton	setTitle:nil											];

		[itsHelpButton	setImage:[UIImage imageNamed:@"Toolbar Buttons/Help"]	];
		[itsHelpButton	setTitle:nil											];
	}
	else	//	horizontal size class is "regular"
	{
		//	Use text labels for the Space and Help buttons.

		[itsSpaceButton	setTitle:GetLocalizedTextAsNSString(u"Space")	];
		[itsSpaceButton	setImage:nil									];

		[itsHelpButton	setTitle:GetLocalizedTextAsNSString(u"Help")	];
		[itsHelpButton	setImage:nil									];
	}
}

- (void)userTappedToolbarButton:(id)sender
{
	ModelData				*md	= NULL;
	CenterpieceType			theCenterpiece;
	bool					theShowFog,
							theShowColorCoding,
							theShowVertexFigures,
							theShowObserver;
	UIAlertController		*theAlertController;
	UIViewController		*theContentViewController;
	UINavigationController	*theNavigationController;
	NSArray<UIView *>		*thePassthroughViews;

	[itsModel lockModelData:&md];
	theCenterpiece			= md->itsCenterpieceType;
	theShowFog				= md->itsFogFlag;
	theShowColorCoding		= md->itsShowColorCoding;
	theShowVertexFigures	= md->itsShowVertexFigures;
	theShowObserver			= md->itsShowObserver;
	[itsModel unlockModelData:&md];

	//	On iOS 8 and later, the UIPopoverPresentationController's setBarButtonItem
	//	automatically adds all of a given button's siblings to the list
	//	of passthrough views, but does not add the given button itself.
	//	So a second tap on the same button will automatically dismiss the popover,
	//	without generating a call to this userTappedToolbarButton method.
	//
	//	Thus when we do get a call to userTappedToolbarButton, we know that
	//	if a popover is already up, it's not this one.  So we may safely dismiss
	//	the already-up popover (if present) and display the newly requested one,
	//	with no risk that it might be the same popover.

	[self dismissViewControllerAnimated:YES completion:nil];

	//	Treat "action sheet" style views as a special case.
	if (sender == itsExportButton)
	{
		GeometryGamesGraphicsViewiOS	*theMainView;
		
		//	Create a local reference to itsMainView
		//	to avoid referring to "self" in the completion handler blocks.
		theMainView	= itsMainView;

		theAlertController = [UIAlertController
								alertControllerWithTitle:	nil
								message:					nil
								preferredStyle:				UIAlertControllerStyleActionSheet];

		[theAlertController addAction:[UIAlertAction
			actionWithTitle:	GetLocalizedTextAsNSString(u"Cancel")
			style:				UIAlertActionStyleCancel
			handler:^(UIAlertAction *action)
			{
				UNUSED_PARAMETER(action);
			}]];

		[theAlertController addAction:[UIAlertAction
			actionWithTitle:	GetLocalizedTextAsNSString(u"Save Image")
			style:				UIAlertActionStyleDefault
			handler:^(UIAlertAction *action)
			{
				UNUSED_PARAMETER(action);

				[theMainView saveImageWithAlphaChannel:false];
			}]];

		[theAlertController addAction:[UIAlertAction
			actionWithTitle:	GetLocalizedTextAsNSString(u"Copy Image")
			style:				UIAlertActionStyleDefault
			handler:^(UIAlertAction *action)
			{
				UNUSED_PARAMETER(action);

				[theMainView copyImageWithAlphaChannel:false];
			}]];

		PresentPopoverFromToolbarButton(self, theAlertController, sender, nil);
	}
	else
	{
		//	Create a view controller whose content corresponds to the button the user touched.
		//
		if (sender == itsSpaceButton)
		{
			theContentViewController = [[CurvedSpacesSpaceChoiceController alloc]
											initWithDelegate:	self
											spaceChoice:		itsGeneratorFileRelativePath];
		}
		else
		if (sender == itsOptionsButton)
		{
			theContentViewController = [[CurvedSpacesOptionsChoiceController alloc]
											initWithDelegate:	self
											centerpiece:		theCenterpiece
											fog:				theShowFog
											colorCoding:		theShowColorCoding
											vertexFigures:		theShowVertexFigures
											observer:			theShowObserver];
		}
		else
		if (sender == itsHelpButton)
		{
			theContentViewController = [[CurvedSpacesHelpChoiceController alloc]
											initWithDelegate:	self];
		}
		else
		{
			theContentViewController = nil;
		}
		
		if (theContentViewController != nil)
		{
			//	Wrap theContentViewController in a UINavigationController
			//	(if only to get a title bar that doesn't scroll with the content view).

			theNavigationController = [[UINavigationController alloc]
				initWithRootViewController:theContentViewController];
			[[theNavigationController navigationBar] setTranslucent:NO];	//	so content won't underlap nav bar

			//	Let the user interact with the main view even while a popover is up.
			thePassthroughViews = @[itsMainView];

			PresentPopoverFromToolbarButton(self, theNavigationController, sender, thePassthroughViews);
		}
	}
}

- (void)userSlidSpeedSlider:(id)sender
{
	double		theNewUserSpeed;
	ModelData	*md	= NULL;

	if (sender == itsSpeedSlider)	//	should never fail
	{
		theNewUserSpeed = [itsSpeedSlider value];

		[itsModel lockModelData:&md];
		md->itsUserSpeed = theNewUserSpeed;
		[itsModel unlockModelData:&md];
	}
}

- (void)simulateHelpButtonTap
{
	[self userTappedToolbarButton:itsHelpButton];
}


#pragma mark -
#pragma mark device motion

- (void)animationTimerFired:(CADisplayLink *)sender
{
	CMAttitude	*theDeviceAttitude;
	
	//	At launch, the Motion Manager will typically not yet
	//	have any attitude data available, and -deviceMotion
	//	will return nil.  After a short while, the attitude
	//	data will become available, with no action required
	//	on our part.
	theDeviceAttitude = [[itsMotionManager deviceMotion] attitude];
	if (theDeviceAttitude != nil)
		[itsCurvedSpacesGraphicsView handleDeviceMotion:[theDeviceAttitude rotationMatrix]];
	
	//	Call the superclass's method.
	[super animationTimerFired:sender];
}


#pragma mark -
#pragma mark CurvedSpacesSpaceChoiceDelegate

- (void)userDidChooseSpace:(NSString *)aGeneratorFileRelativePath	//	e.g. "Flat/Hantzsche Wendt.gen"
{
	//	See comment in userDidChooseOptionCenterpiece below.
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
		[self dismissViewControllerAnimated:YES completion:nil];
	
	[self openGeneratorFile:aGeneratorFileRelativePath];
}

- (void)openGeneratorFile:(NSString *)aGeneratorFileRelativePath	//	e.g. "Flat/Hantzsche Wendt.gen"
{
	ErrorText		theErrorText1,
					theErrorText2;
	NSUInteger		thePathNameLength;
	Char16			*theGeneratorFileRelativePath;
	ModelData		*md									= NULL;
	unsigned int	theFileSize							= 0;
	Byte			*theFileContents					= NULL,
					*theFileContentsWithTerminatingZero	= NULL;
	double			theUserSpeed;
	
	itsGeneratorFileRelativePath = aGeneratorFileRelativePath;
	
	thePathNameLength				= [aGeneratorFileRelativePath length];	//	returns 0 if aGeneratorFileRelativePath == nil
	theGeneratorFileRelativePath	= (Char16 *) GET_MEMORY( (thePathNameLength + 1) * sizeof(Char16) );
	[aGeneratorFileRelativePath getCharacters:theGeneratorFileRelativePath range:NSMakeRange(0, thePathNameLength)];
	theGeneratorFileRelativePath[thePathNameLength] = 0;	//	terminating zero

	if (theGeneratorFileRelativePath != NULL)	//	should never fail
	{
		theErrorText1 = GetFileContents(u"Sample Spaces", theGeneratorFileRelativePath, &theFileSize, &theFileContents);
		if (theErrorText1 == NULL)
		{
			theFileContentsWithTerminatingZero = (Byte *) GET_MEMORY( (theFileSize + 1) * sizeof(Byte) );
			memcpy(theFileContentsWithTerminatingZero, theFileContents, theFileSize);
			theFileContentsWithTerminatingZero[theFileSize] = 0;

			[itsModel lockModelData:&md];

			//	Caution:  Depending on why LoadGeneratorFile() fails,
			//	it may or may not free the honeycomb and Dirichlet domain.
			theErrorText2 = LoadGeneratorFile(md, theFileContentsWithTerminatingZero);
			GEOMETRY_GAMES_ASSERT(theErrorText2 == NULL, "failed to load generators (see theErrorText2 for more info)");

			[itsModel unlockModelData:&md];

			FREE_MEMORY_SAFELY(theFileContentsWithTerminatingZero);
		}
		FreeFileContents(&theFileSize, &theFileContents);
	}

	FREE_MEMORY_SAFELY(theGeneratorFileRelativePath);

	[itsModel lockModelData:&md];
	theUserSpeed = md->itsUserSpeed;
	[itsModel unlockModelData:&md];
	[itsSpeedSlider setValue:theUserSpeed];	//	Safe even if itsSpeedSlider = nil, as happens at launch
}

- (void)spaceNeedsDodecahedralAlignmentAdjustment
{
	ModelData	*md	= NULL;

	//	On the one hand, a dodecahedron aligns most naturally
	//	with a rectangular coordinate system when the coordinate system's
	//	x-, y- and z-axes run through the midpoints
	//	of three of the dodecahedron's edges, and Curved Spaces
	//	uses generating matrices computed in such a coordinate system.
	//
	//	On the other hand, users will prefer to find themselves,
	//	by default, looking towards a face of the dodecahedron,
	//	not an edge.
	//
	//	So let's rotate the default user placement by the angle needed
	//	to put a face center directly in front of the user.

	//	What angle should we rotate by?  Well...
	//
	//	If you draw a dodecahedral tiling of a unit 2-sphere,
	//	and draw a small spherical triangle with its vertices at
	//
	//		a face center,
	//		an adjacent edge center, and
	//		an adjacent vertex,
	//
	//	you'll see that it's a (π/2, π/3, π/5) spherical triangle.
	//	You can then apply the spherical law of cosines for angles
	//
	//		cos γ = -cos α cos β + sin α sin β cos c
	//
	//	to solve for the length of the edge that connects
	//	the face center to the edge center, which is exactly
	//	the angle we want to rotate by.
	//
	double		cosC			= 0.85065080835203993218,	//	= cos(π/3) / sin(π/5)
				sinC			= 0.52573111211913360603;	//	= √(1 - cos²C)
	Matrix		theAdjustment	=
				{
					{
						{ cosC,  0.0, -sinC,  0.0 },
						{ 0.0,   1.0,  0.0,   0.0 },
						{+sinC,  0.0,  cosC,  0.0 },
						{ 0.0,   0.0,  0.0,   1.0 }
					},
					ImagePositive
				};

	[itsModel lockModelData:&md];
	MatrixProduct(&theAdjustment, &md->itsUserBodyPlacement, &md->itsUserBodyPlacement);
	md->itsChangeCount++;
	[itsModel unlockModelData:&md];
}

- (void)userDidCancelSpaceChoice
{
	[self dismissViewControllerAnimated:YES completion:nil];
}


#pragma mark -
#pragma mark CurvedSpacesOptionsChoiceDelegate

- (void)userDidChooseOptionCenterpiece:(CenterpieceType)aCenterpiece
{
	ModelData	*md	= NULL;

	[itsModel lockModelData:&md];
	SetCenterpiece(md, aCenterpiece);
	[itsModel unlockModelData:&md];

	//	On an iPad we can leave the Options Choice panel up while the user explores
	//	the various possibilities.  But on an iPhone the Options Choice panel hides
	//	the main view, so we should dismiss the Options Choice panel
	//	whenever the user makes a choice.
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
		[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)userDidChooseOptionFog:(bool)aFogChoice
{
	ModelData	*md	= NULL;

	[itsModel lockModelData:&md];
	SetFogFlag(md, aFogChoice);
	[itsModel unlockModelData:&md];

	//	See comment in userDidChooseOptionCenterpiece above.
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
		[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)userDidChooseOptionColorCoding:(bool)aColorCodingChoice
{
	ModelData	*md	= NULL;

	[itsModel lockModelData:&md];
	SetShowColorCoding(md, aColorCodingChoice);
	[itsModel unlockModelData:&md];

	//	See comment in userDidChooseOptionCenterpiece above.
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
		[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)userDidChooseOptionVertexFigures:(bool)aVertexFiguresChoice
{
	ModelData	*md	= NULL;

	[itsModel lockModelData:&md];
	SetShowVertexFigures(md, aVertexFiguresChoice);
	[itsModel unlockModelData:&md];

	//	See comment in userDidChooseOptionCenterpiece above.
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
		[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)userDidChooseOptionObserver:(bool)anObserverChoice
{
	ModelData	*md	= NULL;

	[itsModel lockModelData:&md];
	SetShowObserver(md, anObserverChoice);
	[itsModel unlockModelData:&md];

	//	See comment in userDidChooseOptionCenterpiece above.
	if ([[self traitCollection] horizontalSizeClass] == UIUserInterfaceSizeClassCompact)
		[self dismissViewControllerAnimated:YES completion:nil];
}

- (void)userDidCancelOptionsChoice
{
	[self dismissViewControllerAnimated:YES completion:nil];
}


#pragma mark -
#pragma mark CurvedSpacesGraphicsViewDelegate

- (void)refreshSpeedSlider
{
	ModelData	*md	= NULL;
	double		theUserSpeed;

	[itsModel lockModelData:&md];
	theUserSpeed = md->itsUserSpeed;
	[itsModel unlockModelData:&md];

	[itsSpeedSlider setValue:theUserSpeed animated:YES];
}


#pragma mark -
#pragma mark CurvedSpacesHelpChoiceDelegate

- (void)dismissHelp
{
	[self dismissViewControllerAnimated:YES completion:nil];
}


@end
