//	CurvedSpacesWindowController.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "GeometryGamesWindowController.h"
#import "CurvedSpaces-Common.h"	//	for #defined symbols


@interface CurvedSpacesWindowController : GeometryGamesWindowController
	<NSOutlineViewDataSource>

- (id)initWithDelegate:(id<GeometryGamesWindowControllerDelegate>)aDelegate;
- (void)createSubviews;
- (void)windowWillClose:(NSNotification *)aNotification;

- (NSRect)windowWillUseStandardFrame:(NSWindow *)window defaultFrame:(NSRect)newFrame;

- (BOOL)validateMenuItem:(NSMenuItem *)aMenuItem;
- (void)commandChooseSpace:(id)sender;
- (void)commandCenterpiece:(id)sender;
- (void)commandObserver:(id)sender;
- (void)commandColorCoding:(id)sender;
#ifdef HANTZSCHE_WENDT_AXES
- (void)commandHantzscheWendt:(id)sender;
#endif
- (void)commandCliffordParallels:(id)sender;
#ifdef CLIFFORD_FLOWS_FOR_TALKS
- (void)commandCliffordFlowXY:(id)sender;
- (void)commandCliffordFlowZW:(id)sender;
#endif
- (void)commandVertexFigures:(id)sender;
- (void)commandFog:(id)sender;

- (void)languageDidChange;

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

@end
