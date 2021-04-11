//	CurvedSpacesProjection.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include "GeometryGamesUtilities-Common.h"
#include <math.h>	//	for sqrt()


//	As explained in the comment accompanying the definition of DART_INNER_WIDTH,
//	W_NEAR_CLIP should be at least 1/(DART_INNER_WIDTH/2) ≈ 500.
//	However, it shouldn't be unnecessarily large, to avoid
//	needless loss of precision in the depth buffer.
#define W_NEAR_CLIP			512.0


double CharacteristicViewSize(
	double	aFrameWidth,	//	typically in pixels or points
	double	aFrameHeight)	//	typically in pixels or points
{
	//	Here in Curved Spaces (but not necessarily in the other Geometry Games)
	//	this CharacteristicViewSize() function returns the distance
	//	in the view, measured from the center of the view outwards,
	//	that subtends a 45° angle in the observer's field-of-view.
	//
	//		····+----------+····
	//		    |         /
	//		    |       /
	//		    |     /
	//		    |45°/
	//		    | /
	//			*
	//		observer
	//
	//	The rendering code and the touch-handling code
	//	both rely on this same definition of the characteristic view size,
	//	to guarantee consistenty.

	//	I prefer assigning a ±45° field of view
	//	to the half-width or the half-height, whichever is bigger.
	//	This limits the field of view to 90°, which
	//	limits the perspective distortion caused by the fact
	//	that the user typically views the scene from a point
	//	much further back than the perspectively correct viewpoint.
	//	(If we rendered the scene from the user's true vantage point,
	//	the field of view would be far too narrow.)
	return 0.5 * (aFrameWidth >= aFrameHeight ? aFrameWidth : aFrameHeight);
}

void MakeProjectionMatrix(
	double				aFrameWidth,				//	input, typically in pixels or points
	double				aFrameHeight,				//	input, typically in pixels or points
	SpaceType			aSpaceType,					//	input
	ClippingBoxPortion	aClippingBoxPortion,		//	input
	double				aProjectionMatrix[4][4])	//	output
{
	double	w,	//	view half-width  (typically pixels or points)
			h,	//	view half-height (typically pixels or points)
			c,	//	distance that subtends a 45° angle from view's center (typically pixels or points, same as for w and h)
			n,	//	WNearClip (in intrinsic units)
			f,	//	WFarClip  (in intrinsic units)
			a,	//	1 /(n - f)
			b;	//	n /(n - f)
	
	static double	theFullToFrontCompressionMatrix[4][4] =
					{
						{1.0, 0.0, 0.0, 0.0},
						{0.0, 1.0, 0.0, 0.0},
						{0.0, 0.0, 0.5, 0.0},	//	compress z by 1/2
						{0.0, 0.0, 0.0, 1.0}
					},
					theFullToBackCompressionMatrix[4][4] =
					{
						{1.0, 0.0, 0.0, 0.0},
						{0.0, 1.0, 0.0, 0.0},
						{0.0, 0.0, 0.5, 0.0},	//	compress z by 1/2
						{0.0, 0.0, 0.5, 1.0}	//	and shift 1/2 unit forward
					};
	
	////////////////////////////////////
	//	View wedge
	////////////////////////////////////

	//	If we were flying in Euclidean space and the view were square,
	//	the rays through the points
	//
	//		(±ZNearClip, ±ZNearClip, ZNearClip, 1)
	//	and
	//		(±ZFarClip,  ±ZFarClip,  ZFarClip,  1)
	//
	//	would define a view frustum of angular width and angular height
	//	both equal to ±45°, as seen by an observer at (0,0,0,1).
	//	Typically, though, the view isn't square, but instead has
	//	unequal half-width w and half-height h.
	w = 0.5 * aFrameWidth;
	h = 0.5 * aFrameHeight;
	GEOMETRY_GAMES_ASSERT(w > 0.0 && h > 0.0, "received image of nonpositive size");
	
	//	The characteristic view size is the distance in the view,
	//	measured from the center of the view outwards,
	//	that subtends a 45° angle in the observer's field-of-view.
	//	Note that it's measured in pixels or points, which is OK
	//	because all we really care about are the ratios w/c and h/c.
	c = CharacteristicViewSize(aFrameWidth, aFrameHeight);
	GEOMETRY_GAMES_ASSERT(c > 0.0, "nonpositive characteristic size");

#ifdef SHAPE_OF_SPACE_CH_15
	//	Use a narrower field of view for this figure,
	//	but a touch wider than for Figure 16.3,
	//	so that we can get a full large circle visible
	//	while still including lots of deep images.
	//	(With c *= 2.5, those deep images just pop out of view
	//	when the circle becomes fully visible.)
	c *= 2.3;
#endif
#if (SHAPE_OF_SPACE_CH_16 == 3)
	//	Use a narrower field of view for this figure.
	c *= 2.5;
#endif

	//	For this not-necessarily-square view, the rays through the points
	//
	//		(±ZNearClip*(w/c), ±ZNearClip*(h/c), ZNearClip, 1)
	//	and
	//		(±ZFarClip*(w/c),  ±ZFarClip*(h/c),  ZFarClip,  1)
	//
	//	define a view frustum that subtends the correct angle
	//	in each direction, relative to an observer at the ray through (0,0,0,1).
	//
	//	For reasons that will become clear momentarily,
	//	it's convenient to project those points
	//	from the hyperplane w = 1 to the hyperplane z = 1, giving
	//
	//		(±w/c, ±h/c, 1, 1/ZNearClip)
	//	and
	//		(±w/c, ±h/c, 1, 1/ZFarClip )
	//
	//	The rays defined by those points remain the same,
	//	and the observer remains at the ray through (0,0,0,1).
	//
	//	In preparation for working with spherical and hyperbolic geometry,
	//	let's switch to new variables
	//
	//		WNearClip = 1 / ZNearClip
	//		WFarClip  = 1 / ZFarClip
	//
	//	and write the view frustum's vertices as
	//
	//		(±w/c, ±h/c, 1, WNearClip)
	//	and
	//		(±w/c, ±h/c, 1, WFarClip )
	//
	//	In all three geometries, ZNearClip must be have some
	//	small positive value, so that WNearClip doesn't get too large.
	//	An excessively large value for WNearClip would ultimately
	//	waste depth buffer space.
	
	n = W_NEAR_CLIP;

	//	The preferred value of WFarClip depends on the geometry.
	switch (aSpaceType)
	{
		case SpaceFlat:
			//	In Euclidean geometry we may set WFarClip = 0,
			//	which corresponds to ZFarClip = ∞.
			f = 0.0;
			break;
		
		case SpaceSpherical:
			//	In spherical geometry we may set WFarClip = - WNearClip.
			//	In contrast to the Euclidean "view frustum", these values
			//	define a rectangular block of space that, when projected onto the 3-sphere,
			//	gives a "view banana" as illustrated in figure 12 of the article
			//
			//		Jeff Weeks, Real-time rendering in curved spaces,
			//		IEEE Computer Graphics & Applications 22 No. 6 (Nov-Dec 2002) 90-99
			//
			//	If you don't have access to that article, just take the rectangle
			//	with vertices at (±1, 1, ±8) and project it onto the unit sphere,
			//	and you'll see why it's called a "view banana".
			f = -n;
			break;
		
		case SpaceHyperbolic:
			//	In hyperbolic geometry we may set WFarClip = 1, because all
			//	of hyperbolic space is contained within a lightcone of angle 45°.
			f = 1.0;
			break;
		
		case SpaceNone:
			f = 0.0;
			break;
	}
	
	////////////////////////////////////
	//	Clipping wedge
	////////////////////////////////////

	//	Metal (and also DX12 and Vulkan) clip to the wedge
	//
	//		-w ≤ x ≤ w
	//		-w ≤ y ≤ w
	//		 0 ≤ z ≤ w
	//
	//	The intersection of this wedge with the hyperplane at w = 1
	//	is a box with vertices at
	//
	//		(±1, ±1, 0, 1)
	//	and
	//		(±1, ±1, 1, 1)
	//
	//	The observer sits on the ray through (0,0,-1,0).
	
	////////////////////////////////////
	//	Projection matrix
	////////////////////////////////////

	//	We want to construct a projection matrix that takes the view wedge
	//	whose edges are the rays through
	//
	//			(±w/c, ±h/c, 1, WNearClip)
	//		and
	//			(±w/c, ±h/c, 1, WFarClip )
	//
	//	to the clipping wedge whose edges are the rays through
	//
	//			(±1, ±1, 0, 1)
	//		and
	//			(±1, ±1, 1, 1)
	//
	//	Note that we require only that each ray on the view wedge
	//	map to the desired ray on the clipping wedge -- the given points
	//	do not have to match exactly.  For example, it's OK to map
	//	(w/h, h/w, 1, WNearClip) to any positive multiple of (1, 1, 0, 1),
	//	it doesn't have to map to (1, 1, 0, 1) exactly.
	//
	//	To construct the required matrix, let's "follow our nose"
	//	and construct it as the product of several factors.
	//	[Note that here, as throughout the Geometry Games source code, we let
	//	matrices act using the left-to-right (row vector)(matrix) convention,
	//	not the right-to-left (matrix)(column vector) convention.]

	//	Factor #1
	//
	//		The quarter turn matrix
	//
	//			1  0  0  0
	//			0  1  0  0
	//			0  0  0  1
	//			0  0 -1  0
	//
	//		rotates
	//			(±w/c, ±h/c, 1, WNearClip) and
	//			(±w/c, ±h/c, 1, WFarClip )
	//		to
	//			(±w/c, ±h/c, -WNearClip, 1) and
	//			(±w/c, ±h/c, -WFarClip,  1)
	//
	
	//	Factor #2
	//
	//		The xy dilation
	//
	//			c/w  0   0   0
	//			 0  c/h  0   0
	//			 0   0   1   0
	//			 0   0   0   1
	//
	//		stretches or shrinks
	//			(±w/c, ±h/c, -WNearClip, 1) and
	//			(±w/c, ±h/c, -WFarClip,  1)
	//		to
	//			(±1, ±1, -WNearClip, 1) and
	//			(±1, ±1, -WFarClip,  1)
	//
	
	//	Factor #3
	//
	//		The z dilation
	//
	//			1  0  0  0
	//			0  1  0  0
	//			0  0  a  0
	//			0  0  0  1
	//
	//		with a = 1/(WNearClip - WFarClip), stretches or shrinks
	//		the box to have unit length in the z direction, taking
	//			(±1, ±1, -WNearClip, 1) and
	//			(±1, ±1, -WFarClip,  1)
	//		to
	//			(±1, ±1, -WNearClip/(WNearClip - WFarClip), 1) and
	//			(±1, ±1, -WFarClip /(WNearClip - WFarClip), 1)
	//
	a = 1.0 / (n - f);

	//	Factor #4
	//
	//		The shear
	//
	//			1  0  0  0
	//			0  1  0  0
	//			0  0  1  0
	//			0  0  b  1
	//
	//		with b = WNearClip / (WNearClip - WFarClip),
	//		translates the hyperplane w = 1 just the right amount to take
	//			(±1, ±1, -WNearClip/(WNearClip - WFarClip), 1) and
	//			(±1, ±1, -WFarClip /(WNearClip - WFarClip), 1)
	//		to
	//			(±1, ±1, 0, 1) and
	//			(±1, ±1, 1, 1)
	//
	//		as required.
	//
	b = n / (n - f);

	//	The projection matrix is the product (taken left-to-right!)
	//	of those four factors:
	//
	//			( 1  0  0  0 )( c/w  0   0   0 )( 1  0  0  0 )( 1  0  0  0 )
	//			( 0  1  0  0 )(  0  c/h  0   0 )( 0  1  0  0 )( 0  1  0  0 )
	//			( 0  0  0  1 )(  0   0   1   0 )( 0  0  a  0 )( 0  0  1  0 )
	//			( 0  0 -1  0 )(  0   0   0   1 )( 0  0  0  1 )( 0  0  b  1 )
	//		=
	//			( c/w  0   0   0 )
	//			(  0  c/h  0   0 )
	//			(  0   0   b   1 )
	//			(  0   0  -a   0 )
	//

	aProjectionMatrix[0][0] = c / w;
	aProjectionMatrix[0][1] = 0.0;
	aProjectionMatrix[0][2] = 0.0;
	aProjectionMatrix[0][3] = 0.0;
	
	aProjectionMatrix[1][0] = 0.0;
	aProjectionMatrix[1][1] = c / h;
	aProjectionMatrix[1][2] = 0.0;
	aProjectionMatrix[1][3] = 0.0;
	
	aProjectionMatrix[2][0] = 0.0;
	aProjectionMatrix[2][1] = 0.0;
	aProjectionMatrix[2][2] = b;
	aProjectionMatrix[2][3] = 1.0;
	
	aProjectionMatrix[3][0] = 0.0;
	aProjectionMatrix[3][1] = 0.0;
	aProjectionMatrix[3][2] = -a;
	aProjectionMatrix[3][3] = 0.0;

	//	Most spherical manifolds yield tilings that are symmetrical
	//	under the antipodal map, in which case there's no need to draw
	//	the contents of the back hemisphere.  However, for tilings lacking
	//	antipodal symmetry (such as those arising from odd-order lens spaces)
	//	we do need to draw the back hemisphere as well as the front one.
	//	To do this, two modifications are required:
	//
	//	a.	We adjust the projection matrix to draw
	//		the front hemisphere into the front half of the clipping box (  0  ≤ z ≤ w/2 )
	//		the  back hemisphere into the  back half of the clipping box ( w/2 ≤ z ≤  w  ).
	//
	//	b.	For the back hemisphere, the caller applies the antipodal map
	//		to all back-hemisphere scenery to bring it to an equivalent position
	//		in the front hemisphere.
	//
	switch (aClippingBoxPortion)
	{
		case BoxFull:
			//	Use the full clipping box, as computed above.
			break;
		
		case BoxFront:
			//	Compress the results of the current rendering pass
			//	into the front half of the clipping box.
			Matrix44Product(aProjectionMatrix, theFullToFrontCompressionMatrix, aProjectionMatrix);
			break;
		
		case BoxBack:
			//	Compress the results of the current rendering pass
			//	into the back half of the clipping box.
			Matrix44Product(aProjectionMatrix, theFullToBackCompressionMatrix,  aProjectionMatrix);
			break;
	}
}
