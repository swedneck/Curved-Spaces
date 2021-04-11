//	CurvedSpacesGestures.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include <math.h>


#define APERTURE_DILATION_CONSTANT	0.5


void GestureRotate(
	ModelData	*md,
	double		anAngle)	//	in radians, measured counterclockwise
{
	double	c,
			s;
	Matrix	theRotation;
	
	//	When the user makes a counterclockwise rotation gesture,
	//	s/he expects to see the scenery rotate counterclockwise,
	//	which corresponds to rotating the observer clockwise
	//	in the space.

	c = cos(anAngle);
	s = sin(anAngle);

	theRotation.m[0][0] =  c;  theRotation.m[0][1] = -s;  theRotation.m[0][2] = 0.0; theRotation.m[0][3] = 0.0;
	theRotation.m[1][0] =  s;  theRotation.m[1][1] =  c;  theRotation.m[1][2] = 0.0; theRotation.m[1][3] = 0.0;
	theRotation.m[2][0] = 0.0; theRotation.m[2][1] = 0.0; theRotation.m[2][2] = 1.0; theRotation.m[2][3] = 0.0;
	theRotation.m[3][0] = 0.0; theRotation.m[3][1] = 0.0; theRotation.m[3][2] = 0.0; theRotation.m[3][3] = 1.0;
	
	theRotation.itsParity = ImagePositive;
	
	MatrixProduct(&theRotation, &md->itsUserBodyPlacement, &md->itsUserBodyPlacement);
	
	md->itsChangeCount++;
}

void GesturePinch(
	ModelData	*md,
	double		anExpansionFactor)	//	typically close to 1.0
{
	//	If we simply multiplied itsAperture by anExpansionFactor,
	//	the user could never completely close the aperture,
	//	because itsAperture would asymptotically approach zero
	//	but never reach it.  So instead let's add an amount
	//	proportional to the change in the expansion factor.
	
	md->itsAperture += APERTURE_DILATION_CONSTANT * (anExpansionFactor - 1.0);

	if (md->itsAperture < 0.0)
		md->itsAperture = 0.0;
	if (md->itsAperture > 1.0)
		md->itsAperture = 1.0;

	md->itsDirichletWallsMeshNeedsRefresh = true;
	md->itsChangeCount++;
}

void GestureTap(
	ModelData	*md)
{
	if (md->itsUserSpeed != 0.0)
	{
		//	Remember the current speed and pause the motion.
		md->itsPrePauseUserSpeed	= md->itsUserSpeed;
		md->itsUserSpeed			= 0.0;
	}
	else	//	itsUserSpeed == 0.0
	{
		//	Restore the speed as it was when the user last paused the motion.
		md->itsUserSpeed			= md->itsPrePauseUserSpeed;
		md->itsPrePauseUserSpeed	= 0.0;
	}
	
	//	No need to increment md->itsChangeCount.
	//	The caller will refresh the speed slider.
}
