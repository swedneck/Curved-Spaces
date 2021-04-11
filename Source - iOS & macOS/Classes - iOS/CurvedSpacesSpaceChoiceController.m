//	CurvedSpacesSpaceChoiceController.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesSpaceChoiceController.h"
#import "CurvedSpacesSpaceChoiceSubfolderController.h"
#import "GeometryGamesUtilities-Common.h"
#import "GeometryGamesUtilities-iOS.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"


static struct
{
	Char16	*itsNameKey,
			*itsFileName;
	bool	itsDodecahedralAlignmentAdjustmentIsNeeded;
} const gBasicSpaces[] =
		{
			{u"3-Torus",				u"3-Torus",						false	},
			{u"Poincaré Dodec. Space",	u"Poincaré Dodecahedral Space",	true	},
			{u"Mirrored Dodecahedron",	u"Mirrored Dodecahedron",		true	}
		};

static const Char16	* const	gIntermediateSubfolderNames[] =
							{
								u"Shape of Space, 3rd edition"
							};

static const Char16	* const	gAdvancedSubfolderNames[] =
							{
								u"Spherical",
								u"Flat",
								u"Hyperbolic"
							};


@implementation CurvedSpacesSpaceChoiceController
{
	id<CurvedSpacesSpaceChoiceDelegate> __weak	itsDelegate;
	NSString									*itsGeneratorFileRelativePath;	//	e.g. "Flat/Hantzsche Wendt.gen"
	UIBarButtonItem								*itsCancelButton,
												*itsDoneButton;
}

- (id)initWithDelegate:(id<CurvedSpacesSpaceChoiceDelegate>)aDelegate
		   spaceChoice:(NSString *)aGeneratorFileRelativePath	//	e.g. "Flat/Hantzsche Wendt.gen"
{
	self = [super initWithStyle:UITableViewStyleGrouped];
	if (self != nil)
	{
		itsDelegate						= aDelegate;
		itsGeneratorFileRelativePath	= aGeneratorFileRelativePath;

		[[self navigationItem] setTitle:GetLocalizedTextAsNSString(u"Space")];

		//	On an iPhone (or on an iPad with compact horizontal size,
		//	if that were possible with Curved Spaces)
		//	choosing any space automatically dismisses the Space Choice panel,
		//	so a "Cancel" button is appropriate to dismisses it without selecting a space.
		//	On an iPad (always with regular horizontal size class in Curved Spaces)
		//	the Space Choice panel stays up while the user changes the space,
		//	so a "Done" button is appropriate to dismiss the panel,
		//	because even when the panel goes away, any newly selected space remains.
		
		itsCancelButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:	UIBarButtonSystemItemCancel
			target:							aDelegate
			action:							@selector(userDidCancelSpaceChoice)];

		itsDoneButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:	UIBarButtonSystemItemDone
			target:							aDelegate
			action:							@selector(userDidCancelSpaceChoice)];
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
	UNUSED_PARAMETER(aTableView);

	return 3;
}

- (NSString *)tableView:(UITableView *)aTableView titleForHeaderInSection:(NSInteger)aSection
{
	UNUSED_PARAMETER(aTableView);

	switch (aSection)
	{
		case 0:  return GetLocalizedTextAsNSString(u"Basic");
		case 1:  return GetLocalizedTextAsNSString(u"Intermediate");
		case 2:  return GetLocalizedTextAsNSString(u"Advanced");

		default: return nil;
	}
}

- (NSInteger)tableView:(UITableView *)aTableView numberOfRowsInSection:(NSInteger)aSection
{
	UNUSED_PARAMETER(aTableView);

	switch (aSection)
	{
		case 0:  return BUFFER_LENGTH(gBasicSpaces);
		case 1:  return 1;
		case 2:  return 3;

		default: return 0;
	}
}

- (UITableViewCell *)tableView:(UITableView *)aTableView cellForRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	UITableViewCell	*theCell;
	UILabel			*theLabel;
	NSUInteger		theSection,
					theRow;
	NSString		*theRelativePath,
					*theTitle;
	bool			theCheckmark;

	UNUSED_PARAMETER(aTableView);

	theCell			= [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
	theLabel		= [theCell textLabel];
	theSection		= [anIndexPath section];
	theRow			= [anIndexPath row];
	
	switch (theSection)
	{
		case 0:	//	basic spaces

			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gBasicSpaces), "Invalid row index");

			theTitle		= GetLocalizedTextAsNSString(gBasicSpaces[theRow].itsNameKey);
			theRelativePath	= [NSString stringWithFormat:@"Basic/%@.gen",
								GetNSStringFromZeroTerminatedString(gBasicSpaces[theRow].itsFileName)];
			theCheckmark	= [itsGeneratorFileRelativePath isEqualToString:theRelativePath];

			[theLabel setText:theTitle];
			[theCell setAccessoryType:(theCheckmark ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone)];

			break;
		
		case 1:	//	intermediate spaces
		
			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gIntermediateSubfolderNames), "Invalid row index");

			theTitle = GetLocalizedTextAsNSString(gIntermediateSubfolderNames[theRow]);
			
			[theLabel setText:theTitle];
			[theCell setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
			
			break;
		
		case 2:	//	advanced spaces
		
			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gAdvancedSubfolderNames), "Invalid row index");

			theTitle = GetLocalizedTextAsNSString(gAdvancedSubfolderNames[theRow]);
			
			[theLabel setText:theTitle];
			[theCell setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
			
			break;

		default:
			[theLabel setText:LocalizationNotNeeded(@"INTERNAL ERROR")];
			break;
	}

	return theCell;
}


#pragma mark -
#pragma mark UITableViewDelegate

- (NSIndexPath *)tableView:(UITableView *)aTableView willSelectRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	NSUInteger									theSection,
												theRow;
	const Char16								*theSubfolderName;
	CurvedSpacesSpaceChoiceSubfolderController	*theSubfolderController;

	UNUSED_PARAMETER(aTableView);

	theSection	= [anIndexPath section];
	theRow		= [anIndexPath row];

	switch (theSection)
	{
		case 0:	//	basic spaces

			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gBasicSpaces), "Invalid row index");

			itsGeneratorFileRelativePath = [NSString stringWithFormat:@"Basic/%@.gen",
						GetNSStringFromZeroTerminatedString(gBasicSpaces[theRow].itsFileName)];

			//	Reload the table so the checkmark will appear
			//	by the newly selected Space.
			//	On an iPod touch or iPhone, the user will see the checkmark
			//	briefly as the table slides off the bottom of the screen.
			//	On an iPad the table will remain up.
			[[self tableView] reloadData];

			//	Report the new space to the caller,
			//	who will dismiss this CurvedSpacesSpaceChoiceController.
			[itsDelegate userDidChooseSpace:itsGeneratorFileRelativePath];
			
			if (gBasicSpaces[theRow].itsDodecahedralAlignmentAdjustmentIsNeeded)
				[itsDelegate spaceNeedsDodecahedralAlignmentAdjustment];

			break;
		
		case 1:	//	intermediate spaces
		
			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gIntermediateSubfolderNames), "Invalid row index");

			theSubfolderName		= gIntermediateSubfolderNames[theRow];
			theSubfolderController	= [[CurvedSpacesSpaceChoiceSubfolderController alloc]
										initWithDelegate:	itsDelegate
											   subfolder:	theSubfolderName
											 spaceChoice:	itsGeneratorFileRelativePath];
		
			[[self navigationController] pushViewController:theSubfolderController animated:YES];
			
			break;
		
		case 2:	//	advanced spaces
		
			GEOMETRY_GAMES_ASSERT(theRow < BUFFER_LENGTH(gAdvancedSubfolderNames), "Invalid row index");

			theSubfolderName		= gAdvancedSubfolderNames[theRow];
			theSubfolderController	= [[CurvedSpacesSpaceChoiceSubfolderController alloc]
										initWithDelegate:	itsDelegate
											   subfolder:	theSubfolderName
											 spaceChoice:	itsGeneratorFileRelativePath];
		
			[[self navigationController] pushViewController:theSubfolderController animated:YES];
			
			break;
	}

	return nil;
}

@end
