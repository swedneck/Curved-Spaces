//	CurvedSpacesSpaceChoiceSubfolderController.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesSpaceChoiceSubfolderController.h"
#import "GeometryGamesUtilities-iOS.h"
#import "GeometryGamesUtilities-Mac-iOS.h"
#import "GeometryGamesLocalization.h"


static NSArray<NSString *>	*GetFilesInFolder(NSString *aFolderPath);


@implementation CurvedSpacesSpaceChoiceSubfolderController
{
	id<CurvedSpacesSpaceChoiceDelegate> __weak	itsDelegate;
	const Char16								*itsSubfolderName;	//	not localized
	NSString									*itsGeneratorFileRelativePath;	//	e.g. "Flat/Hantzsche Wendt.gen"
	UIBarButtonItem								*itsCancelButton,
												*itsDoneButton;
	NSArray<NSString *>							*itsFileList;	//	without subfolder name, but with .gen file extension,
																//		e.g. "Hantzsche Wendt.gen"
}

- (id)initWithDelegate:(id<CurvedSpacesSpaceChoiceDelegate>)aDelegate
			 subfolder:(const Char16 *)aSubfolderName
		   spaceChoice:(NSString *)aGeneratorFileRelativePath	//	e.g. "Flat/Hantzsche Wendt.gen"
{
	NSString	*theSubfolderPath;

	self = [super initWithStyle:UITableViewStyleGrouped];
	if (self != nil)
	{
		itsDelegate						= aDelegate;
		itsSubfolderName				= aSubfolderName;
		itsGeneratorFileRelativePath	= aGeneratorFileRelativePath;

		[[self navigationItem] setTitle:GetLocalizedTextAsNSString(itsSubfolderName)];

		//	See the comment in CurvedSpacesSpaceChoiceController's -initWithDelegate:…,
		//	which explains why we may sometimes need a Done button instead of a Cancel button.

		itsCancelButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:	UIBarButtonSystemItemCancel
			target:							aDelegate
			action:							@selector(userDidCancelSpaceChoice)];

		itsDoneButton = [[UIBarButtonItem alloc]
			initWithBarButtonSystemItem:	UIBarButtonSystemItemDone
			target:							aDelegate
			action:							@selector(userDidCancelSpaceChoice)];

		theSubfolderPath = [NSString stringWithFormat:@"%@/Sample Spaces/%@/",
								[[NSBundle mainBundle] resourcePath],
								GetNSStringFromZeroTerminatedString(aSubfolderName)];

		itsFileList = GetFilesInFolder(theSubfolderPath);
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

	return 1;
}

- (NSString *)tableView:(UITableView *)aTableView titleForHeaderInSection:(NSInteger)aSection
{
	UNUSED_PARAMETER(aTableView);
	UNUSED_PARAMETER(aSection);

	return nil;
}

- (NSInteger)tableView:(UITableView *)aTableView numberOfRowsInSection:(NSInteger)aSection
{
	UNUSED_PARAMETER(aTableView);
	UNUSED_PARAMETER(aSection);

	return [itsFileList count];
}

- (UITableViewCell *)tableView:(UITableView *)aTableView cellForRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	UITableViewCell	*theCell;
	UILabel			*theLabel;
	NSUInteger		theSection,
					theRow;
	NSString		*theFileName,				//	for example, "Hantzsche Wendt.gen"
					*theRelativePath,			//	for example, "Flat/Hantzsche Wendt.gen"
					*theExtensionlessFileName,	//	for example, "Hantzsche Wendt"
					*theTitle;
	bool			theCheckmark;

	UNUSED_PARAMETER(aTableView);

	theCell		= [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:nil];
	theLabel	= [theCell textLabel];
	theSection	= [anIndexPath section];
	theRow		= [anIndexPath row];
	
	//	Somewhat surprisingly (at least to me), theLabel comes
	//	with a default font of size 14.0pt on iPad,
	//	but 17.0pt on iPhone.  So...
	//
	//		The sample spaces' names fit acceptably well
	//			in a 320pt wide popover on iPad,
	//			because they appear in a 14pt font.
	//
	//		The sample spaces' names fit acceptably well
	//			on most current phones, because even though
	//			they appear in a 17pt font, the phones
	//			are at least 375pt wide, and the popover
	//			expands to fill the width of the phone.
	//
	//		A few of the sample spaces' names, for example
	//			"Ch 18 - Fig 18-3(h) hexKlein with horizontal flip"
	//			fit poorly in a 17pt font on a 320pt wide phone
	//			like the iPhone SE, but Apple seems to be phasing out
	//			phones of this width, so this problem will soon disappear.
	//
	//	Note:  To check the font size when running under Xcode
	//	(with either a real device or in the simulator), use the line
	//
	//		CGFloat theSize = theLabel.font.pointSize;
	//
	
	if (theSection == 0					//	should never fail
	 && theRow < [itsFileList count])	//	should never fail
	{
		theFileName = [itsFileList objectAtIndex:theRow];
		if ([theFileName hasSuffix:@".gen"])	//	should never fail
		{
			theRelativePath				= [NSString stringWithFormat:@"%@/%@",
											GetNSStringFromZeroTerminatedString(itsSubfolderName),
											theFileName];
			theExtensionlessFileName	= [theFileName substringToIndex:( [theFileName length] - [@".gen" length] )];

			theTitle		= theExtensionlessFileName;
			theCheckmark	= [itsGeneratorFileRelativePath isEqualToString:theRelativePath];
		}
		else
		{
			theTitle		= @"INTERNAL ERROR";
			theCheckmark	= false;
		}
	}
	else
	{
		theTitle		= @"INTERNAL ERROR";
		theCheckmark	= false;
	}
	
	[theLabel setText:LocalizationNotNeeded(theTitle)];
	[theCell setAccessoryType:(theCheckmark ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone)];

	return theCell;
}


#pragma mark -
#pragma mark UITableViewDelegate

- (NSIndexPath *)tableView:(UITableView *)aTableView willSelectRowAtIndexPath:(NSIndexPath *)anIndexPath
{
	NSUInteger	theSection,
				theRow;
	NSString	*theFileName,		//	for example, "Hantzsche Wendt.gen"
				*theRelativePath;	//	for example, "Flat/Hantzsche Wendt.gen"
	
	UNUSED_PARAMETER(aTableView);

	theSection	= [anIndexPath section];
	theRow		= [anIndexPath row];
	
	GEOMETRY_GAMES_ASSERT(theSection == 0,				"Invalid section index"	);
	GEOMETRY_GAMES_ASSERT(theRow < [itsFileList count],	"Invalid row index"		);

	theFileName		= [itsFileList objectAtIndex:theRow];
	theRelativePath	= [NSString stringWithFormat:@"%@/%@",
						GetNSStringFromZeroTerminatedString(itsSubfolderName),
						theFileName];

	itsGeneratorFileRelativePath = theRelativePath;

	//	Reload the table so the checkmark will appear
	//	by the newly selected Space.
	//	On an iPod touch or iPhone, the user will see the checkmark
	//	briefly as the table slides off the bottom of the screen.
	//	On an iPad the table will remain up.
	[[self tableView] reloadData];

	//	Report the new space to the caller,
	//	who will dismiss this CurvedSpacesSpaceChoiceSubfolderController.
	[itsDelegate userDidChooseSpace:itsGeneratorFileRelativePath];

	return nil;
}

@end


static NSArray<NSString *> *GetFilesInFolder(
	NSString	*aFolderPath)
{
	NSFileManager		*theFileManager;
	BOOL				theDirectoryExists,
						theIsDirectoryFlag;
	NSArray<NSString *>	*theRawFileList,
						*theFilteredFileList,
						*theSortedFileList;

	theFileManager = [NSFileManager defaultManager];

	theDirectoryExists = [theFileManager fileExistsAtPath:aFolderPath isDirectory:&theIsDirectoryFlag];
	if ( ! theDirectoryExists || ! theIsDirectoryFlag )	//	should never occur
		return [NSArray array];	//	return empty array

	//	Get the folder's contents.  The file manager will return
	//	a list of file names with extensions, but without full paths,
	//	for example
	//
	//		"Half Turn.gen"
	//		"Hantzsche Wendt.gen"
	//		…
	//
	theRawFileList = [theFileManager contentsOfDirectoryAtPath:aFolderPath error:NULL];
	
	//	The "Spherical", "Flat" and "Hyperbolic" folders currently contain
	//	only .gen files.  But if someday somebody added, say, a "Read Me.txt" file,
	//	we'd want to avoid it, and report only the .gen files.
	theFilteredFileList = [theRawFileList filteredArrayUsingPredicate:
							[NSPredicate predicateWithFormat:@"self ENDSWITH '.gen'"]];
	
	//	Even though -contentsOfDirectoryAtPath: seems to return the files
	//	in alphabetical order on iOS, the documentation says
	//
	//		"The order of the files in the returned array is undefined."
	//
	//	so let's sort them.
	theSortedFileList = [theFilteredFileList sortedArrayUsingSelector:@selector(localizedStandardCompare:)];


	return theSortedFileList;
}
