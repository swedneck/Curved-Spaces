//	CurvedSpacesOptionsChoiceController.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesOptionsChoiceController.h"
#import "GeometryGamesUtilities-iOS.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"


struct
{
	CenterpieceType	itsCenterpieceType;
	Char16			*itsName,
					*itsImageName;
} gCenterpieceNames[] =
{
	{CenterpieceNone,		u"No Centerpiece",	u"Table Images/Options/Centerpiece/None"		},
	{CenterpieceEarth,		u"Earth",			u"Table Images/Options/Centerpiece/Earth"		},
	{CenterpieceGalaxy,		u"Galaxy",			u"Table Images/Options/Centerpiece/Galaxy"		},
	{CenterpieceGyroscope,	u"Gyroscope",		u"Table Images/Options/Centerpiece/Gyroscope"	}
};


//	Privately-declared properties and methods
@interface CurvedSpacesOptionsChoiceController()
- (void)fogToggled:(id)sender;
- (void)colorCodingToggled:(id)sender;
- (void)vertexFiguresToggled:(id)sender;
- (void)observerToggled:(id)sender;
- (void)nothingToggled:(id)sender;
@end


@implementation CurvedSpacesOptionsChoiceController
{
	id<CurvedSpacesOptionsChoiceDelegate> __weak	itsDelegate;
	CenterpieceType									itsCenterpieceChoice;
	bool											itsFogChoice,
													itsColorCodingChoice,
													itsVertexFiguresChoice,
													itsObserverChoice;	//	show self?
	UIBarButtonItem									*itsCancelButton,
													*itsDoneButton;
}


- (id)	initWithDelegate:	(id<CurvedSpacesOptionsChoiceDelegate>)aDelegate 
			 centerpiece:	(CenterpieceType)aCenterpiece
					 fog:	(bool)aShowFog
			 colorCoding:	(bool)aShowColorCoding
		   vertexFigures:	(bool)aShowVertexFigures
				observer:	(bool)aShowObserver	//	"show self"
{
	self = [super initWithStyle:UITableViewStyleGrouped];
	if (self != nil)
	{
		itsDelegate = aDelegate;

		itsCenterpieceChoice	= aCenterpiece;
		itsFogChoice			= aShowFog;
		itsColorCodingChoice	= aShowColorCoding;
		itsVertexFiguresChoice	= aShowVertexFigures;
		itsObserverChoice		= aShowObserver;

		[[self navigationItem] setTitle:GetLocalizedTextAsNSString(u"Options")];

		//	On an iPhone (or on an iPad with compact horizontal size, if that were possible)
		//	choosing any option automatically dismisses the Options panel,
		//	so a "Cancel" button is appropriate to dismisses it without making any changes.
		//	On an iPad (always with regular horizontal size class in Curved Spaces)
		//	the Options panel stays up while the user changes the options,
		//	so a "Done" button is appropriate to dismiss the panel,
		//	because even when the panel goes away, the user's changes remain.
		
		itsCancelButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:	UIBarButtonSystemItemCancel
			target:							aDelegate
			action:							@selector(userDidCancelOptionsChoice)];
		
		itsDoneButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:	UIBarButtonSystemItemDone
			target:							aDelegate
			action:							@selector(userDidCancelOptionsChoice)];
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
	
	//	We must call -layoutIfNeeded explicitly, otherwise -contentSize returns CGSizeZero.
	[[self tableView] layoutIfNeeded];
	[self setPreferredContentSize:(CGSize){	PREFERRED_POPOVER_WIDTH,
											[[self tableView] contentSize].height}];

	[self adaptNavBarForHorizontalSize:
		[[RootViewController(self) traitCollection] horizontalSizeClass]];
}


#pragma mark -
#pragma mark GeometryGamesPopover

- (void)adaptNavBarForHorizontalSize:(UIUserInterfaceSizeClass)aHorizontalSizeClass
{
	[[self navigationItem]
		setRightBarButtonItem:	(aHorizontalSizeClass == UIUserInterfaceSizeClassCompact ?
									itsCancelButton : itsDoneButton)
		animated:				NO];
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
		case 0:		return GetLocalizedTextAsNSString(u"Centerpiece");
		case 1:		return nil;	//	this section needs no title
		default:	return @"internal error";
	}
}

- (NSInteger)tableView:(UITableView *)aTableView numberOfRowsInSection:(NSInteger)aSection
{
	switch (aSection)
	{
		case 0:		return BUFFER_LENGTH(gCenterpieceNames);
		case 1:		return 4;
		default:	return 0;
	}
}

- (UITableViewCell *)tableView:(UITableView *)aTableView cellForRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	UITableViewCell	*theCell;
	UILabel			*theLabel;
	NSUInteger		theSection,
					theRow;
	const Char16	*theTitle,
					*theImageName;
	bool			theCheckmark;
	UISwitch		*theSwitch;
	bool			theSwitchIsOn;
	SEL				theSwitchAction;

	theCell		= [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
	theLabel	= [theCell textLabel];

	theSection	= [anIndexPath section];
	theRow		= [anIndexPath row];

	switch (theSection)
	{
		case 0:

			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gCenterpieceNames), "invalid row number");

			theTitle		= GetLocalizedText(gCenterpieceNames[theRow].itsName);
			theImageName	= gCenterpieceNames[theRow].itsImageName;
			theCheckmark	= (gCenterpieceNames[theRow].itsCenterpieceType == itsCenterpieceChoice);

			[theLabel setText:GetNSStringFromZeroTerminatedString(theTitle)];

			[[theCell imageView] setImage:[UIImage imageNamed:
				GetNSStringFromZeroTerminatedString(theImageName)]];

			[theCell setAccessoryType:(theCheckmark ?
						UITableViewCellAccessoryCheckmark :
						UITableViewCellAccessoryNone)];

			break;
		
		case 1:

			theSwitch = [[UISwitch alloc] initWithFrame:CGRectZero];

			switch (theRow)
			{
				case 0:
					theTitle		= u"Fog";
					theSwitchIsOn	= itsFogChoice;
					theSwitchAction	= @selector(fogToggled:);
					break;
				
				case 1:
					theTitle		= u"Color Coding";
					theSwitchIsOn	= itsColorCodingChoice;
					theSwitchAction	= @selector(colorCodingToggled:);
					break;
				
				case 2:
					theTitle		= u"Vertex Figures";
					theSwitchIsOn	= itsVertexFiguresChoice;
					theSwitchAction	= @selector(vertexFiguresToggled:);
					break;
				
				case 3:
					theTitle		= u"Self";
					theSwitchIsOn	= itsObserverChoice;
					theSwitchAction	= @selector(observerToggled:);
					break;
				
				default:
					theTitle		= u"internal error";
					theSwitchIsOn	= false;
					theSwitchAction	= @selector(nothingToggled:);
					break;
			}

			[theLabel setText:GetLocalizedTextAsNSString(theTitle)];

			[theSwitch setOn:theSwitchIsOn];
			[theSwitch addTarget:self action:theSwitchAction forControlEvents:UIControlEventValueChanged];

			[theCell setAccessoryView:theSwitch];
			[theCell setSelectionStyle:UITableViewCellSelectionStyleNone];

			break;

		default:	//	should never occur
			break;
	}

	return theCell;
}


#pragma mark -
#pragma mark UITableViewDelegate

- (NSIndexPath *)tableView:(UITableView *)aTableView willSelectRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	NSUInteger	theSection,
				theRow;
	
	theSection	= [anIndexPath section];
	theRow		= [anIndexPath row];

	switch (theSection)
	{
		case 0:

			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gCenterpieceNames), "invalid row number");

			itsCenterpieceChoice = gCenterpieceNames[theRow].itsCenterpieceType;

			//	Reload the table so the checkmark will appear by the newly selected option.
			//	The user will see it change briefly as the table animates away.
			[[self tableView] reloadData];

			//	Report the new option to the caller,
			//	who will dismiss this CurvedSpacesOptionsChoiceController.
			[itsDelegate userDidChooseOptionCenterpiece:itsCenterpieceChoice];

			break;

		case 1:
			//	Ignore taps on cells containing switches.
			//	The switches will notify us directly when their values change.
			return nil;
	}	

	return nil;
}


#pragma mark -
#pragma mark switch actions

- (void)fogToggled:(id)sender
{
	if ([sender isKindOfClass:[UISwitch class]])	//	should never fail
	{
		itsFogChoice = [((UISwitch *)sender) isOn];
		[itsDelegate userDidChooseOptionFog:itsFogChoice];
	}
}

- (void)colorCodingToggled:(id)sender
{
	if ([sender isKindOfClass:[UISwitch class]])	//	should never fail
	{
		itsColorCodingChoice = [((UISwitch *)sender) isOn];
		[itsDelegate userDidChooseOptionColorCoding:itsColorCodingChoice];
	}
}

- (void)vertexFiguresToggled:(id)sender
{
	if ([sender isKindOfClass:[UISwitch class]])	//	should never fail
	{
		itsVertexFiguresChoice = [((UISwitch *)sender) isOn];
		[itsDelegate userDidChooseOptionVertexFigures:itsVertexFiguresChoice];
	}
}

- (void)observerToggled:(id)sender
{
	if ([sender isKindOfClass:[UISwitch class]])	//	should never fail
	{
		itsObserverChoice = [((UISwitch *)sender) isOn];
		[itsDelegate userDidChooseOptionObserver:itsObserverChoice];
	}
}

- (void)nothingToggled:(id)sender
{
	//	This method should never get called.
}



@end
