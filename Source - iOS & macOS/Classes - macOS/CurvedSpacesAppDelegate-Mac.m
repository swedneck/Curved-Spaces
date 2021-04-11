//	CurvedSpacesAppDelegate-Mac.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesAppDelegate-Mac.h"
#import "CurvedSpacesWindowController.h"
#import "GeometryGamesUtilities-Mac.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"


#define HELP_PANEL_WIDTH	320
#define HELP_PANEL_HEIGHT	768

//	HelpType serves to index gHelpInfo,
//	so its zero-based indexing is essential.
typedef enum
{
	HelpNone = 0,
	HelpHowToFly,
	HelpContact,
	HelpThankTranslators,
	HelpThankNSF,
	NumHelpTypes
} HelpType;

//	gHelpInfo[] is indexed by the HelpType enum.
const HelpPageInfo gHelpInfo[NumHelpTypes] =
{
	{u"n/a",			u"n/a",						u"n/a",				false	},
	{u"How to Fly",		u"Help - legacy format",	u"HowToFly-macOS",	true	},
	{u"Contact",		u"Help - legacy format",	u"Contact",			true	},
	{u"Translators",	u"Thanks",					u"Translators",		false	},
	{u"NSF",			u"Thanks",					u"NSF",				false	}
};


//	Privately-declared methods
@interface CurvedSpacesAppDelegate()
- (NSMenu *)buildLocalizedMenuBar;
@end


@implementation CurvedSpacesAppDelegate
{
}


- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	//	Let the superclass initialize a language, etc.
	[super applicationDidFinishLaunching:aNotification];
	
	//	Create a fallback set of user defaults (the "factory settings").
	//	These will get used if and only if the user has not provided his/her own values.
	[[NSUserDefaults standardUserDefaults] registerDefaults:
		@{
			//	At first launch we'll want to show the Help window,
			//	and show the Space Selection sheet.
			@"is first launch" : @YES,
		}
	];

	//	Set Help parameters.
	itsHelpPanelSize	= (CGSize){HELP_PANEL_WIDTH, HELP_PANEL_HEIGHT};
	itsNumHelpPages		= NumHelpTypes;
	itsHelpPageIndex	= HelpNone;
	itsHelpPageInfo		= gHelpInfo;
	
	//	Curved Spaces is a single-window app, so no tab bar is needed.
	//	(By default, macOS 10.12 enables window tabbing, and automatically
	//	inserts a Show Tab Bar item at the top of the View menu.)
	[NSWindow setAllowsAutomaticWindowTabbing:NO];

	//	Create a window.
	//	Typically the window created here will be the only one,
	//	but if ALLOW_MULTIPLE_WINDOWS is defined,
	//	the user may choose File : New Window to create additional windows.
	[itsWindowControllers addObject:[[CurvedSpacesWindowController alloc] initWithDelegate:self]];
	
	if (GetUserPrefBool(u"is first launch"))
		[self showHelpPageWithIndex:HelpHowToFly];
}


- (NSMenu *)buildLocalizedMenuBar
{
	NSMenu			*theMenuBar,
					*theMenu,
					*theSubmenu;
	NSMenuItem		*theItemWithKeyModifier;
	unsigned int	i;
	
	static const struct
	{
		CenterpieceType	itsCenterpieceType;
		Char16			*itsNameKey;
	} theCenterpieceList[] =
	{
		{CenterpieceNone,		u"No Centerpiece"	},
		{CenterpieceEarth,		u"Earth"			},
		{CenterpieceGalaxy,		u"Galaxy"			},
		{CenterpieceGyroscope,	u"Gyroscope"		}
#ifdef SHAPE_OF_SPACE_CH_7
		//	The language files contain no translations
		//	for the word "Cube", but that's OK, because
		//	I use CenterpieceCube only privately.
		//	It's not part of the public release.
		,
		{CenterpieceCube,		u"Cube"				}
#endif
	};
	
//	static const struct
//	{
//		CliffordMode	itsCliffordMode;
//		Char16			*itsNameKey,
//						itsKeyEquivalent;
//	} theCliffordList[] =
//	{
//		{CliffordNone,			u"CliffordNone",	 0  },
//		{CliffordBicolor,		u"Bicolor",			u'b'},
//#ifdef CLIFFORD_FLOWS_FOR_TALKS
//		{CliffordCenterlines,	u"Centerlines",		u'0'},
//#endif
//		{CliffordOneSet,		u"One Set",			u'1'},
//		{CliffordTwoSets,		u"Two Sets",		u'2'},
//		{CliffordThreeSets,		u"Three Sets",		u'3'}
//	};

	//	Construct all menus using the current language.
	
	//	Main menu bar
	theMenuBar = [[NSMenu alloc] initWithTitle:LocalizationNotNeeded(@"main menu")];
	
	//	Application menu
	//	(Cocoa replaces the title with the localized CFBundleName.)
	theMenu = AddSubmenuWithTitle(theMenuBar, u"Curved Spaces");
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"About Curved Spaces")
			action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
		[theMenu addItem:[NSMenuItem separatorItem]];
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Hide Curved Spaces")
			action:@selector(hide:) keyEquivalent:@"h"];
		theItemWithKeyModifier = [[NSMenuItem alloc]
					initWithTitle:GetLocalizedTextAsNSString(u"Hide Others")
					action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
	[theItemWithKeyModifier setKeyEquivalentModifierMask:
				(NSEventModifierFlagCommand | NSEventModifierFlagOption)];
			[theMenu addItem:theItemWithKeyModifier];
			theItemWithKeyModifier = nil;
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Show All")
			action:@selector(unhideAllApplications:) keyEquivalent:@""];
		[theMenu addItem:[NSMenuItem separatorItem]];
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Quit Curved Spaces")
			action:@selector(terminate:) keyEquivalent:@"q"];
	theMenu = nil;

#ifdef ALLOW_MULTIPLE_WINDOWS
	//	For the most part, the Geometry Games software doesn't save files,
	//	so it makes no sense to have a File menu.  Moreover, there's little need
	//	to open more than one window at once, and doing so could slow
	//	the animations' frame rates.  Nevertheless, just for testing purposes,
	//	here's a File menu that lets the user open and close windows.
	theMenu = AddSubmenuWithTitle(theMenuBar, u"File");
		[theMenu addItemWithTitle:@"New Window"
			action:@selector(commandNewWindow:) keyEquivalent:@"n"];
		[theMenu addItemWithTitle:@"Close Window"
			action:@selector(performClose:) keyEquivalent:@"w"];
	theMenu = nil;
#endif

	theMenu = AddSubmenuWithTitle(theMenuBar, u"Space");
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Change Space…")
			action:@selector(commandChooseSpace:) keyEquivalent:@"o"];
	theMenu = nil;
	
	//	View menu
	theMenu = AddSubmenuWithTitle(theMenuBar, u"View");

		theSubmenu = AddSubmenuWithTitle(theMenu, u"Centerpiece");
			for (i = 0; i < BUFFER_LENGTH(theCenterpieceList); i++)
			{
				[theSubmenu addItem:MenuItemWithTitleActionTag(
							GetLocalizedTextAsNSString(theCenterpieceList[i].itsNameKey),
							@selector(commandCenterpiece:),
							theCenterpieceList[i].itsCenterpieceType)];
			}
		theSubmenu = nil;

		[theMenu addItem:[NSMenuItem separatorItem]];

		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Fog")
			action:@selector(commandFog:) keyEquivalent:@""];

		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Color Coding")
			action:@selector(commandColorCoding:) keyEquivalent:@""];

		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Vertex Figures")
			action:@selector(commandVertexFigures:) keyEquivalent:@""];

		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Self")
			action:@selector(commandObserver:) keyEquivalent:@""];

#ifdef HANTZSCHE_WENDT_AXES
//		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Hantzsche-Wendt Axes")
		[theMenu addItemWithTitle:@"Hantzsche-Wendt Axes"
			action:@selector(commandHantzscheWendt:) keyEquivalent:@""];
#endif

//		theSubmenu = AddSubmenuWithTitle(theMenu, u"Clifford Parallels");
//			for (i = 0; i < BUFFER_LENGTH(theCliffordList); i++)
//			{
//				[theSubmenu addItem:MenuItemWithTitleActionKeyTag(
//					GetLocalizedTextAsNSString(theCliffordList[i].itsNameKey),
//					@selector(commandCliffordParallels:),
//					theCliffordList[i].itsKeyEquivalent,
//					theCliffordList[i].itsCliffordMode)];
//			}
//		theSubmenu = nil;

#ifdef CLIFFORD_FLOWS_FOR_TALKS
//English only for now.
//If I ever add something similar to the public version, localize to all languages.
//Note too that the key equivalents ⌘X and ⌘Z conflict with Cut and Undo.
		[theMenu addItemWithTitle:@"Clifford Flow XY"
			action:@selector(commandCliffordFlowXY:) keyEquivalent:@"x"];
		[theMenu addItemWithTitle:@"Clifford Flow ZW"
			action:@selector(commandCliffordFlowZW:) keyEquivalent:@"z"];
#endif

	theMenu = nil;

	//	Export menu
	theMenu = AddSubmenuWithTitle(theMenuBar, u"Export");
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Copy Image")
			action:@selector(commandCopyImageRGB:) keyEquivalent:@"c"];	//	use commandCopyImageRGBA: to save with transparency
		[theMenu addItemWithTitle:GetLocalizedTextAsNSString(u"Save Image…")
			action:@selector(commandSaveImageRGB:) keyEquivalent:@"s"];	//	use commandCopyImageRGBA: to save with transparency
	theMenu = nil;

	//	Help menu
	theMenu = AddSubmenuWithTitle(theMenuBar, u"Help");
		[theMenu addItem:MenuItemWithTitleActionTag(
					GetLocalizedTextAsNSString(gHelpInfo[HelpHowToFly].itsTitleKey),
					@selector(commandHelp:),
					HelpHowToFly)];
		[theMenu addItem:MenuItemWithTitleActionTag(
					GetLocalizedTextAsNSString(gHelpInfo[HelpContact].itsTitleKey),
					@selector(commandHelp:),
					HelpContact)];
		[theMenu addItem:[NSMenuItem separatorItem]];
		[theMenu addItem:MenuItemWithTitleActionTag(
					GetLocalizedTextAsNSString(gHelpInfo[HelpThankTranslators].itsTitleKey),
					@selector(commandHelp:),
					HelpThankTranslators)];
		[theMenu addItem:MenuItemWithTitleActionTag(
					GetLocalizedTextAsNSString(gHelpInfo[HelpThankNSF].itsTitleKey),
					@selector(commandHelp:),
					HelpThankNSF)];
	theMenu = nil;
	
	return theMenuBar;
}


- (BOOL)validateMenuItem:(NSMenuItem *)aMenuItem
{
#ifdef ALLOW_MULTIPLE_WINDOWS
	SEL	theAction;
	
	theAction = [aMenuItem action];

	if (theAction == @selector(commandNewWindow:))
	{
		return YES;
	}
#endif

	return [super validateMenuItem:aMenuItem];
}

#ifdef ALLOW_MULTIPLE_WINDOWS
- (void)commandNewWindow:(id)sender
{
	[itsWindowControllers addObject:[[CurvedSpacesWindowController alloc] initWithDelegate:self]];
}
#endif

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
	[super applicationWillTerminate:aNotification];
	
	SetUserPrefBool(u"is first launch", false);
}


@end
