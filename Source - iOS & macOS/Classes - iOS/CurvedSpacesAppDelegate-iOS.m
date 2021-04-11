//	CurvedSpacesAppDelegate-iOS.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesAppDelegate-iOS.h"
#import "CurvedSpacesRootController.h"
#import "GeometryGamesUtilities-Common.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"
#import <CoreMotion/CMMotionManager.h>


//	Privately-declared properties and methods
@interface CurvedSpacesAppDelegate()
@end


@implementation CurvedSpacesAppDelegate
{
	UIWindow	*itsWindow;	//	strong reference keeps window alive
}


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
	NSString					*theTwoLetterLanguageCodeAsNSString;
	Char16						theTwoLetterLanguageCode[3];
	CMMotionManager				*theMotionManager;
	CurvedSpacesRootController	*theRootController;

	UNUSED_PARAMETER(application);
	UNUSED_PARAMETER(launchOptions);

#if TARGET_OS_SIMULATOR
#ifdef MAKE_SCREENSHOTS
	//	Here's where the screenshots will go.
	NSLog(@"Documents directory:  %@",
		NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES)[0]);
#endif
#endif

	//	Create a fallback set of user defaults (the "factory settings").
	//	These will get used if and only if the user has not provided his/her own values.
	[[NSUserDefaults standardUserDefaults] registerDefaults:
		@{
			//	At first launch we'll want to show the Help window.
			@"is first launch" : @YES,

			//	Should exported images be rendered
			//	at single, double or quadruple resolution?
			//	This factor gets applied to the view's dimension
			//	in points (not pixels) so set the default to 2,
			//	so that the quality of the exported image will match
			//	the quality of the onscreen image on a retina display.
			//
			@"exported image magnification factor" : @2
		}
	];

	//	Get the user's preferred language and convert it to a zero-terminated UTF-16 string.
	theTwoLetterLanguageCodeAsNSString = GetPreferredLanguage();
	[theTwoLetterLanguageCodeAsNSString getCharacters:theTwoLetterLanguageCode range:(NSRange){0,2}];
	theTwoLetterLanguageCode[2] = 0;

	//	Initialize the language code and dictionary to the user's default language.
	SetCurrentLanguage(theTwoLetterLanguageCode);
	
	//	Create theMotionManager.
	//	Apple recommends using a single app-wide CMMotionManager for best performance.
	theMotionManager = [[CMMotionManager alloc] init];
	[theMotionManager startDeviceMotionUpdates];

	//	Create itsWindow.
	itsWindow = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

	//	Create the root controller and assign it to the window.
	theRootController = [[CurvedSpacesRootController alloc]
							initWithNibName:nil
							bundle:nil
							motionManager:theMotionManager];
	[itsWindow setRootViewController:theRootController];

	//	Show the window.
	[itsWindow makeKeyAndVisible];

#ifdef MAKE_SCREENSHOTS
	//	Don't auto-show the Help window when making screenshots.
#else
	if (GetUserPrefBool(u"is first launch"))
	{
		//	Simulate a tap on the Help button.
		//
		//		Caution:  On iOS 13, presenting a popover
		//		immediately at this point would cause a crash.
		//		So we must delay this call by some small amount
		//		(in practice even a delay of 0.0 seconds works fine,
		//		because it delays the call to simulateHelpButtonTap
		//		to the next pass through the run loop).
		//		On iOS 12 and earlier the call seemed
		//		to work fine without the delay.
		//		I'm a little embarrassed to write such kludgy code,
		//		but presumably this code will get retired
		//		when I migrate to SwiftUI.
		//
		[theRootController
			performSelector:@selector(simulateHelpButtonTap)
			withObject:nil
			afterDelay:0.25];	//	0.0 would be fine (see above), but let's use 0.25 to be safe
	}
#endif

	return YES;
}


@end
