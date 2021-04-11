//	CurvedSpacesSimulation.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include <math.h>

//	How much can the simulation advance in one frame?
#define MAX_FRAME_PERIOD				0.1

//	How quickly should the aperture open?
#define APERTURE_VELOCITY				0.25

//	How fast is the galaxy, Earth or gyroscope rotating?
#ifdef CENTERPIECE_DISPLACEMENT
#define CENTERPIECE_ANGULAR_VELOCITY	0.2		//	radians/second
#else
#define CENTERPIECE_ANGULAR_VELOCITY	0.1		//	radians/second
#endif

#ifdef START_OUTSIDE

//	How many seconds should the viewpoint transition take?
#define	VIEWPOINT_TRANSITION_TIME		8.0

//	How fast should the extrinsically viewed
//	fundamental polyhedron rotate?
#define EXTRINSIC_ROTATION_RATE			0.25	//	radians per second

#endif

#ifdef CLIFFORD_FLOWS_FOR_TALKS
//	How fast is the Clifford flow?
#define CLIFFORD_FLOW_RATE	0.125
#endif


static void		UpdateUserPlacement(ModelData *md, double aFramePeriod);
static void		UpdateCenterpieceRotation(ModelData *md, double aFramePeriod);
#ifdef START_OUTSIDE
static void		UpdateExtrinsicRotation(ModelData *md, double aFramePeriod);
static void		UpdateViewpoint(ModelData *md, double aFramePeriod);
#endif


bool SimulationWantsUpdates(
	ModelData	*md)
{
	return
	(
		md->itsSpaceType != SpaceNone
	 &&
		(	md->itsUserSpeed != 0.0
#if defined(SHAPE_OF_SPACE_CH_7)
		//	Better not to rotate the centerpiece,
		//	to keep all the figures consistent.
#elif defined(SHAPE_OF_SPACE_CH_16)
		//	No huge problem if the centerpiece rotates,
		//	but maybe better that it not.
#else
		 || md->itsCenterpieceType != CenterpieceNone
#endif
#ifdef CLIFFORD_FLOWS_FOR_TALKS
		 || md->itsCliffordFlowXYEnabled
		 || md->itsCliffordFlowZWEnabled
#endif
		)
	);
}


void SimulationUpdate(
	ModelData	*md,
	double		aFramePeriod)
{
	//	If some external delay suspends the animation for a few seconds
	//	(for example if the user holds down a menu) we'll receive
	//	a huge frame period.  To avoid a discontinuous jump,
	//	limit the frame period to MAX_FRAME_PERIOD.
	//	This limit should also have the desirable effect of slowing the animation
	//	on systems with humble GPUs and very slow frame rates.
	if (aFramePeriod > MAX_FRAME_PERIOD)
		aFramePeriod = MAX_FRAME_PERIOD;

	//	Update all types of motion, and anything else that's changing.
#if (defined(SHAPE_OF_SPACE_CH_7) || defined(SHAPE_OF_SPACE_CH_16))
	//	Suppress centerpiece rotations
#else
	UpdateCenterpieceRotation(md, aFramePeriod);
#endif
#ifdef START_OUTSIDE
	if (md->itsViewpoint == ViewpointIntrinsic)
		UpdateUserPlacement(md, aFramePeriod);
	else
		UpdateExtrinsicRotation(md, aFramePeriod);
	UpdateViewpoint(md, aFramePeriod);
#else
	UpdateUserPlacement(md, aFramePeriod);
#endif
	
	//	The UI-specific code will need to redraw the scene.
	md->itsChangeCount++;
}


static void UpdateCenterpieceRotation(
	ModelData	*md,
	double		aFramePeriod)
{
	//	Rotate the centerpiece (Earth, galaxy or gyroscope).

	md->itsRotationAngle -= aFramePeriod * CENTERPIECE_ANGULAR_VELOCITY;

	if (md->itsRotationAngle <  0.0)
		md->itsRotationAngle += 2*PI;
}


static void UpdateUserPlacement(
	ModelData	*md,
	double		aFramePeriod)
{
	double	d;	//	distance
	Matrix	theIncrement;
#ifdef CLIFFORD_FLOWS_FOR_TALKS
	Matrix	theFlow;
	double	theFlowAngle,
			c,
			s;
#endif

	//	How far forward should we move the user?
	d = md->itsUserSpeed * aFramePeriod;

	//	Express the motion as a matrix.
	//	(If we wanted to avoid the transcendental functions
	//	we could probably get away with the linear approximations
	//	sin(d) ≈ d and cos(d) ≈ 1, letting Gram-Schmidt clean things
	//	up for us, but this function is unlikely to be a bottleneck
	//	so for now let's use the full version.)
	MatrixIdentity(&theIncrement);
	switch (md->itsSpaceType)
	{
		case SpaceSpherical:
			theIncrement.m[2][2] =  cos(d);	theIncrement.m[2][3] = -sin(d);
			theIncrement.m[3][2] =  sin(d);	theIncrement.m[3][3] =  cos(d);
			break;

		case SpaceFlat:
			theIncrement.m[3][2] = d;
			break;

		case SpaceHyperbolic:
			theIncrement.m[2][2] = cosh(d);	theIncrement.m[2][3] = sinh(d);
			theIncrement.m[3][2] = sinh(d);	theIncrement.m[3][3] = cosh(d);
			break;

		default:	//	should never occur
			break;
	}

	//	Move the observer's body.
	MatrixProduct(&theIncrement, &md->itsUserBodyPlacement, &md->itsUserBodyPlacement);

#ifdef CLIFFORD_FLOWS_FOR_TALKS
	//	itsUserBodyPlacement   moves the camera in world space.
	//	itsUserBodyPlacement⁻¹ moves world in camera space.
	//	To realize a Clifford flow, rotate the world by a flow matrix F
	//	before applying itsUserBodyPlacement⁻¹,
	//
	//		theFlow⋅itsUserBodyPlacement⁻¹
	//
	//	The equivalent user placement is thus
	//
	//		itsUserBodyPlacement⋅theFlow⁻¹
	//
	if (md->itsCliffordFlowXYEnabled
	 || md->itsCliffordFlowZWEnabled)
	{
		MatrixIdentity(&theFlow);
		theFlowAngle = CLIFFORD_FLOW_RATE * aFramePeriod;
		c = cos(theFlowAngle);
		s = sin(theFlowAngle);
		if (md->itsCliffordFlowXYEnabled)
		{
			theFlow.m[0][0] =  c;	theFlow.m[0][1] =  s;
			theFlow.m[1][0] = -s;	theFlow.m[1][1] =  c;
		}
		if (md->itsCliffordFlowZWEnabled)
		{
			theFlow.m[2][2] =  c;	theFlow.m[2][3] =  s;
			theFlow.m[3][2] = -s;	theFlow.m[3][3] =  c;
		}
		MatrixProduct(&md->itsUserBodyPlacement, &theFlow, &md->itsUserBodyPlacement);
	}
#endif

	//	Stay within the central image of the fundamental domain.
	StayInDirichletDomain(md->itsDirichletDomain, &md->itsUserBodyPlacement);

	//	Keep numerical errors from accumulating, so we stay
	//	in Isom(S³) = O(4), Isom(E³) or Isom(H³) = O(3,1).
	FastGramSchmidt(&md->itsUserBodyPlacement, md->itsSpaceType);
}


void FastGramSchmidt(
	Matrix		*aMatrix,
	SpaceType	aSpaceType)
{
	//	Numerical errors can accumulate and force aMatrix "out of round",
	//	in the sense that its rows are no longer orthonormal.
	//	This effect is small in spherical and flat spaces,
	//	but can be significant in hyperbolic spaces, especially
	//	if the camera travels far from the origin.

	//	The Gram-Schmidt process consists of rescaling each row to restore
	//	unit length, and subtracting small multiples of one row from another
	//	to restore orthogonality.  Here we carry out a first-order approximation
	//	to the Gram-Schmidt process.  That is, we normalize each row
	//	to unit length, but then assume that the subsequent orthogonalization step
	//	doesn't spoil the unit length.  This assumption will be well satisfied
	//	because small first order changes orthogonal to a given vector affect
	//	its length only to second order.

	double			(*theMetricPair)[4],
					*theMetric,
					*theRow,
					theInnerProduct,
					theFactor;
	unsigned int	i,
					j,
					k;

	static double	theMetricChoices[3][2][4] =
					{
						//	spherical
						{
							{+1, +1, +1, +1},
							{+1, +1, +1, +1}
						},
						//	flat
						{
							{+1, +1, +1,  0},	//	horizontal metric
							{ 0,  0,  0, +1}	//	vertical metric
						},
						//	hyperbolic
						{
							{+1, +1, +1, -1},	//	for spacelike vectors
							{-1, -1, -1, +1}	//	for timelike vectors
						}
					};

	//	Select an appropriate pair of metric coefficient sets.
	switch (aSpaceType)
	{
		case SpaceSpherical:	theMetricPair = theMetricChoices[0];	break;
		case SpaceFlat:			theMetricPair = theMetricChoices[1];	break;
		case SpaceHyperbolic:	theMetricPair = theMetricChoices[2];	break;
		default:				return;	//	should never occur
	}

	//	Normalize each row to unit length.
	for (i = 0; i < 4; i++)
	{
		theRow = aMatrix->m[i];

		theMetric = theMetricPair[i == 3 ? 1 : 0];

		theInnerProduct = 0.0;
		for (j = 0; j < 4; j++)
			theInnerProduct += theMetric[j] * theRow[j] * theRow[j];

		theFactor = 1.0 / sqrt(theInnerProduct);
		for (j = 0; j < 4; j++)
			theRow[j] *= theFactor;
	}

	//	Make the rows orthogonal.
	for (i = 4; i-- > 0; )	//	leaves the last row untouched
	{
		theMetric = theMetricPair[i == 3 ? 1 : 0];

		for (j = i; j-- > 0; )
		{
			theInnerProduct = 0.0;
			for (k = 0; k < 4; k++)
				theInnerProduct += theMetric[k] * aMatrix->m[i][k] * aMatrix->m[j][k];

			for (k = 0; k < 4; k++)
				aMatrix->m[j][k] -= theInnerProduct * aMatrix->m[i][k];
		}
	}
}


#ifdef START_OUTSIDE

static void UpdateExtrinsicRotation(
	ModelData	*md,
	double		aFramePeriod)
{
	double	theSpeed;

	//	Gradually slow the rotation as we enter
	//	the fundamental polyhedron.
	theSpeed = md->itsViewpointTransition * EXTRINSIC_ROTATION_RATE;

	md->itsExtrinsicRotation += aFramePeriod * theSpeed;

	if (md->itsExtrinsicRotation >= 2*PI)
		md->itsExtrinsicRotation -= 2*PI;

	MatrixRotation(	&md->itsUserBodyPlacement,
					(2.0/3.0) * md->itsExtrinsicRotation,
					(2.0/3.0) * md->itsExtrinsicRotation,
					(1.0/3.0) * md->itsExtrinsicRotation);
}


static void UpdateViewpoint(
	ModelData	*md,
	double		aFramePeriod)
{
	if (md->itsViewpoint == ViewpointEntering)
	{
		md->itsViewpointTransition -= aFramePeriod / VIEWPOINT_TRANSITION_TIME;
		if (md->itsViewpointTransition <= 0.0)
		{
			md->itsViewpointTransition	= 0.0;
			md->itsViewpoint			= ViewpointIntrinsic;
		}
	}
}

#endif


uint64_t GetChangeCount(ModelData *md)
{
	return md->itsChangeCount;
}
