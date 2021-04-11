//	CurvedSpacesGraphicsViewMac.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "GeometryGamesGraphicsViewMac.h"


@interface CurvedSpacesGraphicsViewMac : GeometryGamesGraphicsViewMac <NSGestureRecognizerDelegate>

- (id)initWithModel:(GeometryGamesModel *)aModel frame:(NSRect)aFrame;

- (void)keyDown:(NSEvent *)anEvent;
- (void)mouseDragged:(NSEvent *)anEvent;

//	NSGestureRecognizerDelegate
- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer
	shouldRecognizeSimultaneouslyWithGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer;

@end
