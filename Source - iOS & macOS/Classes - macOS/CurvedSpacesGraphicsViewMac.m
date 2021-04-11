//	CurvedSpacesGraphicsViewMac.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesGraphicsViewMac.h"
#import "CurvedSpaces-Common.h"
#import "CurvedSpacesRenderer.h"
#import "GeometryGamesModel.h"
#import "GeometryGamesUtilities-Common.h"
#import "GeometryGamesUtilities-Mac.h"
#import <QuartzCore/QuartzCore.h>	//	for CAMetalLayer


//	When the user presses the left- or right-arrow key,
//	how much should the aperture change?
#define APERTURE_INCREMENT	0.125


//	Privately-declared properties and methods
@interface CurvedSpacesGraphicsViewMac()
- (void)handleRotation:(NSRotationGestureRecognizer *)aRotationGestureRecognizer;
- (void)handlePinch:(NSMagnificationGestureRecognizer *)aPinchGestureRecognizer;
@end


@implementation CurvedSpacesGraphicsViewMac
{
}


- (id)initWithModel:(GeometryGamesModel *)aModel frame:(NSRect)aFrame
{
	NSRotationGestureRecognizer			*theRotationGestureRecognizer;
	NSMagnificationGestureRecognizer	*thePinchGestureRecognizer;

	self = [super initWithModel:aModel frame:aFrame];
	if (self != nil)
	{
		CALayer	*theLayer;

		//	The superclass has already installed a CAMetalLayer.
		theLayer = [self layer];

		GEOMETRY_GAMES_ASSERT(
			[theLayer isKindOfClass:[CAMetalLayer class]],
			"Internal error:  layer is not CAMetalLayer");

		itsRenderer	= [[CurvedSpacesRenderer alloc]
						initWithLayer:	(CAMetalLayer *)theLayer
						device:			CurrentMainScreenGPU()
						multisampling:	true
						depthBuffer:	true
						stencilBuffer:	false];

		theRotationGestureRecognizer = [[NSRotationGestureRecognizer alloc]
										initWithTarget:	self
										action:			@selector(handleRotation:)];
		[theRotationGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:theRotationGestureRecognizer];

		thePinchGestureRecognizer = [[NSMagnificationGestureRecognizer alloc]
										initWithTarget:	self
										action:			@selector(handlePinch:)];
		[thePinchGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:thePinchGestureRecognizer];
	}
	return self;
}


- (void)keyDown:(NSEvent *)anEvent
{
	ModelData	*md	= NULL;

	switch ([anEvent keyCode])
	{
		case LEFT_ARROW_KEY:
			[itsModel lockModelData:&md];
			GesturePinch(md, 1.0 - APERTURE_INCREMENT);	//	simulate  inward pinch on computers that lack a trackpad
			[itsModel unlockModelData:&md];
			break;

		case RIGHT_ARROW_KEY:
			[itsModel lockModelData:&md];
			GesturePinch(md, 1.0 + APERTURE_INCREMENT);	//	simulate outward pinch on computers that lack a trackpad
			[itsModel unlockModelData:&md];
			break;

		case DOWN_ARROW_KEY:
			[itsModel lockModelData:&md];
			md->itsUserSpeed -= USER_SPEED_INCREMENT;
			[itsModel unlockModelData:&md];
			break;

		case UP_ARROW_KEY:
			[itsModel lockModelData:&md];
			md->itsUserSpeed += USER_SPEED_INCREMENT;
			[itsModel unlockModelData:&md];
			break;

		case SPACEBAR_KEY:
			[itsModel lockModelData:&md];
			GestureTap(md);
			[itsModel unlockModelData:&md];
			break;

#ifdef START_OUTSIDE
		case ENTER_KEY:
			[itsModel lockModelData:&md];
			if (md->itsViewpoint == ViewpointExtrinsic)
				md->itsViewpoint = ViewpointEntering;
			[itsModel unlockModelData:&md];
			break;
#endif

#ifdef PRINT_USER_BODY_PLACEMENT
		case 35:	//	Key code for the 'p' key, which stands for "print user body placement"
			[itsModel lockModelData:&md];
			double (*m)[4];
			m = md->itsUserBodyPlacement.m;
			printf("\n\tMatrix\ttheInitialPlacementInPDS =\n\t{\n\t\t{\n\t\t\t{%20.17lf, %20.17lf, %20.17lf, %20.17lf},\n\t\t\t{%20.17lf, %20.17lf, %20.17lf, %20.17lf},\n\t\t\t{%20.17lf, %20.17lf, %20.17lf, %20.17lf},\n\t\t\t{%20.17lf, %20.17lf, %20.17lf, %20.17lf}\n\t\t},\n\t\tImagePositive\n\t};\n\n",
				 m[0][0], m[0][1], m[0][2], m[0][3],
				 m[1][0], m[1][1], m[1][2], m[1][3],
				 m[2][0], m[2][1], m[2][2], m[2][3],
				 m[3][0], m[3][1], m[3][2], m[3][3]);
			[itsModel unlockModelData:&md];
			break;
#endif

		default:
		
			//	Ignore the keystoke.
			//	Let the superclass pass it down the responder chain.
			[super keyDown:anEvent];

			//	Process the keystroke normally.
		//	[self interpretKeyEvents:@[anEvent]];

			break;
	}
}


- (void)mouseDragged:(NSEvent *)anEvent
{
	unsigned int	theModifierFlags;
	ModelData		*md				= NULL;

	theModifierFlags = (unsigned int) [anEvent modifierFlags];

	[itsModel lockModelData:&md];
	MouseMoved(	md,
				[self mouseLocation:anEvent],
				[self mouseDisplacement:anEvent],
				theModifierFlags & NSEventModifierFlagShift,	//	rotation or translation
				theModifierFlags & NSEventModifierFlagControl,	//	x and y axes or z axis
				theModifierFlags & NSEventModifierFlagOption);	//	used with CENTERPIECE_DISPLACEMENT only
	[itsModel unlockModelData:&md];
}


#pragma mark -
#pragma mark rotation gesture

- (void)handleRotation:(NSRotationGestureRecognizer *)aRotationGestureRecognizer
{
	ModelData	*md	= NULL;
	double		theAngle;

	switch ([aRotationGestureRecognizer state])
	{
		case NSGestureRecognizerStatePossible:
			break;
		
		case NSGestureRecognizerStateBegan:		//	Respond to the initial rotation when the gesture is first recognized
		case NSGestureRecognizerStateChanged:	//	... as well as to subsequent rotation

			//	An NSRotationGestureRecognizer measures positive angles counterclockwise, as desired.
			theAngle = [aRotationGestureRecognizer rotation];

			[itsModel lockModelData:&md];
			GestureRotate(md, theAngle);
			[itsModel unlockModelData:&md];

			//	We want incremental rotation angles, not cumulative ones,
			//	so reset aRotationGestureRecognizer's rotation angle to zero.
			[aRotationGestureRecognizer setRotation:0.0];

			break;
		
		case NSGestureRecognizerStateEnded:
		case NSGestureRecognizerStateCancelled:
			break;
			
		case NSGestureRecognizerStateFailed:
			break;
	}
}


#pragma mark -
#pragma mark pinch gesture

- (void)handlePinch:(NSMagnificationGestureRecognizer *)aPinchGestureRecognizer
{
	ModelData	*md	= NULL;
	double		theExpansionFactor;

	switch ([aPinchGestureRecognizer state])
	{
		case NSGestureRecognizerStatePossible:
			break;
		
		case NSGestureRecognizerStateBegan:		//	Respond to the initial pinching when the gesture is first recognized
		case NSGestureRecognizerStateChanged:	//	... as well as to subsequent pinching
			
			theExpansionFactor = 1.0 + [aPinchGestureRecognizer magnification];	//	we must add 1.0 manually
			
			[itsModel lockModelData:&md];
			GesturePinch(md, theExpansionFactor);
			[itsModel unlockModelData:&md];

			//	We want incremental pinches, not cumulative ones,
			//	so reset aPinchGestureRecognizer's magnification to 0.0.
			//	This may be important if we'll be interpreting
			//	pinch gestures simultaneously with rotations and/or 2-finger pans.
			//
			[aPinchGestureRecognizer setMagnification:0.0];

			break;
		
		case NSGestureRecognizerStateEnded:
		case NSGestureRecognizerStateCancelled:
			break;
			
		case NSGestureRecognizerStateFailed:
			break;
	}
}


#pragma mark -
#pragma mark NSGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(NSGestureRecognizer *)gestureRecognizer
	shouldRecognizeSimultaneouslyWithGestureRecognizer:(NSGestureRecognizer *)otherGestureRecognizer
{
	//	We use only two gesture recognizers -- an NSRotationGestureRecognizer
	//	and an NSMagnificationGestureRecognizer -- and they should be allowed
	//	to recognize their gestures simultaneously.
	
	return true;
}


@end
