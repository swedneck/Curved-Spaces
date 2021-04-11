//	CurvedSpacesGraphicsViewiOS.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#import "GeometryGamesGraphicsViewiOS.h"
#import <CoreMotion/CMMotionManager.h>


@protocol CurvedSpacesGraphicsViewDelegate
- (void)refreshSpeedSlider;
@end


@interface CurvedSpacesGraphicsViewiOS : GeometryGamesGraphicsViewiOS <UIGestureRecognizerDelegate>
{
}

- (id)initWithModel:(GeometryGamesModel *)aModel frame:(CGRect)aFrame
		delegate:(id<CurvedSpacesGraphicsViewDelegate>)aDelegate;
- (void)layoutSubviews;

- (void)handleDeviceMotion:(CMRotationMatrix)aDeviceAttitude;

//	UIGestureRecognizerDelegate
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
	shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer;

@end
