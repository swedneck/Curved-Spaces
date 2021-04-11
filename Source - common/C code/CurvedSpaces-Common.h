//	CurvedSpaces-Common.h
//
//	Platform-independent definitions for Curved Spaces' internal code.
//	The internal code doesn't know or care what platform (iOS or macOS) it's running on.
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#pragma once

#include "GeometryGames-Common.h"
#include "GeometryGamesMatrix44.h"
#include "GeometryGamesUtilities-SIMD.h"


//	Don't let the user open more than one window at once.  
//	On my 2006 iMac's Radeon X1600, opening 2 or 3 windows 
//	with depth buffers, or 3 or 4 windows without depth buffers, 
//	suddenly brings the computer to a near standstill.
//	On my 2008 MacBook's GeForce 9400M the problem seems not to occur.
//	To enable multiple-window support for testing purposes, 
//	uncomment the following line.
//#define ALLOW_MULTIPLE_WINDOWS
#ifdef ALLOW_MULTIPLE_WINDOWS
#warning ALLOW_MULTIPLE_WINDOWS is enabled
#endif


//	When the 3-torus is first introduced in the Shape of Space lecture,
//	it's nice to start with walls closed and zero speed.
//#define START_STILL
#ifdef START_STILL
#warning START_STILL is enabled
#endif

//	For the Shape of Space presentation it’s nice
//	to be able to move the Earth.  I've suppressed this feature
//	from the public version because it introduces artifacts:
//	1.	The centerpiece may extend beyond the fundamental domain,
//		leading to a "popping" artifact when the fundamental domain
//		passes outside the view pyramid and no longer gets drawn.
//	2.	The centerpiece also suffers a noticeable "popping" artifact
//		when it's near the back of the visible region in the flat case.
//		In such cases the beams also "pop" in theory,
//		but in practice they're not noticeable.
//	3.	The wrong resolution might get used for the spinning Earth,
//		because it's no longer at the center.
//#define CENTERPIECE_DISPLACEMENT
#ifdef CENTERPIECE_DISPLACEMENT
#warning CENTERPIECE_DISPLACEMENT is enabled
#endif

//	For the Shape of Space presentation it’s nice
//	to start outside the dodecahedron and then move in.
//	I’m not sure whether this will work well
//	in the publicly released version or not, because
//	of potential conflicts with existing features.
//	So for now let’s keep it as a compile-time option.
//#define START_OUTSIDE
#ifdef START_OUTSIDE
#warning START_OUTSIDE is enabled
#endif

//	For the Non-Euclidean Billiards talk it's nice to start
//	with the walls mostly open, leaving beams to represent
//	the geodesics (at least in spaces where the beams
//	continue straight through the vertices and out the other side).
//	This option also leaves the observer hidden by default.
//#define NON_EUCLIDEAN_BILLIARDS
#ifdef NON_EUCLIDEAN_BILLIARDS
#warning NON_EUCLIDEAN_BILLIARDS is enabled
#endif

//	To produce a good screenshot, tile deep (can also omit the centerpiece
//	in the home cell to avoid obscuring the view) (might also want
//	to improve the level-of-detail of the spinning Earths)
//	(hmmm... maybe current off-the-shelf settings are OK).
//#define HIGH_RESOLUTION_SCREENSHOT
#ifdef HIGH_RESOLUTION_SCREENSHOT
#warning HIGH_RESOLUTION_SCREENSHOT is enabled
#define SCREENSHOT_FOR_GEOMETRY_GAMES_CURVED_SPACES_PAGE
#endif

//	Use PREPARE_FOR_SCREENSHOT while looking
//	for an aesthetically pleasing view of each space, and then...
//#define PREPARE_FOR_SCREENSHOT
#ifdef PREPARE_FOR_SCREENSHOT
#warning PREPARE_FOR_SCREENSHOT is enabled
#endif
//	...use MAKE_SCREENSHOTS to reproduce the aesthetically pleasing views
//	whenever they're needed for screenshots.
//#define MAKE_SCREENSHOTS
#ifdef MAKE_SCREENSHOTS
#warning MAKE_SCREENSHOTS is enabled
#endif

#if defined(PREPARE_FOR_SCREENSHOT) && defined(MAKE_SCREENSHOTS)
#error You can’t use a previous screenshot configuration while preparing a new one.
#endif


//	A quick-and-dirty hack to show the corkscrew axes
//	in the Hantzsche-Wendt space, for Jon Rogness to use
//	in one of his MathFest 2010 talks.
//#define HANTZSCHE_WENDT_AXES
#ifdef HANTZSCHE_WENDT_AXES
#warning HANTZSCHE_WENDT_AXES is enabled
#endif

//	For the Spazi Finiti or Spin Graphics talks
//	we want to start with beams but no centerpiece.
//	We also want to be able to show Clifford parallels
//	while in a prism space.  And for the Spin Graphics talk
//	we need to show 1, 2 and 3 sets of mutually orthogonal
//	Clifford parallels.
//#define CLIFFORD_FLOWS_FOR_TALKS
#ifdef CLIFFORD_FLOWS_FOR_TALKS
#warning CLIFFORD_FLOWS_FOR_TALKS is enabled
#endif

//	To generate the Tiling View images for the 3rd edition of The Shape of Space.

//#define SHAPE_OF_SPACE_CH_7
#ifdef SHAPE_OF_SPACE_CH_7
#warning SHAPE_OF_SPACE_CH_7 is enabled
#endif

//#define SHAPE_OF_SPACE_CH_15
#ifdef SHAPE_OF_SPACE_CH_15
#warning SHAPE_OF_SPACE_CH_15 is enabled
#endif

//#define SHAPE_OF_SPACE_CH_16	3	//	= 3 or 6, for Figure 16.3 or 16.6
#ifdef SHAPE_OF_SPACE_CH_16
#warning SHAPE_OF_SPACE_CH_16 is enabled
#endif

#if (defined(SHAPE_OF_SPACE_CH_7) || defined(SHAPE_OF_SPACE_CH_15) || defined(SHAPE_OF_SPACE_CH_16))
#define PRINT_USER_BODY_PLACEMENT
#endif


//	π
#define PI	3.14159265358979323846


//	The  up -arrow key decreases the user's speed by USER_SPEED_INCREMENT.
//	The down-arrow key increases the user's speed by USER_SPEED_INCREMENT.
//	The space bar sets the user's speed to zero.
#define USER_SPEED_INCREMENT	0.02
#define MAX_USER_SPEED			0.25


//	Clifford parallels options
typedef enum
{
	CliffordNone,
	CliffordBicolor,
	CliffordCenterlines,
	CliffordOneSet,
	CliffordTwoSets,
	CliffordThreeSets
} CliffordMode;


//	Opaque typedef
typedef struct HEPolyhedron	DirichletDomain;


//	Transparent typedefs

typedef enum
{
	SpaceNone,
    SpaceSpherical,
    SpaceFlat,
    SpaceHyperbolic
} SpaceType;

typedef enum
{
	ImagePositive,	//	not mirror-reversed
	ImageNegative	//	mirror-reversed
} ImageParity;

typedef enum
{
	BoxFull,	//	render into the full clipping box  0  ≤ z ≤  w
	BoxFront,	//	render into the front half         0  ≤ z ≤ w/2
	BoxBack		//	render into the back  half        w/2 ≤ z ≤  w
} ClippingBoxPortion;

typedef struct
{
	double	v[4];
} Vector;

typedef struct
{
	double		m[4][4];
	ImageParity	itsParity;	//	Is determinant positive or negative?
} Matrix;

typedef struct
{
	unsigned int	itsNumMatrices;
	Matrix			*itsMatrices;
} MatrixList;

typedef struct
{
	Matrix			itsMatrix;
	Vector			itsCellCenterInWorldSpace;
	double			itsDistanceCameraToCellCenter;
} Honeycell;
typedef struct
{
	//	A fixed list of the cells, sorted relative
	//	to their distance from the basepoint (0,0,0,1).
	unsigned int	itsNumCells;
	Honeycell		*itsCells;

	//	At render time, we'll let CullAndSortVisibleCells() make a temporary list
	//	of the visible cells and sort them according to their distance
	//	from the observer (near to far).  While it's at it, CullAndSortVisibleCells()
	//	will count how many of those cells will be "plain" (not mirror-reversed)
	//	and how many will be "reflected" (mirror-reversed) after factoring in
	//	the parity of the current view matrix.
	//
	//		itsNumVisibleCells = itsNumVisiblePlainCells + itsNumVisibleReflectedCells
	//
	unsigned int	itsNumVisibleCells,
					itsNumVisiblePlainCells,		//	non mirror-reversed, after accounting for view matrix parity
					itsNumVisibleReflectedCells;	//	    mirror-reversed, after accounting for view matrix parity
	Honeycell		**itsVisibleCells;
} Honeycomb;

typedef enum
{
	CenterpieceNone,
	CenterpieceEarth,
	CenterpieceGalaxy,
	CenterpieceGyroscope
#ifdef SHAPE_OF_SPACE_CH_7
	, CenterpieceCube	//	For figures in Chapter 7 of The Shape of Space
#endif
} CenterpieceType;

#ifdef START_OUTSIDE
typedef enum
{
	ViewpointIntrinsic,		//	normal operation
	ViewpointExtrinsic,		//	external view of fundamental domain
	ViewpointEntering		//	transition from extrinsic to intrinsic
} Viewpoint;
#endif


//	All platform-independent data about the space and
//	how it's displayed live in the ModelData structure.
struct ModelData
{	
	//	When the platform-independent code modifies the ModelData
	//	it increments itsChangeCount as a signal to the platform-dependent code
	//	that it should redraw the view at the next opportunity.
	//
	//		Note:  It's OK to have two or more views of the same ModelData.
	//		Each view keeps its own private variable telling the change count
	//		when the view was last redrawn.
	//
	uint64_t		itsChangeCount;		//	wraparound, while unlikely, would be harmless

	//	Most of the code doesn't need to know the curvature
	//	of space.  However, some parts do, for example
	//	the part that draws the back hemisphere of S³.
	SpaceType		itsSpaceType;
	
	//	For flat and hyperbolic spaces, this flag is ignored.
	bool			itsDrawBackHemisphere;
	
	//	An arbitrary finite set of Clifford parallels
	//	lives most naturally in the 3-sphere,
	//	so enable the Clifford Parallels option only there.
	bool			itsThreeSphereFlag;
	
	//	Everything out to itsHorizonRadius should be visible,
	//	with the fogging reaching pure black just as it reaches itsHorizonRadius.
	//	itsHorizonRadius determines how far out we tile.
	//	Set it carefully to get a good balance between image quality and performance.
	double			itsHorizonRadius;
	
	//	Keep track of the user's placement in the world.  itsUserBodyPlacement moves
	//	the user's body from its default position (0,0,0,1) with right vector (1,0,0,0),
	//	up vector (0,1,0,0) and forward vector (0,0,1,0) to the user's current placement.
	//	It's an element of Isom(S³) = O(4), Isom(E³) or Isom(H³) = O(3,1), according
	//	to whether the space is spherical, flat or hyperbolic.
	Matrix			itsUserBodyPlacement;

	//	How fast is the user moving?
	//	The only sustained momentum is straight forward.
	double			itsUserSpeed;
	
	//	When the user pauses the motion, remember the previous speed.
	double			itsPrePauseUserSpeed;

#ifdef CENTERPIECE_DISPLACEMENT
	//	Keep track of the centerpiece's placement in the world.  The transformation moves
	//	the centerpiece from its default position (0,0,0,1) with right vector (1,0,0,0),
	//	up vector (0,1,0,0) and forward vector (0,0,1,0) to its current placement.
	Matrix			itsCenterpiecePlacement;
#endif
	
	//	Keep a Dirichlet domain for the discrete group.
	//	Assume no group element fixes the origin.
	DirichletDomain	*itsDirichletDomain;
	
	//	Keep a list of all translates of the Dirichlet domain
	//	that sit sufficiently close to the origin.
	//	For a spherical manifold the list will typically include
	//	the whole finite group.  For all manifolds the list
	//	is sorted near-to-far. 
	Honeycomb		*itsHoneycomb;
	
	//	The aperture in each face of the Dirichlet domain may be
	//	fully closed (0.0), fully open (1.0), or anywhere in between.
	//	On a touch screen device or on a laptop with a trackpad,
	//	a pinch gesture adjusts the aperture.
	//	As a fallback, on a computer with no trackpad
	//	the left and right arrow keys adjust it.
	double			itsAperture;
	
	//	Set a flag to let the platform-dependent code
	//	know when it needs to call MakeDirichletMesh()
	//	to re-create the mesh that it uses to represent
	//	the Dirichlet domain with apertures cut into its faces.
	//	That mesh will need to be re-created whenever
	//	either the aperture or the Dirichlet domain itself changes.
	bool			itsDirichletWallsMeshNeedsRefresh;
	
	//	Set a flag to let the platform-dependent code know
	//	when the mesh used to represent the vertex figures
	//	needs to be replaced.  This happens only when
	//	the Dirichlet domain itself changes.
	bool			itsVertexFigureMeshNeedsReplacement;
	
	//	What centerpiece should we display within each translate
	//	of the fundamental cell?
	CenterpieceType	itsCenterpieceType;
	
	//	Let the centerpiece (Earth, galaxy or gyroscope) rotate.
	double			itsRotationAngle;	//	in radians
	
	//	Draw the observer (as a small colored dart,
	//	representing the user's spaceship) ?
	bool			itsShowObserver;
	
	//	Color code the faces?
	bool			itsShowColorCoding;
	
	//	Draw Clifford parallels in spherical spaces?
	CliffordMode	itsCliffordMode;

#ifdef CLIFFORD_FLOWS_FOR_TALKS
	//	Rotate in XY and/or ZW planes?
	bool			itsCliffordFlowXYEnabled,
					itsCliffordFlowZWEnabled;
#endif
	
	//	Draw vertex figures?
	bool			itsShowVertexFigures;
	
	//	Enable fog?
	bool			itsFogFlag;

#ifdef START_OUTSIDE
	//	View the fundamental domain from within, from without,
	//	or somewhere in between?
	Viewpoint		itsViewpoint;

	//	If we’re viewing the fundamental domain from somewhere
	//	in between, how far along are we?
	//		0.0 = ViewpointIntrinsic
	//		1.0 = ViewpointExtrinsic
	double			itsViewpointTransition;
	
	//	Keep the fundamental domain spinning.
	double			itsExtrinsicRotation;
#endif

#ifdef HANTZSCHE_WENDT_AXES
	bool			itsHantzscheWendtSpaceIsLoaded,
					itsShowHantzscheWendtAxes;
#endif

};


//	Color definitions

typedef struct
{
	double	h,
			s,
			l,
			a;
} HSLAColor;

typedef struct
{
	double	r,	//	red,   premultiplied by alpha
			g,	//	green, premultiplied by alpha
			b,	//	blue,  premultiplied by alpha
			a;	//	alpha = opacity
} RGBAColor;


//	Platform-independent global functions

//	in CurvedSpacesProjection.c
extern double		CharacteristicViewSize(double aFrameWidth, double aFrameHeight);
extern void			MakeProjectionMatrix(double aFrameWidth, double aFrameHeight, SpaceType aSpaceType, ClippingBoxPortion aClippingBoxPortion, double aProjectionMatrix[4][4]);

//	in CurvedSpacesOptions.c
extern void			SetCenterpiece(ModelData *md, CenterpieceType aCenterpieceChoice);
extern void			SetShowObserver(ModelData *md, bool aShowObserverChoice);
extern void			SetShowColorCoding(ModelData *md, bool aShowColorCodingChoice);
extern void			SetShowCliffordParallels(ModelData *md, CliffordMode aCliffordMode);
extern void			SetShowVertexFigures(ModelData *md, bool aShowVertexFiguresChoice);
extern void			SetFogFlag(ModelData *md, bool aFogFlag);
#ifdef HANTZSCHE_WENDT_AXES
extern void			SetShowHantzscheWendtAxes(ModelData *md, bool aShowHantzscheWendtAxesChoice);
#endif

//	in CurvedSpacesSimulation.c
extern void			FastGramSchmidt(Matrix *aMatrix, SpaceType aSpaceType);

//	in CurvedSpacesMouse.c
extern void			MouseMoved(ModelData *md, DisplayPoint aMouseLocation, DisplayPointMotion aMouseMotion, bool aTranslationFlag, bool aZAxisFlag, bool aCenterpieceFlag);

//	in CurvedSpacesGestures.c
extern void			GestureRotate(ModelData *md, double anAngle);
extern void			GesturePinch(ModelData *md, double anExpansionFactor);
extern void			GestureTap(ModelData *md);

//	in CurvedSpacesFileIO.c
extern ErrorText	LoadGeneratorFile(ModelData *md, Byte *anInputText);

//	in CurvedSpacesTiling.c
extern ErrorText	ConstructHolonomyGroup(MatrixList *aGeneratorList, double aTilingRadius, MatrixList **aHolonomyGroup);
extern ErrorText	NeedsBackHemisphere(MatrixList *aHolonomyGroup, SpaceType aSpaceType, bool *aDrawBackHemisphereFlag);

//	in CurvedSpacesDirichlet.c
extern ErrorText	ConstructDirichletDomain(MatrixList *aHolonomyGroup, DirichletDomain **aDirichletDomain);
extern void			FreeDirichletDomain(DirichletDomain **aDirichletDomain);
extern double		DirichletDomainOutradius(DirichletDomain *aDirichletDomain);
extern void			StayInDirichletDomain(DirichletDomain *aDirichletDomain, Matrix *aPlacement);
extern ErrorText	ConstructHoneycomb(MatrixList *aHolonomyGroup, DirichletDomain *aDirichletDomain, Honeycomb **aHoneycomb);
extern void			FreeHoneycomb(Honeycomb **aHoneycomb);
extern void			MakeDirichletMesh(DirichletDomain *aDirichletDomain, double aCurrentAperture, bool aShowColorCoding,
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets, unsigned int (**someMeshFacets)[3]);
extern void			FreeDirichletMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets, unsigned int (**someMeshFacets)[3]);
extern void			MakeVertexFigureMesh(DirichletDomain *aDirichletDomain,
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets, unsigned int (**someMeshFacets)[3]);
extern void			FreeVertexFigureMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets, unsigned int (**someMeshFacets)[3]);
extern void			CullAndSortVisibleCells(Honeycomb *aHoneycomb, Matrix *aViewMatrix, double anImageWidth, double anImageHeight,
						double aHorizonRadius, double aDirichletDomainRadius, SpaceType aSpaceType);
#ifdef START_OUTSIDE
extern void			SelectFirstCellOnly(Honeycomb *aHoneycomb);
#endif

//	in CurvedSpacesSphere.c
extern void			MakeSphereMesh(double aRadius, unsigned int aNumSubdivisions, const double aColor[4],
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets,unsigned int (**someMeshFacets)[3]);
extern void			FreeSphereMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets,unsigned int (**someMeshFacets)[3]);

//	in CurvedSpacesGyroscope.c
extern void			MakeGyroscopeMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets,unsigned int (**someMeshFacets)[3]);
extern void			FreeGyroscopeMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets,unsigned int (**someMeshFacets)[3]);

#ifdef SHAPE_OF_SPACE_CH_7
//	in CurvedSpacesCube.c
extern void			MakeCubeMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets,unsigned int (**someMeshFacets)[3]);
extern void			FreeCubeMesh(
						unsigned int *aNumMeshVertices, double (**someMeshVertexPositions)[4], double (**someMeshVertexTexCoords)[3], double (**someMeshVertexColors)[4],
						unsigned int *aNumMeshFacets,unsigned int (**someMeshFacets)[3]);
#endif

//	in CurvedSpacesMatrices.c
extern void			MatrixIdentity(Matrix *aMatrix);
extern bool			MatrixIsIdentity(Matrix *aMatrix);
extern void			MatrixAntipodalMap(Matrix *aMatrix);
extern void			MatrixTranslation(Matrix *aMatrix, SpaceType aSpaceType, double dx, double dy, double dz);
extern void			MatrixRotation(Matrix *aMatrix, double da, double db, double dc);
extern void			MatrixGeometricInverse(Matrix *aMatrix, Matrix *anInverse);
extern double		MatrixDeterminant(Matrix *aMatrix);
extern void			VectorTernaryCrossProduct(Vector *aFactorA, Vector *aFactorB, Vector *aFactorC, Vector *aProduct);
extern bool			MatrixEquality(Matrix *aMatrixA, Matrix *aMatrixB, double anEpsilon);
extern void			MatrixProduct(const Matrix *aMatrixA, const Matrix *aMatrixB, Matrix *aProduct);
extern void			VectorNegate(Vector *aVector, Vector *aNegation);
extern void			VectorSum(Vector *aVectorA, Vector *aVectorB, Vector *aSum);
extern void			VectorDifference(Vector *aVectorA, Vector *aVectorB, Vector *aDifference);
extern void			VectorInterpolate(Vector *aVectorA, Vector *aVectorB, double t, Vector *aResult);
extern double		VectorDotProduct(Vector *aVectorA, Vector *aVectorB);
extern ErrorText	VectorNormalize(Vector *aRawVector, SpaceType aSpaceType, Vector *aNormalizedVector);
extern double		VectorGeometricDistance(Vector *aVector);
extern double		VectorGeometricDistance2(Vector *aVectorA, Vector *aVectorB);
extern void			VectorTimesMatrix(Vector *aVector, Matrix *aMatrix, Vector *aProduct);
extern void			ScalarTimesVector(double aScalar, Vector *aVector, Vector *aProduct);
extern MatrixList	*AllocateMatrixList(unsigned int aNumMatrices);
extern void			FreeMatrixList(MatrixList **aMatrixList);

//	in CurvedSpacesSafeMath.c
extern double		SafeAcos(double x);
extern double		SafeAcosh(double x);

//	in CurvedSpacesColors.c
extern void			HSLAtoRGBA(HSLAColor *anHSLAColor, RGBAColor *anRGBAColor);
