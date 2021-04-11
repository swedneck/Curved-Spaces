//	CurvedSpacesMouse.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include "GeometryGamesUtilities-Common.h"
#include <math.h>


void MouseMoved(
	ModelData			*md,
	DisplayPoint		aMouseLocation,
	DisplayPointMotion	aMouseMotion,
	bool				aTranslationFlag,	//	requests translation (instead of rotation)
	bool				aZAxisFlag,			//	applies motion to z axis (instead of x and y axes)
	bool				aCenterpieceFlag)	//	for use with CENTERPIECE_DISPLACEMENT only
{
	double	theCharacteristicSize,	//	distance that subtends a 45° angle from view's center (same units as in aMouseMotion)
			theConversionFactor,
			x,
			y,
			dx,
			dy,
			theTranslationDistanceX,
			theTranslationDistanceY,
			dTheta,
			dPhi;
	Matrix	theIncrement;

	if (aMouseMotion.itsViewWidth  <= 0.0
	 || aMouseMotion.itsViewHeight <= 0.0
	 || aMouseMotion.itsViewWidth  != aMouseLocation.itsViewWidth
	 || aMouseMotion.itsViewHeight != aMouseLocation.itsViewHeight)
	{
		return;
	}

	//	The characteristic view size is the distance in the view,
	//	measured from the center of the view outwards,
	//	that subtends a 45° angle in the observer's field-of-view.
	theCharacteristicSize = CharacteristicViewSize(aMouseMotion.itsViewWidth, aMouseMotion.itsViewHeight);
	GEOMETRY_GAMES_ASSERT(theCharacteristicSize > 0.0, "nonpositive characteristic size");

	//	Express aMouseLocation and aMouseMotion
	//	as multiples of the characteristic size.
	theConversionFactor = 1.0 / theCharacteristicSize;
	x	= theConversionFactor * (aMouseLocation.itsX  -  0.5 * aMouseLocation.itsViewWidth );
	y	= theConversionFactor * (aMouseLocation.itsY  -  0.5 * aMouseLocation.itsViewHeight);
	dx	= theConversionFactor * aMouseMotion.itsDeltaX;
	dy	= theConversionFactor * aMouseMotion.itsDeltaY;

#ifdef CENTERPIECE_DISPLACEMENT	//	for use in Shape of Space lectures, not for public release
	//	The user typically uses the mouse to drag the scenery as a whole
	//	(equivalently, to drag itsUserBodyPlacement in the opposite direction).
	//	As an exceptional case, if the alt key is down
	//	the mouse motion instead serves to drag the centerpiece.
	if (aCenterpieceFlag)
	{
		//	Interpret the motion relative to the centerpiece's
		//	own local coordinate system.

		//	Fudge alert:
		//	(dx, dy) give a well defined angle, but how that angle
		//	corresponds to a centerpiece translation depends
		//	on how far away the centerpiece is from the observer.
		//	The factor used here was chosen purely for convenience during lectures
		//	-- no effort is made to track the centerpiece exactly.
		theTranslationDistanceX = 2.0 * dx;
		theTranslationDistanceY = 2.0 * dy;

		if (aZAxisFlag)
		{
			MatrixTranslation(	&theIncrement,
								md->itsSpaceType,
								0.0,
								0.0,
								theTranslationDistanceY);
		}
		else
		{
			MatrixTranslation(	&theIncrement,
								md->itsSpaceType,
								theTranslationDistanceX,
								theTranslationDistanceY,
								0.0);
		}

		//	Pre-multiply by theIncrement.
		MatrixProduct(&theIncrement, &md->itsCenterpiecePlacement, &md->itsCenterpiecePlacement);

		//	Stay in the fundamental domain.
		if (md->itsDirichletDomain != NULL)
			StayInDirichletDomain(md->itsDirichletDomain, &md->itsCenterpiecePlacement);

		//	Keep numerical errors from accumulating, so we stay
		//	in Isom(S³) = O(4), Isom(E³) or Isom(H³) = O(3,1).
		FastGramSchmidt(&md->itsCenterpiecePlacement, md->itsSpaceType);
	}
	else
#endif	//	CENTERPIECE_DISPLACEMENT
	{
		//	Allow full six-degrees-of-freedom navigation.
		if (aTranslationFlag)
		{
			//	Translate

			//	Fudge alert:
			//	(dx, dy) give a well defined angle, but how that angle
			//	corresponds to a translation distance depends on how far way
			//	the objects that the user's focusing on are.
			//	In practice, multiplying by 1 seems to work well
			//	for the sample spaces in the Curved Spaces library.
			//
			//	Note:
			//		Use -dx and -dy to translate the scenery,
			//		or  +dx and +dy to translate the observer.
			//
			theTranslationDistanceX = -1.0 * dx;
			theTranslationDistanceY = -1.0 * dy;

			if (aZAxisFlag)
			{
				MatrixTranslation(	&theIncrement,
									md->itsSpaceType,
									0.0,
									0.0,
									theTranslationDistanceY);
			}
			else
			{
				MatrixTranslation(	&theIncrement,
									md->itsSpaceType,
									theTranslationDistanceX,
									theTranslationDistanceY,
									0.0);
			}
		}
		else	//	! aTranslationFlag
		{
			//	Rotate
			
			dTheta	= atan(x + dx) - atan(x);
			dPhi	= atan(y + dy) - atan(y);

			if (aZAxisFlag)
			{
				MatrixRotation(	&theIncrement,
								0.0,
								0.0,
								dTheta);
			}
			else
			{
				MatrixRotation(	&theIncrement,
								dPhi,
								- dTheta,
								0.0);
			}
		}

		MatrixProduct(&theIncrement, &md->itsUserBodyPlacement, &md->itsUserBodyPlacement);

		//	Keep numerical errors from accumulating, so we stay
		//	in Isom(S³) = O(4), Isom(E³) or Isom(H³) = O(3,1).
		FastGramSchmidt(&md->itsUserBodyPlacement, md->itsSpaceType);
	}
	
	//	Ask the idle-time routine to redraw the scene.
	md->itsChangeCount++;
}
