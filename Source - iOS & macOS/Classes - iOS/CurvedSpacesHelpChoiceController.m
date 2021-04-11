//	CurvedSpacesHelpChoiceController.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesHelpChoiceController.h"
#import "CurvedSpaces-Common.h"	//	for #definition of MAKE_SCREENSHOTS
#import "GeometryGamesWebViewController.h"
#import "GeometryGamesUtilities-iOS.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"


struct
{
	Char16	*itsName,
			*itsDirectoryName,
			*itsFileName;
	bool	itsFileIsLocalized;
	Char16	*itsImageName;
} gHelpInfo[2][2] =
{
	{
		{u"How to Fly",	u"Help - legacy format",	u"HowToFly-iOS-%@.html",	true,	u"Table Images (shared)/Help/HowTo"			},
		{u"Contact",	u"Help - legacy format",	u"Contact-%@.html",			true,	u"Table Images (shared)/Help/Contact"		}
	},
	{
		{u"Translators",u"Thanks",					u"Translators.html",		false,	u"Table Images (shared)/Thanks/Translators"	},
		{u"NSF",		u"Thanks",					u"NSF.html",				false,	u"Table Images (shared)/Thanks/NSF"			}
	}
};


@implementation CurvedSpacesHelpChoiceController
{
	id<CurvedSpacesHelpChoiceDelegate> __weak	itsDelegate;
	UIBarButtonItem								*itsCloseButton;
}


- (id)initWithDelegate:(id<CurvedSpacesHelpChoiceDelegate>)aDelegate
{
	self = [super initWithStyle:UITableViewStyleGrouped];
	if (self != nil)
	{
		itsDelegate = aDelegate;
		
		[self setTitle:GetLocalizedTextAsNSString(u"Help")];

		itsCloseButton = [[UIBarButtonItem alloc]
			initWithTitle:	GetLocalizedTextAsNSString(u"Close")
			style:			UIBarButtonItemStyleDone
			target:			aDelegate
			action:			@selector(dismissHelp)];
	}
	return self;
}

- (BOOL)prefersStatusBarHidden
{
	return [itsDelegate prefersStatusBarHidden];
}

- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];
	
	[self setPreferredContentSize:(CGSize){HELP_PICKER_WIDTH, HELP_PICKER_HEIGHT}];

	[self adaptNavBarForHorizontalSize:
		[[RootViewController(self) traitCollection] horizontalSizeClass]];
}

- (void)viewDidAppear:(BOOL)animated
{
	[super viewDidAppear:animated];

#ifdef MAKE_SCREENSHOTS

	//	Simulate a tap on the "How to Fly" item.
	[self tableView:[self tableView]
		willSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

#else

	if (GetUserPrefBool(u"is first launch"))
	{
		//	Simulate a tap on the "How to Fly" item.
		[self tableView:[self tableView]
			willSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
		
		//	We're done with the first-launch action,
		//	so set the user preference to "false"
		//	to avoid repeating auto-showing the Help panel next time.
		SetUserPrefBool(u"is first launch", false);
	}

#endif	//	MAKE_SCREENSHOTS
}


#pragma mark -
#pragma mark GeometryGamesPopover

- (void)adaptNavBarForHorizontalSize:(UIUserInterfaceSizeClass)aHorizontalSizeClass
{
	//	Provide a Close button even in a horizontally regular layout,
	//	because the passthrough views will include the game view.
	//
	[[self navigationItem] setRightBarButtonItem:itsCloseButton];
}



#pragma mark -
#pragma mark UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)aTableView
{
	return 2;
}

- (NSString *)tableView:(UITableView *)aTableView titleForHeaderInSection:(NSInteger)aSection
{
	switch (aSection)
	{
		case 0:		return nil;
		case 1:		return GetLocalizedTextAsNSString(u"Thanks");
		default:	return @"internal error";
	}
	return nil;
}

- (NSInteger)tableView:(UITableView *)aTableView numberOfRowsInSection:(NSInteger)aSection
{
	switch (aSection)
	{
		case 0:		return 2;
		case 1:		return 2;
		default:	return 0;
	}
}

- (UITableViewCell *)tableView:(UITableView *)aTableView cellForRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	UITableViewCell	*theCell;
	UILabel			*theLabel;
	NSUInteger		theSection,
					theRow;

	theCell		= [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
	theLabel	= [theCell textLabel];
	theSection	= [anIndexPath section];
	theRow		= [anIndexPath row];

	[theLabel setText:GetLocalizedTextAsNSString(gHelpInfo[theSection][theRow].itsName)];
	[[theCell imageView] setImage:[UIImage imageNamed:
		GetNSStringFromZeroTerminatedString(gHelpInfo[theSection][theRow].itsImageName)]];
	[theCell setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];

	return theCell;
}


#pragma mark -
#pragma mark UITableViewDelegate

- (NSIndexPath *)tableView:(UITableView *)aTableView willSelectRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	NSUInteger						theSection,
									theRow;
	NSString						*theDirectoryName,
									*theFileBaseName,
									*theLanguageSuffix,
									*theFileName;
	GeometryGamesWebViewController	*theWebViewController;

	theSection			= [anIndexPath section];
	theRow				= [anIndexPath row];
	theDirectoryName	= GetNSStringFromZeroTerminatedString(gHelpInfo[theSection][theRow].itsDirectoryName);
	theFileBaseName		= GetNSStringFromZeroTerminatedString(gHelpInfo[theSection][theRow].itsFileName		);

	if (theFileBaseName != nil)	//	request for HTML page
	{
		if (gHelpInfo[theSection][theRow].itsFileIsLocalized)
		{
			//	My legacy system used a strictly 2-letter language code.
			//	To allow for simplified and traditional Chinese,
			//	it used two exceptional codes
			//
			//		zs -> zh-Hans
			//		zt -> zh-Hant
			//
			if (IsCurrentLanguage(u"zs"))
				theLanguageSuffix = @"zh-Hans";
			else
			if (IsCurrentLanguage(u"zt"))
				theLanguageSuffix = @"zh-Hant";
			else
				theLanguageSuffix = GetNSStringFromZeroTerminatedString(GetCurrentLanguage());

			theFileName = [NSString stringWithFormat:theFileBaseName, theLanguageSuffix];
		}
		else
		{
			theFileName = theFileBaseName;
		}

		theWebViewController = [[GeometryGamesWebViewController alloc]
				initWithDirectory:		theDirectoryName
				page:					theFileName
				closeButton:			itsCloseButton
				showCloseButtonAlways:	YES
				hideStatusBar:			[self prefersStatusBarHidden] ];
		[[self navigationController] pushViewController:theWebViewController animated:YES];
	}

	return nil;
}

@end
