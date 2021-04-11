//	CurvedSpacesInit.c
//
//	Initializes the ModelData.
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include "GeometryGamesLocalization.h"


const Char16 *GetLanguageFileBaseName(void)
{
	return u"CurvedSpaces";
}

unsigned int SizeOfModelData(void)
{
	return sizeof(ModelData);
}

void SetUpModelData(ModelData *md)
{
	md->itsChangeCount			= 0;

	md->itsSpaceType			= SpaceNone;
	md->itsDrawBackHemisphere	= false;
	md->itsThreeSphereFlag		= false;
	md->itsHorizonRadius		= 0.0;	//	LoadGenerators() will set the horizon radius.
	MatrixIdentity(&md->itsUserBodyPlacement);
	md->itsUserSpeed			= 0.0;	//	LoadGenerators() will set the speed.
	md->itsPrePauseUserSpeed	= 0.0;

#ifdef CENTERPIECE_DISPLACEMENT
	MatrixIdentity(&md->itsCenterpiecePlacement);
#endif

	md->itsDirichletDomain		= NULL;
	md->itsHoneycomb			= NULL;

#if defined(START_STILL)
	md->itsAperture				= 0.00;
	md->itsCenterpieceType		= CenterpieceEarth;
#elif defined(CENTERPIECE_DISPLACEMENT)
	md->itsAperture				= 0.00;
	md->itsCenterpieceType		= CenterpieceEarth;
#elif defined(START_OUTSIDE)
	md->itsAperture				= 0.00;
	md->itsCenterpieceType		= CenterpieceGalaxy;
#elif defined(NON_EUCLIDEAN_BILLIARDS)
	md->itsAperture				= 0.875;
	md->itsCenterpieceType		= CenterpieceNone;
#elif defined(CLIFFORD_FLOWS_FOR_TALKS)
	md->itsAperture				= 0.9375;
	md->itsCenterpieceType		= CenterpieceNone;
#elif defined(HIGH_RESOLUTION_SCREENSHOT)
	md->itsAperture				= 0.875;
#ifdef SCREENSHOT_FOR_GEOMETRY_GAMES_CURVED_SPACES_PAGE
	md->itsCenterpieceType		= CenterpieceGalaxy;
#else
	md->itsCenterpieceType		= CenterpieceNone;
#endif	//	SCREENSHOT_FOR_GEOMETRY_GAMES_CURVED_SPACES_PAGE
#elif defined(SHAPE_OF_SPACE_CH_7)
	md->itsAperture				= 1.000;
	md->itsCenterpieceType		= CenterpieceCube;
#elif defined(SHAPE_OF_SPACE_CH_15)
	md->itsAperture				= 0.954;
	md->itsCenterpieceType		= CenterpieceNone;
#elif (SHAPE_OF_SPACE_CH_16 == 3)	//	Fig 16.3
	md->itsAperture				= 0.875;
	md->itsCenterpieceType		= CenterpieceEarth;
#elif (SHAPE_OF_SPACE_CH_16 == 6)	//	Fig 16.6
	md->itsAperture				= 0.954;
	md->itsCenterpieceType		= CenterpieceGalaxy;
#else
	md->itsAperture				= 0.25;
	md->itsCenterpieceType		= CenterpieceEarth;
#endif

	md->itsDirichletWallsMeshNeedsRefresh	= true;	//	no mesh is present at launch
	md->itsVertexFigureMeshNeedsReplacement	= true;	//	no mesh is present at launch

#if (SHAPE_OF_SPACE_CH_16 == 3)
	md->itsRotationAngle		= 0.5 * PI;	//	Let the most visible Earth show something other than just the Pacific.
											//	Note that because EARTH_SPEED is 2.0, setting itsRotationAngle to π/2
											//	sets the effective Earth rotation to π.
#else
	md->itsRotationAngle		= 0.0;
#endif

#if    defined(START_STILL)					\
	|| defined(CENTERPIECE_DISPLACEMENT)	\
	|| defined(START_OUTSIDE)				\
	|| defined(NON_EUCLIDEAN_BILLIARDS)		\
	|| defined(HANTZSCHE_WENDT_AXES)		\
	|| defined(CLIFFORD_FLOWS_FOR_TALKS)	\
	|| defined(HIGH_RESOLUTION_SCREENSHOT)	\
	|| defined(SHAPE_OF_SPACE_CH_7)			\
	|| defined(SHAPE_OF_SPACE_CH_15)		\
	|| defined(SHAPE_OF_SPACE_CH_16)
	md->itsShowObserver			= false;
#else
	md->itsShowObserver			= true;
#endif

#ifdef CENTERPIECE_DISPLACEMENT
	md->itsShowColorCoding		= true;
#else
	md->itsShowColorCoding		= false;
#endif

	md->itsCliffordMode			= CliffordNone;
#ifdef CLIFFORD_FLOWS_FOR_TALKS
	md->itsCliffordFlowXYEnabled = false;
	md->itsCliffordFlowZWEnabled = false;
#endif
	md->itsShowVertexFigures	= false;
#if (defined(HIGH_RESOLUTION_SCREENSHOT)	\
  || defined(NON_EUCLIDEAN_BILLIARDS)		\
  || defined(SHAPE_OF_SPACE_CH_15)			\
  || (SHAPE_OF_SPACE_CH_16 == 3))
	md->itsFogFlag				= false;
#else
	md->itsFogFlag				= true;
#endif

#ifdef START_OUTSIDE
	md->itsViewpoint			= ViewpointExtrinsic;
	md->itsViewpointTransition	= 1.0;
	md->itsExtrinsicRotation	= 0.0;
#endif

#ifdef HANTZSCHE_WENDT_AXES
	md->itsHantzscheWendtSpaceIsLoaded	= false;
	md->itsShowHantzscheWendtAxes		= false;
#endif
}

void ShutDownModelData(ModelData *md)
{
	//	Free any allocated memory.
	//	Leave other information untouched.
	FreeDirichletDomain(&md->itsDirichletDomain);
	FreeHoneycomb(&md->itsHoneycomb);
}
