//	CurvedSpacesGraphicsViewiOS.m
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "CurvedSpacesGraphicsViewiOS.h"
#import "CurvedSpacesRenderer.h"
#import "CurvedSpaces-Common.h"
#import "GeometryGamesModel.h"
#import "GeometryGamesUtilities-Common.h"
#import "GeometryGamesUtilities-Mac-iOS.h"


//	handleDeviceMotion attemps to distinguish purposeful device motions
//	from random vibrations that the device might sense while sitting "motionless" on a table.
//	What should the minimum change in the pitch, yaw or roll angle be
//	for a motion to count as purposeful?
//
//		Note:  The threshold of 0.001 is fairly aggressive.
//		It pretty much stops the animation when the device
//		is lying still on a table and the scene isn't changing.
//		The disadvantage is that if the user purposefully
//		rotates the device very slowly (while the scene
//		isn't otherwise changing) half or more of the frames
//		won't get drawn.  In practice that's not a problem,
//		though, because a suppressed frame's pitch, yaw and roll
//		always (by the very nature of the algorithm) differ
//		from the most recently drawn frame's pitch, yaw and roll
//		by less than 0.001, which is hardly perceptable to the human eye.
//
#define INCREMENTAL_PITCH_YAW_ROLL_THRESHOLD	0.001


static void	ConvertCMRotationMatrixToCurvedSpacesMatrix(CMRotationMatrix *aSrc, Matrix *aDst);
static bool	IsTwoFingerGesture(UIGestureRecognizer *aGestureRecognizer);
#ifdef PREPARE_FOR_SCREENSHOT
static void	PrintUserBodyPlacementToConsole(GeometryGamesModel *aModel);
static void	PrintApertureToConsole(GeometryGamesModel *aModel);
#endif


//	Privately-declared methods
@interface CurvedSpacesGraphicsViewiOS()

- (void)handleOneFingerPanGesture:(UIPanGestureRecognizer *)aPanGestureRecognizer;
- (void)handleTwoFingerPanGesture:(UIPanGestureRecognizer *)aPanGestureRecognizer;
- (void)handleRotationGesture:(UIRotationGestureRecognizer *)aRotationGestureRecognizer;
- (void)handlePinchGesture:(UIPinchGestureRecognizer *)aPinchGestureRecognizer;
- (void)handleTapGesture:(UITapGestureRecognizer *)aTapGestureRecognizer;

- (DisplayPoint)gestureLocationAsDisplayPoint:(CGPoint)aGestureLocation;
- (DisplayPointMotion)gestureTranslationAsDisplayPointMotion:(CGPoint)aGestureTranslation;

@end


@implementation CurvedSpacesGraphicsViewiOS
{
	id<CurvedSpacesGraphicsViewDelegate>	itsDelegate;
	
	//	We'll want to respond not to the device's absolute attitude,
	//	but rather to changes in its attitude.  So keep track
	//	of the previous device attitude.
	bool	itsPreviousDeviceAttitudeHasBeenInitialized;
	Matrix	itsPreviousDeviceAttitude;
}


- (id)initWithModel:(GeometryGamesModel *)aModel frame:(CGRect)aFrame
		delegate:(id<CurvedSpacesGraphicsViewDelegate>)aDelegate
{
	UIPanGestureRecognizer		*theOneFingerPanGestureRecognizer,
								*theTwoFingerPanGestureRecognizer;
	UIRotationGestureRecognizer	*theRotationGestureRecognizer;
	UIPinchGestureRecognizer	*thePinchGestureRecognizer;
	UITapGestureRecognizer		*theTapGestureRecognizer;

	self = [super initWithModel:aModel frame:aFrame];
	if (self != nil)
	{
		CALayer	*theLayer;

		theLayer = [self layer];

		GEOMETRY_GAMES_ASSERT(
			[theLayer isKindOfClass:[CAMetalLayer class]],
			"Internal error:  layer is not CAMetalLayer");
		
		itsRenderer = [[CurvedSpacesRenderer alloc]
			initWithLayer:	(CAMetalLayer *)theLayer
			device:			MTLCreateSystemDefaultDevice()	//	on all iOS devices, this is the integrated GPU
			multisampling:	true
			depthBuffer:	true
			stencilBuffer:	false];

		itsDelegate = aDelegate;
		
		//	When the caller initializes this CurvedSpacesGraphicsViewiOS,
		//	the CMMotionManager typically does not yet have any attitude data
		//	available.
		itsPreviousDeviceAttitudeHasBeenInitialized = false;
		MatrixIdentity(&itsPreviousDeviceAttitude);


		theOneFingerPanGestureRecognizer	= [[UIPanGestureRecognizer alloc]
												initWithTarget:	self
												action:			@selector(handleOneFingerPanGesture:)];
		[theOneFingerPanGestureRecognizer setMinimumNumberOfTouches:1];
		[theOneFingerPanGestureRecognizer setMaximumNumberOfTouches:1];
		[theOneFingerPanGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:theOneFingerPanGestureRecognizer];

		theTwoFingerPanGestureRecognizer	= [[UIPanGestureRecognizer alloc]
												initWithTarget:	self
												action:			@selector(handleTwoFingerPanGesture:)];
		[theTwoFingerPanGestureRecognizer setMinimumNumberOfTouches:2];
		[theTwoFingerPanGestureRecognizer setMaximumNumberOfTouches:2];
		[theTwoFingerPanGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:theTwoFingerPanGestureRecognizer];

		theRotationGestureRecognizer		= [[UIRotationGestureRecognizer alloc]
												initWithTarget:	self
												action:			@selector(handleRotationGesture:)];
		[theRotationGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:theRotationGestureRecognizer];

		thePinchGestureRecognizer			= [[UIPinchGestureRecognizer alloc]
												initWithTarget:	self
												action:			@selector(handlePinchGesture:)];
		[thePinchGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:thePinchGestureRecognizer];

		theTapGestureRecognizer				= [[UITapGestureRecognizer alloc]
												initWithTarget:	self
												action:			@selector(handleTapGesture:)];
		[theTapGestureRecognizer setNumberOfTapsRequired:1];
		[theTapGestureRecognizer setNumberOfTouchesRequired:1];
		[theTapGestureRecognizer setDelegate:self];
		[self addGestureRecognizer:theTapGestureRecognizer];
	}
	return self;
}

- (void)layoutSubviews
{
	ModelData	*md	= NULL;

	//	In the case of a GeometryGamesGraphicsViewiOS, a call to -layoutSubviews
	//	has absolutely nothing to do with subviews, because the view has no subviews.
	//	Rather the call is telling us that the view's dimensions may have changed.

	//	Let the GeometryGamesGraphicsViewiOS implementation of this method resize the framebuffer.
	[super layoutSubviews];

	//	Request a redraw.
	[itsModel lockModelData:&md];
	md->itsChangeCount++;
	[itsModel unlockModelData:&md];
}


#pragma mark -
#pragma mark touch handling

//	Touch handing code isn't needed, because
//	the gesture recognizers are doing all the work.


#pragma mark -
#pragma mark device motion handling

- (void)handleDeviceMotion:(CMRotationMatrix)aDeviceAttitude
{
	Matrix		theDeviceAttitude;
	Matrix		theOldInverse,
				theIncrementalRotation;	//	in the device's local coordinates
	ModelData	*md	= NULL;

#if (defined(PREPARE_FOR_SCREENSHOT) || defined(MAKE_SCREENSHOTS) || defined(HIGH_RESOLUTION_SCREENSHOT))
	//	Ignore device motion when making screenshots.
	return;
#endif

	//	According to
	//
	//		https://nshipster.com/cmdevicemotion/
	//
	//	and implicitly confirmed at
	//
	//		https://developer.apple.com/documentation/coremotion/getting_processed_device_motion_data/understanding_reference_frames_and_device_attitude?changes=_5&language=objc
	//
	//	CMAttitude uses a coordinate frame attached to the device, namely
	//
	//		the x axis runs left-to-right through the device's left and right edges,
	//		the y axis runs bottom-to-top through the device's bottom and top edges, and
	//		the z axis runs upward from the back side up through the display,
	//
	//	and reports the position of that coordinate frame
	//	relative to some fixed reference frame.

	ConvertCMRotationMatrixToCurvedSpacesMatrix(&aDeviceAttitude, &theDeviceAttitude);

	if ( ! itsPreviousDeviceAttitudeHasBeenInitialized )
	{
		itsPreviousDeviceAttitude = theDeviceAttitude;
		
		itsPreviousDeviceAttitudeHasBeenInitialized = true;
	}
	else	//	itsPreviousDeviceAttitudeHasBeenInitialized
	{
		MatrixGeometricInverse(&itsPreviousDeviceAttitude, &theOldInverse);
		MatrixProduct(&theDeviceAttitude, &theOldInverse, &theIncrementalRotation);

		if (fabs(theIncrementalRotation.m[0][1]) > INCREMENTAL_PITCH_YAW_ROLL_THRESHOLD
		 || fabs(theIncrementalRotation.m[0][2]) > INCREMENTAL_PITCH_YAW_ROLL_THRESHOLD
		 || fabs(theIncrementalRotation.m[1][2]) > INCREMENTAL_PITCH_YAW_ROLL_THRESHOLD)
		{
			[itsModel lockModelData:&md];
			MatrixProduct(&theIncrementalRotation, &md->itsUserBodyPlacement, &md->itsUserBodyPlacement);
			md->itsChangeCount++;
			[itsModel unlockModelData:&md];
			
			itsPreviousDeviceAttitude = theDeviceAttitude;
		}
	}
}


#pragma mark -
#pragma mark gesture handling

- (void)handleOneFingerPanGesture:(UIPanGestureRecognizer *)aPanGestureRecognizer
{
	DisplayPoint		theGestureLocation;
	DisplayPointMotion	theGestureMotion;
	ModelData			*md	= NULL;

	switch ([aPanGestureRecognizer state])
	{
		case UIGestureRecognizerStatePossible:
			break;
		
		case UIGestureRecognizerStateBegan:		//	Respond to the initial pan when the gesture is first recognized
		case UIGestureRecognizerStateChanged:	//	... as well as to subsequent panning
		
			theGestureLocation	= [self gestureLocationAsDisplayPoint:[aPanGestureRecognizer locationInView:self]];
			theGestureMotion	= [self gestureTranslationAsDisplayPointMotion:[aPanGestureRecognizer translationInView:self]];

			[itsModel lockModelData:&md];
			MouseMoved(	md,
				theGestureLocation,
				theGestureMotion,
				false,	//	rotation instead of translation
				false,	//	about x and y axes
				false);
			[itsModel unlockModelData:&md];

			//	We want incremental translations, not cumulative ones,
			//	so reset aPanGestureRecognizer's translationInView to 0.0.
			[aPanGestureRecognizer setTranslation:CGPointZero inView:self];

			break;
		
		case UIGestureRecognizerStateEnded:
#ifdef PREPARE_FOR_SCREENSHOT
			PrintUserBodyPlacementToConsole(itsModel);
#endif
			break;
			
		case UIGestureRecognizerStateCancelled:
			break;
			
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)handleTwoFingerPanGesture:(UIPanGestureRecognizer *)aPanGestureRecognizer
{
	DisplayPoint		theGestureLocation;
	DisplayPointMotion	theGestureMotion;
	ModelData			*md	= NULL;

	switch ([aPanGestureRecognizer state])
	{
		case UIGestureRecognizerStatePossible:
			break;
		
		case UIGestureRecognizerStateBegan:		//	Respond to the initial pan when the gesture is first recognized
		case UIGestureRecognizerStateChanged:	//	... as well as to subsequent panning
		
			theGestureLocation	= [self gestureLocationAsDisplayPoint:[aPanGestureRecognizer locationInView:self]];
			theGestureMotion	= [self gestureTranslationAsDisplayPointMotion:[aPanGestureRecognizer translationInView:self]];

			[itsModel lockModelData:&md];
			MouseMoved(	md,
				theGestureLocation,
				theGestureMotion,
				true,	//	translation instead of rotation
				false,	//	along x and y axes
				false);
			[itsModel unlockModelData:&md];

			//	We want incremental translations, not cumulative ones,
			//	so reset aPanGestureRecognizer's translationInView to 0.0.
			[aPanGestureRecognizer setTranslation:CGPointZero inView:self];

			break;
		
		case UIGestureRecognizerStateEnded:
#ifdef PREPARE_FOR_SCREENSHOT
			PrintUserBodyPlacementToConsole(itsModel);
#endif
			break;
			
		case UIGestureRecognizerStateCancelled:
			break;
			
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)handleRotationGesture:(UIRotationGestureRecognizer *)aRotationGestureRecognizer
{
	ModelData	*md	= NULL;
	double		theAngle;

	switch ([aRotationGestureRecognizer state])
	{
		case UIGestureRecognizerStatePossible:
			break;
		
		case UIGestureRecognizerStateBegan:		//	includes the initial rotation at time of gesture recognition
		case UIGestureRecognizerStateChanged:

			//	A UIRotationGestureRecognizer measures positive angles clockwise.
			//	Negate this to get a counterclockwise rotation angle.
			theAngle = - [aRotationGestureRecognizer rotation];
			
			[itsModel lockModelData:&md];
			GestureRotate(md, theAngle);
			[itsModel unlockModelData:&md];

			//	We want incremental rotation angles, not cumulative ones,
			//	so reset aRotationGestureRecognizer's rotation angle to zero.
			[aRotationGestureRecognizer setRotation:0.0];

			break;
		
		case UIGestureRecognizerStateEnded:
#ifdef PREPARE_FOR_SCREENSHOT
			PrintUserBodyPlacementToConsole(itsModel);
#endif
			break;
			
		case UIGestureRecognizerStateCancelled:
			break;
			
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)handlePinchGesture:(UIPinchGestureRecognizer *)aPinchGestureRecognizer
{
	ModelData	*md	= NULL;
	double		theExpansionFactor;

	switch ([aPinchGestureRecognizer state])
	{
		case UIGestureRecognizerStatePossible:
			break;
		
		case UIGestureRecognizerStateBegan:		//	Respond to the initial pinching when the gesture is first recognized
		case UIGestureRecognizerStateChanged:	//	... as well as to subsequent pinching

			theExpansionFactor = [aPinchGestureRecognizer scale];
			
			[itsModel lockModelData:&md];
			GesturePinch(md, theExpansionFactor);
			[itsModel unlockModelData:&md];

			//	We want incremental pinches, not cumulative ones,
			//	so reset aPinchGestureRecognizer scale to 1.0.
			//	This may be important if we'll be interpreting
			//	pinch gestures simultaneously with rotations and/or 2-finger pans.
			//
			[aPinchGestureRecognizer setScale:1.0];

			break;
		
		case UIGestureRecognizerStateEnded:
#ifdef PREPARE_FOR_SCREENSHOT
			PrintUserBodyPlacementToConsole(itsModel);
			PrintApertureToConsole(itsModel);
#endif
			break;
			
		case UIGestureRecognizerStateCancelled:
			break;
			
		case UIGestureRecognizerStateFailed:
			break;
	}
}

- (void)handleTapGesture:(UITapGestureRecognizer *)aTapGestureRecognizer
{
	ModelData	*md	= NULL;

	switch ([aTapGestureRecognizer state])
	{
		case UIGestureRecognizerStateRecognized:

			[itsModel lockModelData:&md];
			GestureTap(md);
			[itsModel unlockModelData:&md];
			
			[itsDelegate refreshSpeedSlider];

#ifdef PREPARE_FOR_SCREENSHOT
			PrintUserBodyPlacementToConsole(itsModel);
#endif

			break;
		
		default:
			//	We don't expect any other messages,
			//	but if any did arrive we'd ignore them.
			break;
	}
}

- (DisplayPoint)gestureLocationAsDisplayPoint:(CGPoint)aGestureLocation
{
	CGRect			theViewBounds;
	DisplayPoint	theResult;

	//	Both aGestureLocation and theViewBounds should be in points (not pixels),
	//	but either would be fine just so everything is consistent.
	theViewBounds = [self bounds];

	//	Gesture location in coordinates (0,0) to (width, height)
	aGestureLocation.x -= theViewBounds.origin.x;
	aGestureLocation.y -= theViewBounds.origin.y;

	//	Flip from iOS's top-down coordinates
	//	to our DisplayPoint's bottom-up coordinates.
	aGestureLocation.y = theViewBounds.size.height - aGestureLocation.y;
	
	//	Package up the result.
	theResult.itsX			= aGestureLocation.x;
	theResult.itsY			= aGestureLocation.y;
	theResult.itsViewWidth	= theViewBounds.size.width;
	theResult.itsViewHeight	= theViewBounds.size.height;

	return theResult;
}

- (DisplayPointMotion)gestureTranslationAsDisplayPointMotion:(CGPoint)aGestureTranslation
{
	CGRect				theViewBounds;
	DisplayPointMotion	theResult;

	//	Both aGestureTranslation and theViewBounds should be in points (not pixels),
	//	but either would be fine just so everything is consistent.
	theViewBounds = [self bounds];
	
	//	Flip from iOS's top-down coordinates
	//	to our DisplayPointMotion's bottom-up coordinates.
	aGestureTranslation.y = - aGestureTranslation.y;
	
	//	Package up the result.
	theResult.itsDeltaX		= aGestureTranslation.x;
	theResult.itsDeltaY		= aGestureTranslation.y;
	theResult.itsViewWidth	= theViewBounds.size.width;
	theResult.itsViewHeight	= theViewBounds.size.height;

	return theResult;
}


#pragma mark -
#pragma mark UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
	shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	//	Let the 2-finger pan gesture, the rotation gesture
	//	and the pinch gesture act simultaneously.

	return (IsTwoFingerGesture(gestureRecognizer)
	 && IsTwoFingerGesture(otherGestureRecognizer));
}


@end


static void ConvertCMRotationMatrixToCurvedSpacesMatrix(
	CMRotationMatrix	*aSrc,
	Matrix				*aDst)
{
	//	theAttitudeMatrix apparently uses left-to-right matrix action with row-major order
	//	(or, equivalently, right-to-left matrix action with column-major order)
	//	the same as we do, so there's no need to take a transpose here.

	//	Relative to a device held in portrait orientation,
	//	[theAttitude rotationMatrix] uses the frame (right vector, up vector, back vector)
	//	(see illustration at https://nshipster.com/cmdevicemotion/ ).
	//	But we'd rather work with (right vector, up vector, forward vector).
	//	So let's change coordinates to flip the z axis:
	//
	//		( 1  0  0 ) ( m₀₀  m₀₁  m₀₂ ) ( 1  0  0 )
	//		( 0  1  0 ) ( m₁₀  m₁₁  m₁₂ ) ( 0  1  0 )
	//		( 0  0 -1 ) ( m₂₀  m₂₁  m₂₂ ) ( 0  0 -1 )
	//	  =
	//		( 1  0  0 ) ( m₀₀  m₀₁ -m₀₂ ) ( 1  0  0 )
	//		( 0  1  0 ) ( m₁₀  m₁₁ -m₁₂ ) ( 0  1  0 )
	//		( 0  0 -1 ) (-m₂₀ -m₂₁  m₂₂ ) ( 0  0 -1 )
	//


	aDst->m[0][0] =   aSrc->m11;
	aDst->m[0][1] =   aSrc->m12;
	aDst->m[0][2] = - aSrc->m13;
	aDst->m[0][3] =   0.0;

	aDst->m[1][0] =   aSrc->m21;
	aDst->m[1][1] =   aSrc->m22;
	aDst->m[1][2] = - aSrc->m23;
	aDst->m[1][3] =   0.0;

	aDst->m[2][0] = - aSrc->m31;
	aDst->m[2][1] = - aSrc->m32;
	aDst->m[2][2] =   aSrc->m33;
	aDst->m[2][3] =   0.0;

	aDst->m[3][0] =   0.0;
	aDst->m[3][1] =   0.0;
	aDst->m[3][2] =   0.0;
	aDst->m[3][3] =   1.0;

	aDst->itsParity = ImagePositive;
}


static bool IsTwoFingerGesture(
	UIGestureRecognizer	*aGestureRecognizer)
{
	return
	(
		(	[aGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]
		 && [((UIPanGestureRecognizer *)aGestureRecognizer) minimumNumberOfTouches] == 2
		)
	 ||
		[aGestureRecognizer isKindOfClass:[UIRotationGestureRecognizer class]]
	 ||
		[aGestureRecognizer isKindOfClass:[UIPinchGestureRecognizer class]]
	);
}


#ifdef PREPARE_FOR_SCREENSHOT

static void PrintUserBodyPlacementToConsole(
	GeometryGamesModel	*aModel)
{
	ModelData	*md	= NULL;

	[aModel lockModelData:&md];
		printf("md->itsUserBodyPlacement = (Matrix)\n");
		printf("{\n");
		printf("\t{\n");
		for (unsigned int i = 0; i < 4; i++)
		{
			printf("\t\t{%20.17lf, %20.17lf, %20.17lf, %20.17lf}",
				md->itsUserBodyPlacement.m[i][0],
				md->itsUserBodyPlacement.m[i][1],
				md->itsUserBodyPlacement.m[i][2],
				md->itsUserBodyPlacement.m[i][3]);
			printf( i < 3 ? ",\n" : "\n");
		}
		printf("\t},\n");
		printf("\tImagePositive\n");
		printf("};\n\n");
	[aModel unlockModelData:&md];
}

static void PrintApertureToConsole(
	GeometryGamesModel	*aModel)
{
	ModelData	*md	= NULL;

	[aModel lockModelData:&md];
		printf("md->itsAperture = %8.5lf\n\n", md->itsAperture);
	[aModel unlockModelData:&md];
}

#endif
