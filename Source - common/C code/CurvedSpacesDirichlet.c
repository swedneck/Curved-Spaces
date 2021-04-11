//	CurvedSpacesDirichlet.c
//
//	Given a set of matrix generators, construct a Dirichlet domain.
//	The geometry may be spherical, Euclidean or hyperbolic,
//	but no group element may fix the origin.
//
//	The algorithm works in a projective context in which
//	rays from the origin represent the Dirichlet domain's vertices,
//	planes through the origin represent lines containing
//		the Dirichlet domain's edges, and
//	hyperplanes through the origin represent planes containing
//		the Dirichlet domain's faces.
//
//	For convenience we may visualize this space as the unit 3-sphere,
//	because each ray from the origin determines a unique point on S³.
//
//	The Dirichlet domain's basepoint sits at (0,0,0,1).
//
//	The geometry (spherical, Euclidean or hyperbolic) comes into play
//	only briefly, when deciding what halfspace a given matrix represents.
//	Thereafter the construction is geometry-independent,
//	because it's simply a matter of intersecting halfspaces.
//	[HMMM... THERE COULD BE SOME MORE SUBTLE ISSUES WITH VERTEX NORMALIZATION.]
//
//	Note that in the hyperbolic case, this projective model
//	includes the region outside the lightcone, which corresponds
//	to a region beyond the usual hyperbolic sphere-at-infinity
//	(the latter being the lightcone itself).  But as long as a given
//	Dirichlet domain sits within the lightcone (possibly with
//	vertices on the lightcone) everything will work great.
//	In particular, this model makes it easy to work with Dirichlet
//	domains for cusped manifolds, although of course when the user
//	flies down into the cusp, s/he will see past the finite available
//	portion of the tiling.
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt


#include "CurvedSpaces-Common.h"
#include "GeometryGamesUtilities-Common.h"
#include <stddef.h>	//	for offsetof()
#include <math.h>
#include <stdlib.h>	//	for qsort()


//	Three vectors will be considered linearly independent iff their
//	ternary cross product has squared length at least PLANARITY_EPSILON
//	(meaning length at least √PLANARITY_EPSILON).  Use a fairly large value
//	here -- the low-index matrices should be blatently independent.
#define PLANARITY_EPSILON		1e-4

//	A fourth hyperplane normal will be considered linearly independent
//	iff it avoids the (antipodal) endpoints of the banana defined by
//	the first three hyperplanes.  Numerical accuracy should be good
//	and the linear independence should be robust, so any plausible value
//	should work OK here.  (Famous last words?)
#define HYPERPLANARITY_EPSILON	1e-4

//	How precisely do we expect to be able to infer the order
//	of a cyclic matrix?
#define ORDER_EPSILON			1e-6

//	How well must a vertex satisfy a halfspace equation
//	to be considered lying on that halfspace's boundary?
//	(I HAVEN'T THOUGHT CAREFULLY ABOUT THIS VALUE.)
#define VERTEX_HALFSPACE_EPSILON	1e-6

//	Matching faces should have equal matrices to pretty high precision.
//	Nevertheless, we can safely choose a large value here, since *all*
//	matrix entries must agree to that precision.
#define MATE_MATRIX_EPSILON		1e-6

//	Make sure we're well clear of a face before applying
//	a face-pairing matrix to stay within the fundamental domain.
//	In particular, if we happen to run *along* a face,
//	we don't want to be flipping back and forth.
#define RESTORING_EPSILON		1e-8

//	How large should a vertex figure be?
#define VERTEX_FIGURE_SIZE		0.1	//	in radians of S³

//	How large a hole should get cut the face of a vertex figure?
#define VERTEX_FIGURE_CUTOUT	0.7	//	as fraction of face size

//	How many times should the face texture repeat across a single quad?
#define FACE_TEXTURE_MULTIPLE_PLAIN	6
#define FACE_TEXTURE_MULTIPLE_WOOD	1


//	__cdecl isn't defined or needed on macOS,
//	so make it disappear from our callback function prototypes.
#ifndef __cdecl
#define __cdecl
#endif


typedef enum
{
	VertexInsideHalfspace,
	VertexOnBoundary,
	VertexOutsideHalfspace
} VertexVsHalfspace;


//	Use a half-edge data structure to represent a Dirichlet domain.
//	The half-edge data structure is easier to work with than
//	the winged-edge data structure used in Curved Spaces 1.0.
//
//	Orientation conventions
//
//	One may orient the faces all clockwise or all counterclockwise,
//	relative to my standard left-handed coordinate system.
//	The documentation accompanying the following typedefs allows
//	for both possibilities:   the present Curved Spaces code orients
//	faces counterclockwise as seen from *inside* the polyhedron
//	(as the end-user will see them), which is the same as orienting
//	them clockwise as seen from *outside* the polyhedron
//	(as the programmer tends to visualize them while writing the code);
//	future polyhedron-viewing software may wish to reverse the convention
//	if the end-user will view the polyhedra from the outside rather than
//	the inside.
//
//	September 2018
//		When migrating from OpenGL to Metal, I left the orientation convention
//		on the Dirichlet domain the same (faces go counterclockwise
//		when viewed from inside the polyhedron in a left-handed coordinate system),
//		but reversed the orientation convention on the mesh faces that comprise
//		the Dirichlet domain's walls and vertex figures (the mesh faces now
//		go clockise when viewed from inside the polyhedron in a left-handed
//		coordinate system, for consistency with Metal's default clockwise winding direction.)
//

typedef struct HEVertex
{
	//	The projective approach (see comments at top of file)
	//	represents a vertex as a ray from the origin.
	//	For most purposes we need only its direction, not its length.
	//	Eventually, though, we might need its length as well,
	//	for example when determining suitable fog parameters.
	Vector				itsRawPosition,			//	normalized to unit 3-sphere at end of algorithm
						itsNormalizedPosition;	//	normalized relative to SpaceType

	//	Knowing a single adjacent half edge gives easy access to them all.
	//	The given half edge starts at this vertex and points away from it.
	struct HEHalfEdge	*itsOutboundHalfEdge;

	//	IntersectWithHalfspace() evaluates each halfspace inequality on each vertex
	//	and stores the result temporarily in itsHalfspaceStatus.
	//	Otherwise this field is unused and undefined.
	VertexVsHalfspace	itsHalfspaceStatus;

	//	The center of a face of the vertex figure.
	//	Please see the explanation of vertex figures in HEHalfEdge below.
	Vector				itsCenterPoint;

	//	The Dirichlet domain keeps all vertices on a NULL-terminated linked list.
	struct HEVertex		*itsNext;

} HEVertex;

typedef struct HEHalfEdge
{
	//	The vertex this half edge points to.
	struct HEVertex		*itsTip;

	//	The other half of the given edge, pointing in the opposite direction.
	//	As viewed from outside the polyhedron, with faces oriented
	//	clockwise (resp. counterclockwise) the two half edges look like
	//	traffic in Europe or the U.S. (resp. Australia or Japan),
	//	assuming a left-handed {x,y,z} coordinate system.
	struct HEHalfEdge	*itsMate;

	//	Traverse the adjacent face clockwise (resp. counter-clockwise),
	//	as viewed from outside the polyhedron.
	struct HEHalfEdge	*itsCycle;

	//	The face that itsCycle traverses lies to the right (resp. left)
	//	of the edge, as viewed from outside the polyhedron.
	struct HEFace		*itsFace;
	
	//	When we draw a face with a window cut out from its center,
	//	we'll need to compute texture coordinates for the window's vertices.
	//	To do this, we'll need to know the dimensions of the triangle whose base 
	//	is the present HalfEdge and whose vertex is the face's center.
	double				itsBase,		//	normalized so largest base has length 1
						itsAltitude;	//	normalized so largest base has length 1

	//	IntersectWithHalfspace() uses a temporary flag to mark half edges
	//	for deletion.	Thereafter itsDeletionFlag is unused and undefined.
	bool				itsDeletionFlag;

	//	Vertex figures are normally not shown, but if the user requests them,
	//	draw them as a framework.  That is, at each vertex of the fundamental
	//	polyhedron, draw the corresponding face of the vertex figure, but with
	//	a hollow center.  In other words, draw the face as a polyhedral annulus.
	//	Each "outer point" of the annulus sits on the outbound half-edge
	//	emanating from the given vertex of the fundamental polyhedron,
	//	while each "inner point" is interpolated between the outer point
	//	and the center of the face of the vertex figure.
	Vector				itsOuterPoint,
						itsInnerPoint;

	//	The Dirichlet domain keeps all half edges on a NULL-terminated linked list.
	struct HEHalfEdge	*itsNext;

} HEHalfEdge;

typedef struct HEFace
{
	//	Knowing a single adjacent half edge gives easy access to them all.
	//	The adjacent half edges all point clockwise (resp. counter-clockwise)
	//	around the face.
	struct HEHalfEdge	*itsHalfEdge;

	//	The Dirichlet domain is the intersection of halfspaces
	//
	//		ax + by + cz + dw ≤ 0
	//
	//	Let itsHalfspace record the coefficients (a,b,c,d) for the given face.
	Vector				itsHalfspace;

	//	Record the defining matrix.
	Matrix				itsMatrix;

	//	A face and its mate will have the same color.
	unsigned int		itsColorIndex;		//	used only temporarily
	RGBAColor			itsColorRGBA;		//	color as {αr, αg, αb, α}
	double				itsColorGreyscale;	//	color as greyscale

	//	Record the face center.
	//
	//		If we ever wanted to support vertices-at-infinity
	//		(for example to allow "ideal polyhedra" in hyperbolic space,
	//		and chimney or slab spaces in Euclidean space)
	//		we'd need to normalize the face center to the 3-sphere,
	//		to facilitate interpolating from the face center to vertices-at-infinity.
	//		Alas I never pursued that idea far enough to find a suitable
	//		solution for the texture coordinates on such vertices,
	//		so itsRawCenter is currently unused, and vertices-at-infinity
	//		remain unsupported.
	//
	Vector				itsRawCenter,			//	normalized to unit 3-sphere [UNUSED IN CURRENT VERSION]
						itsNormalizedCenter;	//	normalized relative to SpaceType

	//	IntersectWithHalfspace() uses a temporary flag to mark faces
	//	for deletion.	Thereafter itsDeletionFlag is unused and undefined.
	bool				itsDeletionFlag;

	//	The Dirichlet domain keeps all faces on a NULL-terminated linked list.
	struct HEFace		*itsNext;

} HEFace;

struct HEPolyhedron
{
	//	Keep vertices, half edges and faces on NULL-terminated linked lists.
	HEVertex		*itsVertexList;
	HEHalfEdge		*itsHalfEdgeList;
	HEFace			*itsFaceList;
	
	//	For convenience, record the space type and the outradius.
	SpaceType		itsSpaceType;
	double			itsOutradius;
};


static ErrorText			MakeBanana(Matrix *aMatrixA, Matrix *aMatrixB, Matrix *aMatrixC, DirichletDomain **aDirichletDomain);
static ErrorText			MakeLens(Matrix *aMatrixA, Matrix *aMatrixB, DirichletDomain **aDirichletDomain);
static void					MakeHalfspaceInequality(Matrix *aMatrix, Vector *anInequality);
static ErrorText			IntersectWithHalfspace(DirichletDomain *aDirichletDomain, Matrix *aMatrix);
static void					AssignFaceColors(DirichletDomain *aDirichletDomain);
static void					ComputeFaceCenters(DirichletDomain *aDirichletDomain);
static void					ComputeWallDimensions(DirichletDomain *aDirichletDomain);
static ErrorText			ComputeVertexFigures(DirichletDomain *aDirichletDomain);
static void					ComputeOutradius(DirichletDomain *aDirichletDomain);
static Honeycomb			*AllocateHoneycomb(unsigned int aNumCells, unsigned int aNumVertices);
static void					MakeCullingHyperplanes(double anImageWidth, double anImageHeight, double someCullingPlanes[4][4]);
static bool					BoundingSphereIntersectsViewFrustum(double aCellCenterInCameraSpace[4], double anAdjustedDirichletDomainRadius, double someCullingPlanes[4][4]);
static __cdecl signed int	CompareCellCenterDistances(const void *p1, const void *p2);


ErrorText ConstructDirichletDomain(
	MatrixList		*aHolonomyGroup,
	DirichletDomain	**aDirichletDomain)
{
	ErrorText		theErrorMessage	= NULL;
	Vector			theHalfspaceA,
					theHalfspaceB,
					theHalfspaceC,
					theHalfspaceD,
					theCrossProduct;
	unsigned int	theThirdIndex,
					theFourthIndex,
					i;
	HEVertex		*theVertex;

	if (aHolonomyGroup == NULL)
		return u"ConstructDirichletDomain() received a NULL holonomy group.";

	if (*aDirichletDomain != NULL)
		return u"ConstructDirichletDomain() received a non-NULL output location.";

	//	Do we have at least the identity and two other matrices?
	if (aHolonomyGroup->itsNumMatrices < 3)
	{
		//	Special case:  Allow the identity matrix alone,
		//	which represents the 3-sphere, or
		//	{±Id}, which represents projective 3-space.
		//	We'll need the 3-sphere to display Clifford parallels.
		//	(Confession:  This is a hack.  I hope it causes no trouble.)
		if (aHolonomyGroup->itsNumMatrices > 0)	//	{Id} or {±Id}
			return NULL;	//	Leave *aDirichletDomain == NULL, but report no error.
		else
			return u"ConstructDirichletDomain() received no matrices.";
	}

	//	Make sure group element 0 is the identity matrix, as expected.
	if ( ! MatrixIsIdentity(&aHolonomyGroup->itsMatrices[0]) )
		return u"ConstructDirichletDomain() expects the first matrix to be the identity.";

	//	Thinking projectively (as explained at the top of this file)
	//	each matrix determines a halfspace of R⁴ or, equivalently,
	//	a hemisphere of S³.  Just as any two distinct hemispheres of S²
	//	intersect in a 2-sided wedge-shaped sector (a "lune"),
	//	any three independent hemispheres of S³ intersect in a 3-sided
	//	wedge-shaped solid (a "banana") and any four independent hemispheres
	//	intersect in a tetrahedron.  Here "independent" means that
	//	the hemispheres' normal vectors are linearly independent.

	//	Which four group elements should we use?

	//	Ignore group element 0, which is the identity matrix.
	//	Groups elements 1 and 2 should be fine (they can't be colinear
	//	because we assume no group element fixes the basepoint (0,0,0,1)).
	MakeHalfspaceInequality(&aHolonomyGroup->itsMatrices[1], &theHalfspaceA);
	MakeHalfspaceInequality(&aHolonomyGroup->itsMatrices[2], &theHalfspaceB);

	//	For the third group element, use the first one we find that's
	//	not coplanar with the elements 1 and 2.
	for (theThirdIndex = 3; theThirdIndex < aHolonomyGroup->itsNumMatrices; theThirdIndex++)
	{
		MakeHalfspaceInequality(&aHolonomyGroup->itsMatrices[theThirdIndex], &theHalfspaceC);
		VectorTernaryCrossProduct(&theHalfspaceA, &theHalfspaceB, &theHalfspaceC, &theCrossProduct);
		if (fabs(VectorDotProduct(&theCrossProduct, &theCrossProduct)) > PLANARITY_EPSILON)
			break;	//	success!
	}
	if (theThirdIndex < aHolonomyGroup->itsNumMatrices)
	{
		//	Before seeking a fourth independent group element,
		//	construct the banana defined by the first three.
		theErrorMessage = MakeBanana(	&aHolonomyGroup->itsMatrices[1],
										&aHolonomyGroup->itsMatrices[2],
										&aHolonomyGroup->itsMatrices[theThirdIndex],
										aDirichletDomain);
		if (theErrorMessage != NULL)
			goto CleanUpConstructDirichletDomain;

		//	Look for a fourth independent group element.
		for (	theFourthIndex = theThirdIndex + 1;
				theFourthIndex < aHolonomyGroup->itsNumMatrices;
				theFourthIndex++)
		{
			//	We could in principle test for linear independence by computing
			//	the determinant of the four hyperplane vectors.
			//	However it's simpler (and perhaps more numerically robust?)
			//	to test with the fourth hyperplane avoids the two (antipodal)
			//	banana vertices.
			MakeHalfspaceInequality(&aHolonomyGroup->itsMatrices[theFourthIndex], &theHalfspaceD);
			if (fabs(VectorDotProduct(&theHalfspaceD, &(*aDirichletDomain)->itsVertexList->itsRawPosition)) > HYPERPLANARITY_EPSILON)
				break;	//	success!
		}
		if (theFourthIndex < aHolonomyGroup->itsNumMatrices)
		{
			//	Slice the banana with the (independent!) fourth hemisphere
			//	to get a tetrahedron.
			theErrorMessage = IntersectWithHalfspace(*aDirichletDomain, &aHolonomyGroup->itsMatrices[theFourthIndex]);
			if (theErrorMessage != NULL)
				goto CleanUpConstructDirichletDomain;
		}
		else
		{
			//	No independent fourth element was found.
			//	The group defines some sort of chimney-like space,
			//	which the current code does not support.
			//	Even though we've constructed the Dirichlet domain,
			//	the graphics code isn't prepared to draw it.
			theErrorMessage = u"Chimney-like spaces not supported.";
			goto CleanUpConstructDirichletDomain;
		}
	}
	else
	{
		//	We couldn't find three independent group elements,
		//	so most likely we have a lens space or a slab space.
		//	The current code *is* prepared to handle such a space!
		theErrorMessage = MakeLens(	&aHolonomyGroup->itsMatrices[1],
									&aHolonomyGroup->itsMatrices[2],
									aDirichletDomain);
		if (theErrorMessage != NULL)
			goto CleanUpConstructDirichletDomain;
	}

	//	Intersect the initial banana with the halfspace determined
	//	by each matrix in aHolonomyGroup.  For best numerical accuracy
	//	(and least work!) start with the nearest group elements and work
	//	towards the more distance ones.
	//
	//	Technical note:  For large tilings all but the first handful
	//	of group elements will be irrelevant.  If desired one could
	//	modify this code to break the loop when the slicing halfspaces
	//	lie further away than the most distance vertices.
	for (i = 0; i < aHolonomyGroup->itsNumMatrices; i++)
	{
		theErrorMessage = IntersectWithHalfspace(*aDirichletDomain, &aHolonomyGroup->itsMatrices[i]);
		if (theErrorMessage != NULL)
			goto CleanUpConstructDirichletDomain;
	}

	//	Record the space type.
	if (aHolonomyGroup->itsMatrices[1].m[3][3] <  1.0)
		(*aDirichletDomain)->itsSpaceType = SpaceSpherical;
	else
	if (aHolonomyGroup->itsMatrices[1].m[3][3] == 1.0)
		(*aDirichletDomain)->itsSpaceType = SpaceFlat;
	else
		(*aDirichletDomain)->itsSpaceType = SpaceHyperbolic;

	//	Normalize each vertex's position relative to the geometry.
//WILL NEED TO THINK ABOUT THIS STEP WITH VERTICES-AT-INFINITY.
	for (	theVertex = (*aDirichletDomain)->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		theErrorMessage = VectorNormalize(	&theVertex->itsRawPosition,
											(*aDirichletDomain)->itsSpaceType,
											&theVertex->itsNormalizedPosition);
		if (theErrorMessage != NULL)
			goto CleanUpConstructDirichletDomain;
	}

	//	Normalize each vertex's raw position to sit on the unit 3-sphere.
	//	This ignores the space's intrinsic geometry (spherical, flat or
	//	hyperbolic) but provides reasonable interpolation between
	//	finite vertices and vertices-at-infinity.  In addition, it serves
	//	the more prosaic purpose or making it easy to sum vertex positions
	//	to get face centers.
	//
	//	Note:  Unlike (I think) the rest of the algorithm, this step
	//	requires a division.  Consider this if moving to exact arithmetic.
	//	At any rate, the normalization isn't needed for the main algorithm.
	for (	theVertex = (*aDirichletDomain)->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		theErrorMessage = VectorNormalize(	&theVertex->itsRawPosition,
											SpaceSpherical,	//	regardless of true SpaceType
											&theVertex->itsRawPosition);
		if (theErrorMessage != NULL)
			goto CleanUpConstructDirichletDomain;
	}

	//	Assign colors to the Dirichlet domain's faces
	//	so that matching faces have the same color.
	AssignFaceColors(*aDirichletDomain);

	//	Compute the center of each face,
	//	normalized to the unit 3-sphere and to the SpaceType.
	ComputeFaceCenters(*aDirichletDomain);
	
	//	Compute the dimensions of the triangular wedges comprising each face.
	ComputeWallDimensions(*aDirichletDomain);

	//	Compute the faces of the vertex figure(s).
	//	One face of the vertex figure(s) sits at each vertex
	//	of the fundamental polyhedron.
	//	This code relies on the fact that for each vertex,
	//	itsRawPosition has already been normalized to sit on the 3-sphere.
	theErrorMessage = ComputeVertexFigures(*aDirichletDomain);
	if (theErrorMessage != NULL)
		goto CleanUpConstructDirichletDomain;
	
	//	Compute the outradius.
	ComputeOutradius(*aDirichletDomain);
	
CleanUpConstructDirichletDomain:

	if (theErrorMessage != NULL)
		FreeDirichletDomain(aDirichletDomain);

	return theErrorMessage;
}


void FreeDirichletDomain(DirichletDomain **aDirichletDomain)
{
	HEVertex	*theDeadVertex;
	HEHalfEdge	*theDeadHalfEdge;
	HEFace		*theDeadFace;

	if (aDirichletDomain != NULL
	 && *aDirichletDomain != NULL)
	{
		while ((*aDirichletDomain)->itsVertexList != NULL)
		{
			theDeadVertex						= (*aDirichletDomain)->itsVertexList;
			(*aDirichletDomain)->itsVertexList	= theDeadVertex->itsNext;
			FREE_MEMORY(theDeadVertex);
		}

		while ((*aDirichletDomain)->itsHalfEdgeList != NULL)
		{
			theDeadHalfEdge							= (*aDirichletDomain)->itsHalfEdgeList;
			(*aDirichletDomain)->itsHalfEdgeList	= theDeadHalfEdge->itsNext;
			FREE_MEMORY(theDeadHalfEdge);
		}

		while ((*aDirichletDomain)->itsFaceList != NULL)
		{
			theDeadFace							= (*aDirichletDomain)->itsFaceList;
			(*aDirichletDomain)->itsFaceList	= theDeadFace->itsNext;
			FREE_MEMORY(theDeadFace);
		}

		FREE_MEMORY_SAFELY(*aDirichletDomain);
	}
}


static ErrorText MakeBanana(
	Matrix			*aMatrixA,			//	input
	Matrix			*aMatrixB,			//	input
	Matrix			*aMatrixC,			//	input
	DirichletDomain	**aDirichletDomain)	//	output
{
	ErrorText		theErrorMessage			= NULL,
					theOutOfMemoryMessage	= u"Out of memory in MakeBanana().";
	Matrix			*theMatrices[3];
	Vector			theHalfspaces[3];
	HEVertex		*theVertices[2];
	HEHalfEdge		*theHalfEdges[3][2];	//	indexed by which face, coming from which vertex
	HEFace			*theFaces[3];
	unsigned int	i,
					j;

	//	Make sure the output pointer is clear.
	if (*aDirichletDomain != NULL)
		return u"MakeBanana() received a non-NULL output location.";

	//	Put the input matrices on an array.
	theMatrices[0] = aMatrixA;
	theMatrices[1] = aMatrixB;
	theMatrices[2] = aMatrixC;

	//	Each matrix determines a halfspace
	//
	//		ax + by + cz + dw ≤ 0
	//
	for (i = 0; i < 3; i++)
		MakeHalfspaceInequality(theMatrices[i], &theHalfspaces[i]);

	//	Allocate the base DirichletDomain structure.
	//	Initialize its lists to NULL immediately, so everything will be kosher
	//	if we encounter an error later in the construction.
	*aDirichletDomain = (DirichletDomain *) GET_MEMORY(sizeof(DirichletDomain));
	if (*aDirichletDomain == NULL)
	{
		theErrorMessage = theOutOfMemoryMessage;
		goto CleanUpMakeBanana;
	}
	(*aDirichletDomain)->itsVertexList		= NULL;
	(*aDirichletDomain)->itsHalfEdgeList	= NULL;
	(*aDirichletDomain)->itsFaceList		= NULL;

	//	Allocate memory for the new vertices, half edges and faces.
	//	Put them on the Dirichlet domain's linked lists immediately,
	//	so that if anything goes wrong all memory will get freed.
	for (i = 0; i < 2; i++)
	{
		theVertices[i] = (HEVertex *) GET_MEMORY(sizeof(HEVertex));
		if (theVertices[i] == NULL)
		{
			theErrorMessage = theOutOfMemoryMessage;
			goto CleanUpMakeBanana;
		}
		theVertices[i]->itsNext				= (*aDirichletDomain)->itsVertexList;
		(*aDirichletDomain)->itsVertexList	= theVertices[i];
	}
	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
		{
			theHalfEdges[i][j] = (HEHalfEdge *) GET_MEMORY(sizeof(HEHalfEdge));
			if (theHalfEdges[i][j] == NULL)
			{
				theErrorMessage = theOutOfMemoryMessage;
				goto CleanUpMakeBanana;
			}
			theHalfEdges[i][j]->itsNext				= (*aDirichletDomain)->itsHalfEdgeList;
			(*aDirichletDomain)->itsHalfEdgeList	= theHalfEdges[i][j];
		}
	for (i = 0; i < 3; i++)
	{
		theFaces[i] = (HEFace *) GET_MEMORY(sizeof(HEFace));
		if (theFaces[i] == NULL)
		{
			theErrorMessage = theOutOfMemoryMessage;
			goto CleanUpMakeBanana;
		}
		theFaces[i]->itsNext				= (*aDirichletDomain)->itsFaceList;
		(*aDirichletDomain)->itsFaceList	= theFaces[i];
	}

	//	Set up the vertices.

	//	The two vertices are sit antipodally opposite each other.
	//	We must choose which vertex will be +TernaryCrossProduct(...)
	//	and which will be -TernaryCrossProduct(...).  One choice
	//	will yield clockwise-oriented faces while the other choice
	//	yields counterclockwise-oriented faces.  To figure out which
	//	is which, consider the three group elements
	//
	//			(x, y, z) → (x+ε, y,  z )
	//			(x, y, z) → ( x, y+ε, z )
	//			(x, y, z) → ( x,  y, z+ε)
	//
	//	with inequalities x ≤ ε/2, y ≤ ε/2 and z ≤ ε/2, respectively.
	//	The ternary cross product of the coefficient vectors
	//	(1,0,0,-ε/2), (0,1,0,-ε/2) and (0,0,1,-ε/2) comes out to
	//
	//				(-1, -ε/2, -ε/2, -ε/2)
	//
	//	So with the half edge pointers organized as below, we want
	//	the cross product to be vertex 0 (near the south pole (0,0,0,-1))
	//	and it's negative to be vertex 1 (near the north pole (0,0,0,+1))
	//	to give clockwise oriented faces in our left-handed coordinate system.
	//	By continuity, we expect clockwise orientations for all linearly
	//	independent halfspaces (but I should probably think about that
	//	more carefully!).

	VectorTernaryCrossProduct(	&theHalfspaces[0],
								&theHalfspaces[1],
								&theHalfspaces[2],
								&theVertices[0]->itsRawPosition);
	VectorNegate(	&theVertices[0]->itsRawPosition,
					&theVertices[1]->itsRawPosition);

	for (i = 0; i < 2; i++)
	{
		//	Let each vertex see an outbound edge on face 0.
		theVertices[i]->itsOutboundHalfEdge = theHalfEdges[0][i];
	}

	//	Set up the half edges.
	for (i = 0; i < 3; i++)
		for (j = 0; j < 2; j++)
		{
			//	Let theHalfEdges[i][j] run from vertex j to vertex ~j.
			theHalfEdges[i][j]->itsTip = theVertices[!j];

			//	It mate sits on a neighboring face.
			theHalfEdges[i][j]->itsMate = theHalfEdges[(i+1+j)%3][!j];

			//	Two two half edges on each face form their own cycle.
			theHalfEdges[i][j]->itsCycle = theHalfEdges[i][!j];

			//	The edge sees the face.
			theHalfEdges[i][j]->itsFace = theFaces[i];
		}

	//	Set up the faces.
	for (i = 0; i < 3; i++)
	{
		//	The face sees one of its edges.
		theFaces[i]->itsHalfEdge = theHalfEdges[i][0];

		//	Copy the matrix.
		theFaces[i]->itsMatrix = *theMatrices[i];

		//	Set the halfspace inequality.
		theFaces[i]->itsHalfspace = theHalfspaces[i];
	}

CleanUpMakeBanana:

	if (theErrorMessage != NULL)
		FreeDirichletDomain(aDirichletDomain);

	return theErrorMessage;
}


static ErrorText MakeLens(
	Matrix			*aMatrixA,			//	input
	Matrix			*aMatrixB,			//	input
	DirichletDomain	**aDirichletDomain)	//	output
{
	//	This is not a fully general algorithm!
	//	It assumes a central axis passing through the basepoint (0,0,0,1)
	//	and running in the z-direction.  In other words, it assumes
	//	the face planes, whether for a lens or for a slab,
	//	are “parallel” to the xy-plane.

	ErrorText		theErrorMessage			= NULL,
					theOutOfMemoryMessage	= u"Out of memory in MakeLens().";
	unsigned int	n;
	double			theApproximateN;
	HEVertex		**theVertices		= NULL;	//	*theVertices[n]
	HEHalfEdge		*(*theHalfEdges)[2]	= NULL;	//	*theHalfEdges[n][2]
	HEFace			*theFaces[2];
	unsigned int	i,
					j;

	//	Make sure the output pointer is clear.
	if (*aDirichletDomain != NULL)
		return u"MakeLens() received a non-NULL output location.";

	//	The two face planes will meet along the circle
	//	{x² + y² = 1, z² + w² = 0}, which we divide into n segments
	//	(n ≥ 3) in such a way as to respect the group.
	//
	//	Warning:  The determination of n is ad hoc and will
	//	work only with the sorts of matrices we are expecting!
	if (aMatrixA->m[3][3] == 1.0)
	{
		//	Flat space.
		//	n = 4 should work great for the sorts of reflections
		//	and half-turns we are expecting.
		n = 4;
	}
	else if (aMatrixA->m[3][3] < 1.0)
	{
		//	Lens space.
		//	Infer the order from the zw-rotation.
		if (aMatrixA->m[0][2] != 0.0 || aMatrixA->m[0][3] != 0.0
		 || aMatrixA->m[1][2] != 0.0 || aMatrixA->m[1][3] != 0.0
		 || aMatrixA->m[2][0] != 0.0 || aMatrixA->m[2][1] != 0.0
		 || aMatrixA->m[3][0] != 0.0 || aMatrixA->m[3][1] != 0.0)
		{
			theErrorMessage = u"MakeLens() confused by potential lens space.";
			goto CleanUpMakeLens;
		}

		theApproximateN = (2*PI)/fabs(atan2(aMatrixA->m[3][2], aMatrixA->m[3][3]));
		n = (unsigned int)floor(theApproximateN + 0.5);
		if (fabs(theApproximateN - n) > ORDER_EPSILON)
		{
			theErrorMessage = u"MakeLens() couldn't deduce order of potential lens space.";
			goto CleanUpMakeLens;
		}
		if (n < 3)
		{
			theErrorMessage = u"MakeLens() expected a lens space of order at least 3.";
			goto CleanUpMakeLens;
		}
	}
	else
	{
		theErrorMessage = u"MakeLens() can't handle hyperbolic slab spaces.";
		goto CleanUpMakeLens;
	}

	//	Allocate the base DirichletDomain structure.
	//	Initialize its lists to NULL immediately, so everything will be kosher
	//	if we encounter an error later in the construction.
	*aDirichletDomain = (DirichletDomain *) GET_MEMORY(sizeof(DirichletDomain));
	if (*aDirichletDomain == NULL)
	{
		theErrorMessage = theOutOfMemoryMessage;
		goto CleanUpMakeLens;
	}
	(*aDirichletDomain)->itsVertexList		= NULL;
	(*aDirichletDomain)->itsHalfEdgeList	= NULL;
	(*aDirichletDomain)->itsFaceList		= NULL;

	//	Allocate memory for the new vertices, half edges and faces.
	//	Put them on the Dirichlet domain's linked lists immediately,
	//	so that if anything goes wrong all memory will get freed.

	theVertices = (HEVertex **) GET_MEMORY(n * sizeof(HEVertex *));
	if (theVertices == NULL)
	{
		theErrorMessage = theOutOfMemoryMessage;
		goto CleanUpMakeLens;
	}
	for (i = 0; i < n; i++)
	{
		theVertices[i] = (HEVertex *) GET_MEMORY(sizeof(HEVertex));
		if (theVertices[i] == NULL)
		{
			theErrorMessage = theOutOfMemoryMessage;
			goto CleanUpMakeLens;
		}
		theVertices[i]->itsNext				= (*aDirichletDomain)->itsVertexList;
		(*aDirichletDomain)->itsVertexList	= theVertices[i];
	}

	theHalfEdges = (HEHalfEdge *(*)[2]) GET_MEMORY(n * sizeof(HEHalfEdge *[2]));
	if (theHalfEdges == NULL)
	{
		theErrorMessage = theOutOfMemoryMessage;
		goto CleanUpMakeLens;
	}
	for (i = 0; i < n; i++)
		for (j = 0; j < 2; j++)
		{
			theHalfEdges[i][j] = (HEHalfEdge *) GET_MEMORY(sizeof(HEHalfEdge));
			if (theHalfEdges[i][j] == NULL)
			{
				theErrorMessage = theOutOfMemoryMessage;
				goto CleanUpMakeLens;
			}
			theHalfEdges[i][j]->itsNext				= (*aDirichletDomain)->itsHalfEdgeList;
			(*aDirichletDomain)->itsHalfEdgeList	= theHalfEdges[i][j];
		}

	for (i = 0; i < 2; i++)
	{
		theFaces[i] = (HEFace *) GET_MEMORY(sizeof(HEFace));
		if (theFaces[i] == NULL)
		{
			theErrorMessage = theOutOfMemoryMessage;
			goto CleanUpMakeLens;
		}
		theFaces[i]->itsNext				= (*aDirichletDomain)->itsFaceList;
		(*aDirichletDomain)->itsFaceList	= theFaces[i];
	}

	//	Set up the vertices.
	for (i = 0; i < n; i++)
	{
		//	All vertices sit on the xy circle.
		theVertices[i]->itsRawPosition.v[0] = cos(i*2*PI/n);
		theVertices[i]->itsRawPosition.v[1] = sin(i*2*PI/n);
		theVertices[i]->itsRawPosition.v[2] = 0.0;
		theVertices[i]->itsRawPosition.v[3] = 0.0;

		//	Let each vertex see an outbound edge on face 0
		//	(the face sitting at positive z).
		theVertices[i]->itsOutboundHalfEdge = theHalfEdges[i][0];
	}

	//	Set up the half edges.
	for (i = 0; i < n; i++)
	{
		//	Let theHalfEdges[i][j] connect vertex i to vertex (i+1)%n.
		//	On face 0 (at positive z) the half edge runs "forward"
		//	while on face 1 (at negative z) the half edge runs "backwards".
		theHalfEdges[i][0]->itsTip = theVertices[(i+1)%n];
		theHalfEdges[i][1]->itsTip = theVertices[   i   ];

		//	theHalfEdges[i][0] and theHalfEdges[i][1] are mates.
		theHalfEdges[i][0]->itsMate = theHalfEdges[i][1];
		theHalfEdges[i][1]->itsMate = theHalfEdges[i][0];

		//	All half edges should cycle clockwise as seen from the outside.
		theHalfEdges[i][0]->itsCycle = theHalfEdges[( i+1 )%n][0];
		theHalfEdges[i][1]->itsCycle = theHalfEdges[(i+n-1)%n][1];

		//	Note the faces.
		theHalfEdges[i][0]->itsFace = theFaces[0];
		theHalfEdges[i][1]->itsFace = theFaces[1];
	}

	//	Set up the faces.

	//		Each face sees one of its edges.
	theFaces[0]->itsHalfEdge = theHalfEdges[0][0];
	theFaces[1]->itsHalfEdge = theHalfEdges[0][1];

	//		Set the halfspace inequalities.
	MakeHalfspaceInequality(aMatrixA, &theFaces[0]->itsHalfspace);
	MakeHalfspaceInequality(aMatrixB, &theFaces[1]->itsHalfspace);

	//		Copy the matrices.
	theFaces[0]->itsMatrix = *aMatrixA;
	theFaces[1]->itsMatrix = *aMatrixB;

CleanUpMakeLens:

	if (theErrorMessage != NULL)
		FreeDirichletDomain(aDirichletDomain);

	FREE_MEMORY_SAFELY(theHalfEdges);
	FREE_MEMORY_SAFELY(theVertices);

	return theErrorMessage;
}


static void MakeHalfspaceInequality(
	Matrix	*aMatrix,		//	input
	Vector	*anInequality)	//	output
{
	//	Find the halfspace
	//
	//		ax + by + cz + dw ≤ 0
	//
	//	lying midway between the origin (0,0,0,1) and the image
	//	of the origin under the action of aMatrix, and containing the origin.

	double			theLengthSquared;
	unsigned int	i;

	//	The last row of aMatrix gives the image of the basepoint (0,0,0,1).
	//	Compute the difference vector running from the basepoint to that image.
	for (i = 0; i < 4; i++)
		anInequality->v[i] = aMatrix->m[3][i];
	anInequality->v[3] -= 1.0;

	//	Adjust the raw difference vector according to the geometry.

	if (aMatrix->m[3][3] <  1.0)	//	spherical case
	{
		//	no adjustment needed
	}

	if (aMatrix->m[3][3] == 1.0)	//	flat case
	{
		theLengthSquared = VectorDotProduct(anInequality, anInequality);
		anInequality->v[3] = -0.5 * theLengthSquared;
	}

	if (aMatrix->m[3][3] >  1.0)	//	hyperbolic case
	{
		//	mimic Minkowski metric
		anInequality->v[3] = - anInequality->v[3];
	}
}


static ErrorText IntersectWithHalfspace(
	DirichletDomain	*aDirichletDomain,	//	input and output
	Matrix			*aMatrix)			//	input
{
	ErrorText	theMemoryError = u"Memory request failed in IntersectWithHalfspace().";
	Vector		theHalfspace;
	bool		theCutIsNontrivial;
	double		theDotProduct;
	HEVertex	*theVertex,
				*theVertex1,
				*theVertex2,
				*theNewVertex;
	HEHalfEdge	*theHalfEdge1,
				*theHalfEdge1a,
				*theHalfEdge1b,
				*theHalfEdge2,
				*theHalfEdge2a,
				*theHalfEdge2b;
	HEFace		*theFace,
				*theInnerFace,
				*theOuterFace;
	HEHalfEdge	*theGoingOutEdge,
				*theGoingInEdge,
				*theHalfEdge,
				*theInnerHalfEdge,
				*theOuterHalfEdge;
	HEFace		*theNewFace;
	HEVertex	**theVertexPtr,
				*theDeadVertex;
	HEHalfEdge	**theHalfEdgePtr,
				*theDeadHalfEdge;
	HEFace		**theFacePtr,
				*theDeadFace;

	//	Ignore the identity matrix.
	if (MatrixIsIdentity(aMatrix))
		return NULL;	//	Nothing to do, but not an error.

	//	What halfspace does aMatrixDefine?
	MakeHalfspaceInequality(aMatrix, &theHalfspace);

	//	Evaluate the halfspace equation on all vertices
	//	of the provisional Dirichlet domain.
	//	Work with raw (non-normalized) positions for now.

	theCutIsNontrivial = false;

	for (	theVertex = aDirichletDomain->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		theDotProduct = VectorDotProduct(&theHalfspace, &theVertex->itsRawPosition);

		if (theDotProduct < -VERTEX_HALFSPACE_EPSILON)
		{
			theVertex->itsHalfspaceStatus = VertexInsideHalfspace;
		}
		else
		if (theDotProduct > +VERTEX_HALFSPACE_EPSILON)
		{
			theVertex->itsHalfspaceStatus = VertexOutsideHalfspace;
			theCutIsNontrivial = true;
		}
		else
		{
			theVertex->itsHalfspaceStatus = VertexOnBoundary;
		}
	}

	//	If the halfspace fails to cut aDirichletDomain,
	//	nothing needs to be done.
	if ( ! theCutIsNontrivial )
		return NULL;

	//	Wherever the slicing halfspace crosses an edge,
	//	introduce a new vertex at the cut point.
	for (	theHalfEdge1 = aDirichletDomain->itsHalfEdgeList;
			theHalfEdge1 != NULL;
			theHalfEdge1 = theHalfEdge1->itsNext)
	{
		//	Find the mate.
		theHalfEdge2 = theHalfEdge1->itsMate;

		//	Find the adjacent vertices.
		theVertex1 = theHalfEdge1->itsTip;
		theVertex2 = theHalfEdge2->itsTip;

		//	Does the edge get cut?
		//
		//	Technical note:  Consider only the case that
		//	theVertex1 lies inside the halfspace while
		//	theVertex2 lies outside it, so that we'll have
		//	a reliable orientation for the ternary cross product.
		//	The for(;;) loop will eventually consider all half edges,
		//	so all edges will get properly cut.
		if (theVertex1->itsHalfspaceStatus == VertexInsideHalfspace
		 && theVertex2->itsHalfspaceStatus == VertexOutsideHalfspace)
		{
			//	Split the pair of half edges as shown.
			//	(View this diagram in a mirror if you're orienting
			//	your faces counterclockwise rather than clockwise.)
			//
			//		vertex  <---1a---   new   <---1b---  vertex
			//		  1      ---2b---> vertex  ---2a--->   2
			//

			//	Create a new vertex and put it on the list.
			theNewVertex = (HEVertex *) GET_MEMORY(sizeof(HEVertex));
			if (theNewVertex == NULL)
				return theMemoryError;
			theNewVertex->itsNext			= aDirichletDomain->itsVertexList;
			aDirichletDomain->itsVertexList	= theNewVertex;

			//	Set the new vertex's raw position, along with itsHalfspaceStatus.
			//	We'll compute the normalized position when the Dirichlet domain
			//	is complete.
			//
			//	To be honest I'm not sure a priori what ordering of the factors
			//	will give a ternary cross product result with w > 0, but
			//	the result should vary continuously, so if we get it right
			//	for one set of inputs it should remain right for all other
			//	inputs as well.  (Yes, I know, I should give this more
			//	careful thought!)
			VectorTernaryCrossProduct(	&theHalfEdge1->itsFace->itsHalfspace,
										&theHalfEdge2->itsFace->itsHalfspace,
										&theHalfspace,
										&theNewVertex->itsRawPosition);
			theNewVertex->itsHalfspaceStatus = VertexOnBoundary;

			//	We'll set theNewVertex->itsOutboundHalfEdge in a moment,
			//	after the creating the new edges.

			//	Create two new edges and put them on the list.

			theHalfEdge1a = (HEHalfEdge *) GET_MEMORY(sizeof(HEHalfEdge));
			if (theHalfEdge1a == NULL)
				return theMemoryError;
			theHalfEdge1a->itsNext				= aDirichletDomain->itsHalfEdgeList;
			aDirichletDomain->itsHalfEdgeList	= theHalfEdge1a;

			theHalfEdge2a = (HEHalfEdge *) GET_MEMORY(sizeof(HEHalfEdge));
			if (theHalfEdge2a == NULL)
				return theMemoryError;
			theHalfEdge2a->itsNext				= aDirichletDomain->itsHalfEdgeList;
			aDirichletDomain->itsHalfEdgeList	= theHalfEdge2a;

			//	Recycle the existing pair of edges.
			//	Let them become theHalfEdge1b and theHalfEdge2b
			//	(not theHalfEdge1a and theHalfEdge2a) so that other
			//	vertices and edges that used to point to theHalfEdge1
			//	and theHalfEdge2 will remain valid.
			theHalfEdge1b = theHalfEdge1;
			theHalfEdge2b = theHalfEdge2;

			//	Set the tips.
			theHalfEdge1a->itsTip = theHalfEdge1b->itsTip;
			theHalfEdge2a->itsTip = theHalfEdge2b->itsTip;
			theHalfEdge1b->itsTip = theNewVertex;
			theHalfEdge2b->itsTip = theNewVertex;

			//	Set the mates.
			theHalfEdge1a->itsMate	= theHalfEdge2b;
			theHalfEdge2a->itsMate	= theHalfEdge1b;
			theHalfEdge1b->itsMate	= theHalfEdge2a;
			theHalfEdge2b->itsMate	= theHalfEdge1a;

			//	Set the cycles.
			theHalfEdge1a->itsCycle = theHalfEdge1b->itsCycle;
			theHalfEdge2a->itsCycle = theHalfEdge2b->itsCycle;
			theHalfEdge1b->itsCycle = theHalfEdge1a;
			theHalfEdge2b->itsCycle = theHalfEdge2a;

			//	Set the faces.
			theHalfEdge1a->itsFace = theHalfEdge1b->itsFace;
			theHalfEdge2a->itsFace = theHalfEdge2b->itsFace;

			//	The new vertex sits at the tail of both theHalfEdge1a and theHalfEdge2a.
			theNewVertex->itsOutboundHalfEdge = theHalfEdge1a;
		}
	}

	//	Wherever the slicing halfspace crosses a face,
	//	introduce a new edge along the cut.
	//	The required vertices are already in place
	//	from the previous step.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		//	Look for half edges where the face's cycle is about to leave
		//	the halfspace and where it's about to re-enter the halfspace.

		theGoingOutEdge	= NULL;
		theGoingInEdge	= NULL;

		theHalfEdge = theFace->itsHalfEdge;
		do
		{
			if (theHalfEdge->itsTip->itsHalfspaceStatus == VertexOnBoundary)
			{
				switch (theHalfEdge->itsCycle->itsTip->itsHalfspaceStatus)
				{
					case VertexInsideHalfspace:		theGoingInEdge  = theHalfEdge;	break;
					case VertexOnBoundary:											break;
					case VertexOutsideHalfspace:	theGoingOutEdge = theHalfEdge;	break;
				}
			}
			theHalfEdge = theHalfEdge->itsCycle;
		} while (theHalfEdge != theFace->itsHalfEdge);

		//	If the halfspace doesn't cut the face, there's nothing to be done.
		if (theGoingOutEdge	== NULL
		 || theGoingInEdge  == NULL)
			continue;

		//	Create two new half edges and one new face.
		//	The face will eventually be discarded,
		//	but install it anyhow to keep the data structure clean.

		theInnerHalfEdge = (HEHalfEdge *) GET_MEMORY(sizeof(HEHalfEdge));
		if (theInnerHalfEdge == NULL)
			return theMemoryError;
		theInnerHalfEdge->itsNext			= aDirichletDomain->itsHalfEdgeList;
		aDirichletDomain->itsHalfEdgeList	= theInnerHalfEdge;

		theOuterHalfEdge = (HEHalfEdge *) GET_MEMORY(sizeof(HEHalfEdge));
		if (theOuterHalfEdge == NULL)
			return theMemoryError;
		theOuterHalfEdge->itsNext			= aDirichletDomain->itsHalfEdgeList;
		aDirichletDomain->itsHalfEdgeList	= theOuterHalfEdge;

		theOuterFace = (HEFace *) GET_MEMORY(sizeof(HEFace));
		if (theOuterFace == NULL)
			return theMemoryError;
		theOuterFace->itsNext			= aDirichletDomain->itsFaceList;
		aDirichletDomain->itsFaceList	= theOuterFace;

		//	Recycle theFace as theInnerFace.
		theInnerFace = theFace;

		//	Set the tips.
		theInnerHalfEdge->itsTip	= theGoingInEdge->itsTip;
		theOuterHalfEdge->itsTip	= theGoingOutEdge->itsTip;

		//	Set the mates.
		theInnerHalfEdge->itsMate	= theOuterHalfEdge;
		theOuterHalfEdge->itsMate	= theInnerHalfEdge;

		//	Set the cycles.
		theInnerHalfEdge->itsCycle	= theGoingInEdge->itsCycle;
		theOuterHalfEdge->itsCycle	= theGoingOutEdge->itsCycle;
		theGoingOutEdge->itsCycle	= theInnerHalfEdge;
		theGoingInEdge->itsCycle	= theOuterHalfEdge;

		//	Set the inner face (which equals the original face).
		theInnerHalfEdge->itsFace	= theInnerFace;
		theInnerFace->itsHalfEdge	= theInnerHalfEdge;

		//	Set the outer face.
		theHalfEdge = theOuterHalfEdge;
		do
		{
			theHalfEdge->itsFace = theOuterFace;
			theHalfEdge = theHalfEdge->itsCycle;
		} while (theHalfEdge != theOuterHalfEdge);
		theOuterFace->itsHalfEdge	= theOuterHalfEdge;
	}

	//	Allocate a new face to lie on the boundary of the halfspace.
	theNewFace = (HEFace *) GET_MEMORY(sizeof(HEFace));
	if (theNewFace == NULL)
		return theMemoryError;
	theNewFace->itsNext				= aDirichletDomain->itsFaceList;
	aDirichletDomain->itsFaceList	= theNewFace;

	//	Mark for deletion all half edges and faces
	//	that are incident to a VertexOutsideHalfspace.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		theFace->itsDeletionFlag = false;
	}
	for (	theHalfEdge = aDirichletDomain->itsHalfEdgeList;
			theHalfEdge != NULL;
			theHalfEdge = theHalfEdge->itsNext)
	{
		if (theHalfEdge->itsTip->itsHalfspaceStatus				== VertexOutsideHalfspace
		 || theHalfEdge->itsMate->itsTip->itsHalfspaceStatus	== VertexOutsideHalfspace)
		{
			theHalfEdge->itsDeletionFlag			= true;
			theHalfEdge->itsFace->itsDeletionFlag	= true;
		}
		else
			theHalfEdge->itsDeletionFlag			= false;
	}

	//	Make sure all surviving vertices see a surviving half edge.
	for (	theVertex = aDirichletDomain->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		if (theVertex->itsHalfspaceStatus != VertexOutsideHalfspace)
		{
			while (theVertex->itsOutboundHalfEdge->itsDeletionFlag)
			{
				theVertex->itsOutboundHalfEdge
					= theVertex->itsOutboundHalfEdge->itsMate->itsCycle;
			}
		}
	}

	//	Install the new face.
	for (	theHalfEdge = aDirichletDomain->itsHalfEdgeList;
			theHalfEdge != NULL;
			theHalfEdge = theHalfEdge->itsNext)
	{
		if ( ! theHalfEdge->itsDeletionFlag
		 && theHalfEdge->itsFace->itsDeletionFlag)
		{
			theHalfEdge->itsFace	= theNewFace;
			theNewFace->itsHalfEdge	= theHalfEdge;

			while (theHalfEdge->itsCycle->itsDeletionFlag)
			{
				theHalfEdge->itsCycle = theHalfEdge->itsCycle->itsMate->itsCycle;
			}
		}
	}

	//	Set the new face's halfspace inequality and matrix.
	theNewFace->itsHalfspace	= theHalfspace;
	theNewFace->itsMatrix		= *aMatrix;

	//	Delete excluded vertices, half edges and faces.

	theVertexPtr = &aDirichletDomain->itsVertexList;
	while (*theVertexPtr != NULL)
	{
		if ((*theVertexPtr)->itsHalfspaceStatus == VertexOutsideHalfspace)
		{
			theDeadVertex	= *theVertexPtr;
			*theVertexPtr	= theDeadVertex->itsNext;
			FREE_MEMORY(theDeadVertex);
		}
		else
			theVertexPtr = &(*theVertexPtr)->itsNext;
	}

	theHalfEdgePtr = &aDirichletDomain->itsHalfEdgeList;
	while (*theHalfEdgePtr != NULL)
	{
		if ((*theHalfEdgePtr)->itsDeletionFlag)
		{
			theDeadHalfEdge	= *theHalfEdgePtr;
			*theHalfEdgePtr	= theDeadHalfEdge->itsNext;
			FREE_MEMORY(theDeadHalfEdge);
		}
		else
			theHalfEdgePtr = &(*theHalfEdgePtr)->itsNext;
	}

	theFacePtr = &aDirichletDomain->itsFaceList;
	while (*theFacePtr != NULL)
	{
		if ((*theFacePtr)->itsDeletionFlag)
		{
			theDeadFace	= *theFacePtr;
			*theFacePtr	= theDeadFace->itsNext;
			FREE_MEMORY(theDeadFace);
		}
		else
			theFacePtr = &(*theFacePtr)->itsNext;
	}

	//	Done!
	return NULL;
}


static void AssignFaceColors(DirichletDomain *aDirichletDomain)
{
	HEFace			*theFace;
	unsigned int	theCount;
	Matrix			theInverseMatrix;
	HEFace			*theMate;
	double			theColorParameter;

	//	Initialize each color index to 0xFFFFFFFF as a marker.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		theFace->itsColorIndex = 0xFFFFFFFF;
	}

	//	Count the face pairs as we go along.
	theCount = 0;

	//	Assign an index to each face that doesn't already have one.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		if (theFace->itsColorIndex == 0xFFFFFFFF)
		{
			//	Assign to theFace the next available color index.
			theFace->itsColorIndex = theCount++;

			//	If theFace has a distinct mate,
			//	assign the same index to the mate.
			MatrixGeometricInverse(&theFace->itsMatrix, &theInverseMatrix);
			for (	theMate = theFace->itsNext;
					theMate != NULL;
					theMate = theMate->itsNext)
			{
				if (MatrixEquality(&theMate->itsMatrix, &theInverseMatrix, MATE_MATRIX_EPSILON))
				{
					theMate->itsColorIndex = theFace->itsColorIndex;
					break;
				}
			}
		}
	}

	//	Now that we know how many face pairs we've got,
	//	we can convert the temporary indices to a set
	//	of evenly spaced colors.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		//	Convert the temporary index to a parameter in the range [0,1],
		//	with uniform spacing.
		theColorParameter = (double)theFace->itsColorIndex / (double)theCount;

		//	Interpret theColorParameter as a hue.
		HSLAtoRGBA(	&(HSLAColor){theColorParameter, 0.3, 0.5, 1.0},
					&theFace->itsColorRGBA);

		//	Interpret theColorParameter as a greyscale value.
//		theFace->itsColorGreyscale = 0.5 * (theColorParameter + 1.0);
		theFace->itsColorGreyscale = (theColorParameter + 4.0) / 5.0;
	}
}


static void ComputeFaceCenters(DirichletDomain *aDirichletDomain)
{
	HEFace			*theFace;
	unsigned int	i;

	//	Compute the center of each face, normalized to the unit 3-sphere
	//	for easy interpolation to vertices-at-infinity.

	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		//	The center sits midway between the basepoint (0,0,0,1)
		//	and its image under the face-pairing matrix.
		for (i = 0; i < 4; i++)
			theFace->itsRawCenter.v[i] = 0.5 * theFace->itsMatrix.m[3][i];
		theFace->itsRawCenter.v[3] += 0.5;

		//	Normalize to the unit 3-sphere...
		VectorNormalize(&theFace->itsRawCenter, SpaceSpherical, &theFace->itsRawCenter);
		
		//	...and also relative to the SpaceType.
		VectorNormalize(&theFace->itsRawCenter, aDirichletDomain->itsSpaceType, &theFace->itsNormalizedCenter);
	}
}


static void ComputeWallDimensions(DirichletDomain *aDirichletDomain)
{
	double		theMaxBase;
	HEFace		*theFace;
	Vector		*theFaceCenter;
	HEHalfEdge	*theHalfEdge;
	Vector		*theTail,
				*theTip;
	double		theSide0,
				theSide1,
				theSide2,
				s,
				theArea;

	
	//	Compute the dimensions of the triangular wedges comprising each face.
	theMaxBase = 0;
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		theFaceCenter = &theFace->itsNormalizedCenter;

		theHalfEdge = theFace->itsHalfEdge;
		do
		{
			//	Advance to the next HalfEdge,
			//	but only after reading the current HalfEdge's tip,
			//	which will be the next HalfEdge's tail.
			theTail		= &theHalfEdge->itsTip->itsNormalizedPosition;
			theHalfEdge	= theHalfEdge->itsCycle;
			theTip		= &theHalfEdge->itsTip->itsNormalizedPosition;

			//	Compute the current wedge's dimensions.
			//	The computation is exact in the flat case,
			//	and serves our purposes well enough in the spherical
			//	and hyperbolic cases.
			theSide0					= VectorGeometricDistance2(theTail, theTip       );
			theSide1					= VectorGeometricDistance2(theTail, theFaceCenter);
			theSide2					= VectorGeometricDistance2(theTip,  theFaceCenter);
			s							= 0.5 * (theSide0 + theSide1 + theSide2);
			theArea						= sqrt( s * (s - theSide0) * (s - theSide1) * (s - theSide2) );	//	Heron's formula
			theHalfEdge->itsBase		= theSide0;
			theHalfEdge->itsAltitude	= 2.0 * theArea / theSide0;
			
			//	Note the largest base length.
			if (theMaxBase < theHalfEdge->itsBase)
				theMaxBase = theHalfEdge->itsBase;

		} while (theHalfEdge != theFace->itsHalfEdge);
	}
	
	//	Rescale itsBase and itsAltitude so that the largest base has length 1.
	if (theMaxBase > 0.0)
	{
		for (	theHalfEdge = aDirichletDomain->itsHalfEdgeList;
				theHalfEdge != NULL;
				theHalfEdge = theHalfEdge->itsNext)
		{
			theHalfEdge->itsBase		/= theMaxBase;
			theHalfEdge->itsAltitude	/= theMaxBase;
		}
	}
}


static ErrorText ComputeVertexFigures(DirichletDomain *aDirichletDomain)
{
	ErrorText		theErrorMessage	= NULL;
	HEVertex		*theVertex;
	HEHalfEdge		*theHalfEdge;
	Vector			theTail,
					theTip,
					theComponent,
					theNormal,
					theComponentParallel,
					theComponentPendicular,
					theScaledPointA,
					theScaledPointB;
	double			theDotProduct;

	//	Compute the faces of the vertex figure(s).
	//	One face of the vertex figure(s) sits at each vertex
	//	of the fundamental polyhedron.
	//	This code relies on the fact that for each vertex,
	//	itsRawPosition has already been normalized to sit on the 3-sphere.

	//	Compute the "outer point" on each half edge.
	for (	theHalfEdge = aDirichletDomain->itsHalfEdgeList;
			theHalfEdge != NULL;
			theHalfEdge = theHalfEdge->itsNext)
	{
		theTail			= theHalfEdge->itsMate->itsTip->itsRawPosition;
		theTip			= theHalfEdge->itsTip->itsRawPosition;
		theDotProduct	= VectorDotProduct(&theTail, &theTip);
		ScalarTimesVector(theDotProduct, &theTail, &theComponent);
		VectorDifference(&theTip, &theComponent, &theNormal);
		theErrorMessage	= VectorNormalize(&theNormal, SpaceSpherical, &theNormal);
		if (theErrorMessage != NULL)
			return theErrorMessage;
		ScalarTimesVector(cos(VERTEX_FIGURE_SIZE), &theTail,   &theComponentParallel  );
		ScalarTimesVector(sin(VERTEX_FIGURE_SIZE), &theNormal, &theComponentPendicular);
		VectorSum(&theComponentParallel, &theComponentPendicular, &theHalfEdge->itsOuterPoint);
		theErrorMessage = VectorNormalize(	&theHalfEdge->itsOuterPoint,
											aDirichletDomain->itsSpaceType,
											&theHalfEdge->itsOuterPoint);
		if (theErrorMessage != NULL)
			return theErrorMessage;
	}

	//	Compute the center of each face of the vertex figure.
	for (	theVertex = aDirichletDomain->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		theVertex->itsCenterPoint = (Vector) {{0.0, 0.0, 0.0, 0.0}};

		theHalfEdge = theVertex->itsOutboundHalfEdge;
		do
		{
			VectorSum(	&theVertex->itsCenterPoint,
						&theHalfEdge->itsOuterPoint,
						&theVertex->itsCenterPoint);

			theHalfEdge = theHalfEdge->itsMate->itsCycle;

		} while (theHalfEdge != theVertex->itsOutboundHalfEdge);

		theErrorMessage = VectorNormalize(	&theVertex->itsCenterPoint,
											aDirichletDomain->itsSpaceType,
											&theVertex->itsCenterPoint);
		if (theErrorMessage != NULL)
			return theErrorMessage;
	}

	//	Interpolate the inner vertices between
	//	the outer vertices and the center.
	for (	theHalfEdge = aDirichletDomain->itsHalfEdgeList;
			theHalfEdge != NULL;
			theHalfEdge = theHalfEdge->itsNext)
	{
		ScalarTimesVector(	VERTEX_FIGURE_CUTOUT,
							&theHalfEdge->itsOuterPoint,
							&theScaledPointA);
		ScalarTimesVector(	1.0 - VERTEX_FIGURE_CUTOUT,
							&theHalfEdge->itsMate->itsTip->itsCenterPoint,
							&theScaledPointB);
		VectorSum(&theScaledPointA, &theScaledPointB, &theHalfEdge->itsInnerPoint);
		theErrorMessage = VectorNormalize(	&theHalfEdge->itsInnerPoint,
											aDirichletDomain->itsSpaceType,
											&theHalfEdge->itsInnerPoint);
		if (theErrorMessage != NULL)
			return theErrorMessage;
	}

	return NULL;
}


static void ComputeOutradius(
	DirichletDomain	*aDirichletDomain)
{
	double		theOutradius;
	HEVertex	*theVertex;
	double		theDistance;
	
	GEOMETRY_GAMES_ASSERT(aDirichletDomain != NULL, "expected non-NULL Dirichlet domain");

	theOutradius = 0.0;
	
	for (	theVertex = aDirichletDomain->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		theDistance = VectorGeometricDistance(&theVertex->itsNormalizedPosition);
		
		if (theOutradius < theDistance)
			theOutradius = theDistance;
	}
	
	aDirichletDomain->itsOutradius = theOutradius;
}


double DirichletDomainOutradius(
	DirichletDomain	*aDirichletDomain)
{
	if (aDirichletDomain != NULL)
		return aDirichletDomain->itsOutradius;
	else	//	space is 3-sphere or projective 3-space
		return PI;
}


void StayInDirichletDomain(
	DirichletDomain	*aDirichletDomain,	//	input
	Matrix			*aPlacement)		//	input and output
{
	HEFace			*theFace;
	double			theFaceValue;
	Matrix			theRestoringMatrix;
	unsigned int	i;

	if (aDirichletDomain == NULL)	//	occurs for the 3-sphere and projective 3-space
		return;

	//	The object described by aPlacement is typically
	//	the user him/herself, but may also be the centerpiece
	//	(or anything else, for that matter).

	//	If the object strays out of the Dirichlet domain,
	//	use a face-pairing matrix to bring it back in.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		//	Evaluate the halfspace equation on the image of the basepoint (0,0,0,1)
		//	under the action of aPlacement.
		theFaceValue = 0;
		for (i = 0; i < 4; i++)
			theFaceValue += theFace->itsHalfspace.v[i] * aPlacement->m[3][i];

		//	The value we just computed will be positive
		//	iff the user has gone past the given face plane.
		if (theFaceValue > RESTORING_EPSILON)
		{
			//	Apply the inverse of the face-pairing matrix
			//	to bring the user back closer to the origin.
			MatrixGeometricInverse(&theFace->itsMatrix, &theRestoringMatrix);
			MatrixProduct(aPlacement, &theRestoringMatrix, aPlacement);
		}
	}
}


ErrorText ConstructHoneycomb(
	MatrixList		*aHolonomyGroup,	//	input
	DirichletDomain	*aDirichletDomain,	//	input
	Honeycomb		**aHoneycomb)		//	output
{
	ErrorText		theErrorMessage	= NULL;
	unsigned int	theNumVertices;
	HEVertex		*theVertex;
	unsigned int	i;

	static Vector	theBasepoint		= {{0.0, 0.0, 0.0, 1.0}};

	if (aHolonomyGroup == NULL)
		return u"ConstructHoneycomb() received a NULL holonomy group.";

	if (*aHoneycomb != NULL)
		return u"ConstructHoneycomb() received a non-NULL output location.";

	//	Special case:  Allow a NULL Dirichlet domain,
	//	which occurs for the 3-sphere.  We'll need
	//	the 3-sphere to display Clifford parallels.
	//	(Confession:  This is a hack.  I hope it causes no trouble.)
//	if (aDirichletDomain == NULL)
//		return u"ConstructHoneycomb() received a NULL Dirichlet domain.";

	//	Count the Dirichlet domain's vertices.
	theNumVertices = 0;
	if (aDirichletDomain != NULL)
	{
		for (	theVertex = aDirichletDomain->itsVertexList;
				theVertex != NULL;
				theVertex = theVertex->itsNext)
		{
			theNumVertices++;
		}
	}

	//	Allocate memory for the honeycomb.
	*aHoneycomb = AllocateHoneycomb(aHolonomyGroup->itsNumMatrices, theNumVertices);
	if (*aHoneycomb == NULL)
	{
		theErrorMessage = u"Couldn't get memory for aHoneycomb in ConstructHoneycomb().";
		goto CleanUpConstructHoneycomb;
	}

	//	For each cell...
	for (i = 0; i < aHolonomyGroup->itsNumMatrices; i++)
	{
		//	Set the matrix.
		(*aHoneycomb)->itsCells[i].itsMatrix = aHolonomyGroup->itsMatrices[i];

		//	Compute the image of the basepoint (0,0,0,1).
		VectorTimesMatrix(	&theBasepoint,
							&aHolonomyGroup->itsMatrices[i],
							&(*aHoneycomb)->itsCells[i].itsCellCenterInWorldSpace);
	}

CleanUpConstructHoneycomb:

	//	The present code flags an error only when *aHoneycomb == NULL,
	//	but let's include clean-up code anyhow, for future robustness.
	if (theErrorMessage != NULL)
		FreeHoneycomb(aHoneycomb);

	return theErrorMessage;
}


static Honeycomb *AllocateHoneycomb(
	unsigned int	aNumCells,
	unsigned int	aNumVertices)
{
	Honeycomb		*theHoneycomb	= NULL;
	unsigned int	i;

	if ( aNumCells   > 0xFFFFFFFF / sizeof(Honeycell)	//	for safety
	 || aNumVertices > 0xFFFFFFFF / sizeof(Vector))
		goto CleanUpAllocateHoneycomb;

	theHoneycomb = (Honeycomb *) GET_MEMORY(sizeof(Honeycomb));
	if (theHoneycomb != NULL)
	{
		//	For safe error handling, immediately set all pointers to NULL.
		theHoneycomb->itsCells			= NULL;
		theHoneycomb->itsVisibleCells	= NULL;
	}
	else
		goto CleanUpAllocateHoneycomb;

	theHoneycomb->itsNumCells	= aNumCells;
	theHoneycomb->itsCells		= (Honeycell *) GET_MEMORY(aNumCells * sizeof(Honeycell));

	//	Allocate itsVisibleCells and initialize to an empty array.
	//	For simplicity allocate the maximal buffer size, even though
	//	we will never use all of it.
	theHoneycomb->itsNumVisibleCells			= 0;
	theHoneycomb->itsNumVisiblePlainCells		= 0;
	theHoneycomb->itsNumVisibleReflectedCells	= 0;
	theHoneycomb->itsVisibleCells				= (Honeycell **) GET_MEMORY(aNumCells * sizeof(Honeycell *));
	if (theHoneycomb->itsVisibleCells != NULL)
	{
		for (i = 0; i < aNumCells; i++)
			theHoneycomb->itsVisibleCells[i] = NULL;
	}
	else
		goto CleanUpAllocateHoneycomb;

	//	Success
	return theHoneycomb;

CleanUpAllocateHoneycomb:

	//	We reach this point iff some memory allocation call failed.
	//	Free whatever memory may have been allocated.
	//	Other pointers should all be NULL.
	FreeHoneycomb(&theHoneycomb);
	
	//	Failure
	return NULL;
}


void FreeHoneycomb(Honeycomb **aHoneycomb)
{
	if (aHoneycomb != NULL
	 && *aHoneycomb != NULL)
	{
		FREE_MEMORY_SAFELY((*aHoneycomb)->itsCells);
		FREE_MEMORY_SAFELY((*aHoneycomb)->itsVisibleCells);
		FREE_MEMORY_SAFELY(*aHoneycomb);
	}
}


void MakeDirichletMesh(
	DirichletDomain	*aDirichletDomain,				//	input
	double			aCurrentAperture,				//	input
	bool			aShowColorCoding,				//	input
	unsigned int	*aNumMeshVertices,				//	output
	double			(**someMeshVertexPositions)[4],	//	output
	double			(**someMeshVertexTexCoords)[3],	//	output
	double			(**someMeshVertexColors)[4],	//	output;	pre-multiplied (αR, αG, αB, α)
	unsigned int	*aNumMeshFacets,				//	output
	unsigned int	(**someMeshFacets)[3])			//	output
{
	HEFace			*theFace;
	HEHalfEdge		*theHalfEdge;
	unsigned int	theFaceOrder;
	double			theTextureMultiple,
					(*thePosition)[4],
					(*theTexCoords)[3],
					(*theColor)[4];					//	pre-multiplied (αR, αG, αB, α)
	unsigned int	theMeshVertexIndex,
					(*theMeshFace)[3];
	double			theDirichletDomainFaceColor[4];	//	pre-multiplied (αR, αG, αB, α)
	Vector			*theFaceCenter;		//	normalized to the SpaceType
	bool			theParity;
	Vector			*theNearOuterVertex,//	normalized to the SpaceType
					theNearInnerVertex,	//	normalized to the SpaceType
					*theFarOuterVertex,	//	normalized to the SpaceType
					theFarInnerVertex;	//	normalized to the SpaceType
	double			theBaseTex,
					theAltitudeTex;

	GEOMETRY_GAMES_ASSERT(
			aDirichletDomain		!= NULL
		 && aNumMeshVertices		!= NULL
		 && aNumMeshFacets			!= NULL
		 && someMeshVertexPositions	!= NULL
		 && someMeshVertexTexCoords	!= NULL
		 && someMeshVertexColors	!= NULL
		 && someMeshFacets			!= NULL,
		"Output pointers must not be NULL");
	GEOMETRY_GAMES_ASSERT(
			*aNumMeshVertices			== 0
		 && *aNumMeshFacets				== 0
		 && *someMeshVertexPositions	== NULL
		 && *someMeshVertexTexCoords	== NULL
		 && *someMeshVertexColors		== NULL
		 && *someMeshFacets				== NULL,
		"Output pointers must point to NULL arrays");

	//	Create a mesh for a Dirichlet polyhedron with windows cut in its faces.
	//	Each face will be a polygonal annulus.
	//
	//		Note:
	//		Less vertex sharing is possible than you might at first think,
	//		because even when vertices belonging to adjacent facets
	//		share the same position in space, they typically have
	//		different texture coordinates.  Moreover, if we think
	//		of each polygonal annulus as the union of n quads,
	//		then not even consecutive quads can share vertices,
	//		because their required texture coordinates differ.
	//		(If an underlying face of the Dirichlet domain is regular,
	//		then texture coordinates may be assigned in such a way
	//		that consecutive quads may share vertices, but if
	//		an underlying face isn't regular, then that trick doesn't work.)
	//

	//	Each n-sided face contributes an annular region,
	//	realized as n trapezoids, each with 4 vertices and 2 facets.
	//	Count the mesh's total number of vertices and total number of facets.
	
	*aNumMeshVertices	= 0;
	*aNumMeshFacets		= 0;
	
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		//	Compute the face order n.
		theFaceOrder = 0;
		theHalfEdge = theFace->itsHalfEdge;
		do
		{
			theFaceOrder++;							//	count this edge
			theHalfEdge	= theHalfEdge->itsCycle;	//	move on to the next edge
		} while (theHalfEdge != theFace->itsHalfEdge);
	
		//	Increment the counts.
		*aNumMeshVertices	+= 4 * theFaceOrder;
		*aNumMeshFacets		+= 2 * theFaceOrder;
	}
	
	//	Allocate the arrays.  The caller will take responsibility
	//	for calling FreeDirichletMesh() to free these arrays
	//	when they're no longer needed.
	//
	*someMeshVertexPositions	= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshVertexTexCoords	= (double (*)[3]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [3]) );
	*someMeshVertexColors		= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshFacets				= (unsigned int (*)[3]) GET_MEMORY( (*aNumMeshFacets) * sizeof(unsigned int [3]) );

	theTextureMultiple = (aShowColorCoding ? FACE_TEXTURE_MULTIPLE_PLAIN : FACE_TEXTURE_MULTIPLE_WOOD);

	//	Keep running pointers to the current vertex's attributes...
	thePosition		= *someMeshVertexPositions;
	theTexCoords	= *someMeshVertexTexCoords;
	theColor		= *someMeshVertexColors;
	
	//	... and also keep track of its index in the array.
	theMeshVertexIndex = 0;

	//	Keep a running pointer to the current entry in the index buffer.
	theMeshFace = *someMeshFacets;

	//	Process each of the Dirichlet domain's faces in turn.
	for (	theFace = aDirichletDomain->itsFaceList;
			theFace != NULL;
			theFace = theFace->itsNext)
	{
		if (aShowColorCoding)
		{
			//	itsColorRGBA is already alpha-premultiplied
			theDirichletDomainFaceColor[0] = theFace->itsColorRGBA.r;
			theDirichletDomainFaceColor[1] = theFace->itsColorRGBA.g;
			theDirichletDomainFaceColor[2] = theFace->itsColorRGBA.b;
			theDirichletDomainFaceColor[3] = theFace->itsColorRGBA.a;
		}
		else
		{
			//	If the alpha component were less than 1.0,
			//	we'd need to premultiply the RGB components by it.
			theDirichletDomainFaceColor[0] = theFace->itsColorGreyscale;
			theDirichletDomainFaceColor[1] = theFace->itsColorGreyscale;
			theDirichletDomainFaceColor[2] = theFace->itsColorGreyscale;
			theDirichletDomainFaceColor[3] = 1.0;
		}

		theFaceCenter = &theFace->itsNormalizedCenter;

		//	After opening a window in the center of an n-sided face,
		//	an annulus-like shape remains, which we triangulate
		//	as n trapezoids, each with 4 vertices and 2 faces.
		//
		//		An earlier version of this algorithm, archived
		//		in the file "2n+2 vertices per Dirichlet face.c",
		//		used only 2n+2 vertices for the whole annulus,
		//		but got the texturing right only for regular faces,
		//		not irregular ones.  Furthermore it wasn't much faster.
		//

		//	Let the tangential texture coordinate run alternately
		//	forwards and backwards, so the texture coordinates will
		//	match up whenever possible.
		theParity = false;

		theHalfEdge = theFace->itsHalfEdge;
		do
		{
			//	Use outer vertices and face centers normalized to the SpaceType.
			//
			//		Note:  This won't work if we later support vertices-at-infinity.
			//		For vertices-at-infinity, we'd have to use raw positions.
			//		For now let's stick with normalized vectors
			//		to facilitate texturing.  See details below.
			//

			theNearOuterVertex = &theHalfEdge->itsTip->itsNormalizedPosition;
			VectorInterpolate(	theFaceCenter,
								theNearOuterVertex,
								aCurrentAperture,
								&theNearInnerVertex);
			(void) VectorNormalize(&theNearInnerVertex, aDirichletDomain->itsSpaceType, &theNearInnerVertex);

			theFarOuterVertex = &theHalfEdge->itsCycle->itsTip->itsNormalizedPosition;
			VectorInterpolate(	theFaceCenter,
								theFarOuterVertex,
								aCurrentAperture,
								&theFarInnerVertex);
			(void) VectorNormalize(&theFarInnerVertex, aDirichletDomain->itsSpaceType, &theFarInnerVertex);

			//	Convert the triangle's dimensions from physical units
			//	to texture coordinate units.
			theBaseTex		= theTextureMultiple * theHalfEdge->itsCycle->itsBase;
			theAltitudeTex	= theTextureMultiple * theHalfEdge->itsCycle->itsAltitude;
			
			//	Get the proportions for the texturing exactly right
			//	in the flat, regular case and approximately right otherwise.
			//
			//		Note.  Perspectively correct texture mapping
			//		is a real challenge in curved spaces.
			//		In the flat case, we're mapping a trapezoidal portion
			//		of a Dirichlet domain face onto a trapezoidal region
			//		in the texture, and we're guaranteed success just so
			//		we make sure the two trapezoids have the same shape
			//		(otherwise the final texturing will kink along the trapezoid's
			//		diagonal, where it splits into two triangles).
			//		In the spherical and hyperbolic cases, however,
			//		some residual distortion seems inevitable.
			//		Vertices-at-infinity would further complicate matters.
			//

			//	near inner vertex
			
			(*thePosition)[0] = theNearInnerVertex.v[0];
			(*thePosition)[1] = theNearInnerVertex.v[1];
			(*thePosition)[2] = theNearInnerVertex.v[2];
			(*thePosition)[3] = theNearInnerVertex.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = ( theBaseTex * ( theParity ? 0.5 - 0.5*aCurrentAperture : 0.5 + 0.5*aCurrentAperture ) );
			(*theTexCoords)[1] = ( theAltitudeTex * (1.0 - aCurrentAperture) );
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theDirichletDomainFaceColor[0];
			(*theColor)[1] = theDirichletDomainFaceColor[1];
			(*theColor)[2] = theDirichletDomainFaceColor[2];
			(*theColor)[3] = theDirichletDomainFaceColor[3];
			theColor++;

			//	far inner vertex

			(*thePosition)[0] = theFarInnerVertex.v[0];
			(*thePosition)[1] = theFarInnerVertex.v[1];
			(*thePosition)[2] = theFarInnerVertex.v[2];
			(*thePosition)[3] = theFarInnerVertex.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = ( theBaseTex * ( theParity ? 0.5 + 0.5*aCurrentAperture : 0.5 - 0.5*aCurrentAperture ) );
			(*theTexCoords)[1] = ( theAltitudeTex * (1.0 - aCurrentAperture) );
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theDirichletDomainFaceColor[0];
			(*theColor)[1] = theDirichletDomainFaceColor[1];
			(*theColor)[2] = theDirichletDomainFaceColor[2];
			(*theColor)[3] = theDirichletDomainFaceColor[3];
			theColor++;

			//	near outer vertex

			(*thePosition)[0] = theNearOuterVertex->v[0];
			(*thePosition)[1] = theNearOuterVertex->v[1];
			(*thePosition)[2] = theNearOuterVertex->v[2];
			(*thePosition)[3] = theNearOuterVertex->v[3];
			thePosition++;
			
			(*theTexCoords)[0] = ( theBaseTex * ( theParity ? 0.0 : 1.0 ) );
			(*theTexCoords)[1] = 0.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theDirichletDomainFaceColor[0];
			(*theColor)[1] = theDirichletDomainFaceColor[1];
			(*theColor)[2] = theDirichletDomainFaceColor[2];
			(*theColor)[3] = theDirichletDomainFaceColor[3];
			theColor++;

			//	far outer vertex

			(*thePosition)[0] = theFarOuterVertex->v[0];
			(*thePosition)[1] = theFarOuterVertex->v[1];
			(*thePosition)[2] = theFarOuterVertex->v[2];
			(*thePosition)[3] = theFarOuterVertex->v[3];
			thePosition++;
			
			(*theTexCoords)[0] = ( theBaseTex * ( theParity ? 1.0 : 0.0 ) );
			(*theTexCoords)[1] = 0.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theDirichletDomainFaceColor[0];
			(*theColor)[1] = theDirichletDomainFaceColor[1];
			(*theColor)[2] = theDirichletDomainFaceColor[2];
			(*theColor)[3] = theDirichletDomainFaceColor[3];
			theColor++;
			
			//	Create a pair of triangles.
			//
			//		If the model and view matrices are both orientation-preserving
			//		(or both orientation-reversing) these two mesh faces will have
			//		clockwise winding number when viewed from inside the Dirichlet domain.
			//

			(*theMeshFace)[0] = theMeshVertexIndex + 0;
			(*theMeshFace)[1] = theMeshVertexIndex + 1;
			(*theMeshFace)[2] = theMeshVertexIndex + 2;
			theMeshFace++;

			(*theMeshFace)[0] = theMeshVertexIndex + 2;
			(*theMeshFace)[1] = theMeshVertexIndex + 1;
			(*theMeshFace)[2] = theMeshVertexIndex + 3;
			theMeshFace++;
		
			//	Advance theMeshVertexIndex.
			theMeshVertexIndex += 4;
		
			//	Let the tangential texture coordinate
			//	run the other way next time.
			theParity = ! theParity;
		
			//	Move on to the next HalfEdge.
			theHalfEdge	= theHalfEdge->itsCycle;

		} while (theHalfEdge != theFace->itsHalfEdge);
	}
	
	//	Did we write the correct number of entries into each of the arrays?
	GEOMETRY_GAMES_ASSERT(
		thePosition  - (*someMeshVertexPositions) == *aNumMeshVertices,
		"Wrong number of elements written into someMeshVertexPositions in MakeDirichletMesh()");
	GEOMETRY_GAMES_ASSERT(
		theTexCoords - (*someMeshVertexTexCoords) == *aNumMeshVertices,
		"Wrong number of elements written into someMeshVertexTexCoords in MakeDirichletMesh()");
	GEOMETRY_GAMES_ASSERT(
		theColor     - (*someMeshVertexColors   ) == *aNumMeshVertices,
		"Wrong number of elements written into someMeshVertexColors in MakeDirichletMesh()");
	GEOMETRY_GAMES_ASSERT(
		theMeshFace - (*someMeshFacets) == *aNumMeshFacets,
		"Wrong number of elements written into someMeshFacets in MakeDirichletMesh()");
}

void FreeDirichletMesh(
	unsigned int	*aNumMeshVertices,				//	output
	double			(**someMeshVertexPositions)[4],	//	output
	double			(**someMeshVertexTexCoords)[3],	//	output
	double			(**someMeshVertexColors)[4],	//	output
	unsigned int	*aNumMeshFacets,				//	output
	unsigned int	(**someMeshFacets)[3])			//	output
{
	GEOMETRY_GAMES_ASSERT(
			aNumMeshVertices		!= NULL
		 && aNumMeshFacets			!= NULL
		 && someMeshVertexPositions	!= NULL
		 && someMeshVertexTexCoords	!= NULL
		 && someMeshVertexColors	!= NULL
		 && someMeshFacets			!= NULL,
		"Output pointers must not be NULL");
	
	*aNumMeshVertices	= 0;
	FREE_MEMORY_SAFELY(*someMeshVertexPositions);
	FREE_MEMORY_SAFELY(*someMeshVertexTexCoords);
	FREE_MEMORY_SAFELY(*someMeshVertexColors);

	*aNumMeshFacets		= 0;
	FREE_MEMORY_SAFELY(*someMeshFacets);
}


void MakeVertexFigureMesh(
	DirichletDomain	*aDirichletDomain,				//	input
	unsigned int	*aNumMeshVertices,				//	output
	double			(**someMeshVertexPositions)[4],	//	output
	double			(**someMeshVertexTexCoords)[3],	//	output
	double			(**someMeshVertexColors)[4],	//	output;	pre-multiplied (αR, αG, αB, α)
	unsigned int	*aNumMeshFacets,				//	output
	unsigned int	(**someMeshFacets)[3])			//	output
{
	HEVertex		*theVertex;
	HEHalfEdge		*theHalfEdge;
	unsigned int	theVertexOrder;
	double			(*thePosition)[4],
					(*theTexCoords)[3],
					(*theColor)[4];		//	pre-multiplied (αR, αG, αB, α)
	unsigned int	theMeshVertexIndex,
					(*theMeshFace)[3];
	bool			theParity;
	HEHalfEdge		*theNextHalfEdge;
	
	static const double	theWhiteColor[4]	= {1.0, 1.0, 1.0, 1.0},	//	pre-multiplied (αR, αG, αB, α)
						theGreyColor[4]		= {0.5, 0.5, 0.5, 1.0};	//	pre-multiplied (αR, αG, αB, α)

	GEOMETRY_GAMES_ASSERT(
			aDirichletDomain		!= NULL
		 && aNumMeshVertices		!= NULL
		 && aNumMeshFacets			!= NULL
		 && someMeshVertexPositions	!= NULL
		 && someMeshVertexTexCoords	!= NULL
		 && someMeshVertexColors	!= NULL
		 && someMeshFacets			!= NULL,
		"Output pointers must not be NULL");
	GEOMETRY_GAMES_ASSERT(
			*aNumMeshVertices			== 0
		 && *aNumMeshFacets				== 0
		 && *someMeshVertexPositions	== NULL
		 && *someMeshVertexTexCoords	== NULL
		 && *someMeshVertexColors		== NULL
		 && *someMeshFacets				== NULL,
		"Output pointers must point to NULL arrays");

	//	Create a mesh for the vertex figure faces belonging to this Dirichlet domain.
	//	Each face will be a polygon with a window cut in its center,
	//	in other words, each face will be a polygonal annulus.
	//	Note that we're not create a whole vertex figure in one place,
	//	but rather we're create on polygonal annulus at each
	//	of the Dirichlet domains vertices.  They'll ultimately get
	//	pieced together in the tiling.
	//
	//		Note:
	//		Less vertex sharing is possible than you might at first think,
	//		for the same reason as explained in MakeDirichletMesh().
	//

	//	Each vertex (of the Dirichlet domain) of order n contributes
	//	an annular region, realized as n pairs of trapezoids.
	//	Each pair consists of one white outward-facing trapezoid
	//	and one grey inward-facing trapezoid.
	//	Each trapezoid, in turn, consists of 4 vertices and 2 facets.

	//	Count the mesh's total number of vertices and total number of facets.
	*aNumMeshVertices	= 0;
	*aNumMeshFacets		= 0;
	for (	theVertex = aDirichletDomain->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		//	Compute the vertex order.
		theVertexOrder = 0;
		theHalfEdge = theVertex->itsOutboundHalfEdge;
		do
		{
			theVertexOrder++;								//	count this HalfEdge
			theHalfEdge	= theHalfEdge->itsMate->itsCycle;	//	move on to the next HalfEdge
		} while (theHalfEdge != theVertex->itsOutboundHalfEdge);

		//	Increment the counts.
		*aNumMeshVertices	+= 2 * 4 * theVertexOrder;
		*aNumMeshFacets		+= 2 * 2 * theVertexOrder;
	}
	
	//	Allocate the arrays.  The caller will take responsibility
	//	for calling FreeVertexFigureMesh() to free these arrays
	//	when they're no longer needed.
	//
	*someMeshVertexPositions	= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshVertexTexCoords	= (double (*)[3]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [3]) );
	*someMeshVertexColors		= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshFacets				= (unsigned int (*)[3]) GET_MEMORY( (*aNumMeshFacets) * sizeof(unsigned int [3]) );

	//	Keep running pointers to the current vertex's attributes...
	thePosition		= *someMeshVertexPositions;
	theTexCoords	= *someMeshVertexTexCoords;
	theColor		= *someMeshVertexColors;
	
	//	... and also keep track of its index in the array.
	theMeshVertexIndex = 0;

	//	Keep a running pointer to the current entry in the index buffer.
	theMeshFace = *someMeshFacets;

	//	Process each of the Dirichlet domain's vertices in turn.
	for (	theVertex = aDirichletDomain->itsVertexList;
			theVertex != NULL;
			theVertex = theVertex->itsNext)
	{
		//	Let the tangential texture coordinate run alternately
		//	forwards and backwards, so the texture coordinates will
		//	match up whenever possible.
		theParity = false;

		theHalfEdge = theVertex->itsOutboundHalfEdge;
		do
		{
			//	Note the next HalfEdge.
			theNextHalfEdge = theHalfEdge->itsMate->itsCycle;


			//	Vertices
			
			//		light near inner vertex

			(*thePosition)[0] = theHalfEdge->itsInnerPoint.v[0];
			(*thePosition)[1] = theHalfEdge->itsInnerPoint.v[1];
			(*thePosition)[2] = theHalfEdge->itsInnerPoint.v[2];
			(*thePosition)[3] = theHalfEdge->itsInnerPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 0.15 : 0.85);
			(*theTexCoords)[1] = 1.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theWhiteColor[0];
			(*theColor)[1] = theWhiteColor[1];
			(*theColor)[2] = theWhiteColor[2];
			(*theColor)[3] = theWhiteColor[3];
			theColor++;
			
			//		light near outer vertex

			(*thePosition)[0] = theHalfEdge->itsOuterPoint.v[0];
			(*thePosition)[1] = theHalfEdge->itsOuterPoint.v[1];
			(*thePosition)[2] = theHalfEdge->itsOuterPoint.v[2];
			(*thePosition)[3] = theHalfEdge->itsOuterPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 0.00 : 1.00);
			(*theTexCoords)[1] = 0.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theWhiteColor[0];
			(*theColor)[1] = theWhiteColor[1];
			(*theColor)[2] = theWhiteColor[2];
			(*theColor)[3] = theWhiteColor[3];
			theColor++;
			
			//		light far inner vertex

			(*thePosition)[0] = theNextHalfEdge->itsInnerPoint.v[0];
			(*thePosition)[1] = theNextHalfEdge->itsInnerPoint.v[1];
			(*thePosition)[2] = theNextHalfEdge->itsInnerPoint.v[2];
			(*thePosition)[3] = theNextHalfEdge->itsInnerPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 0.85 : 0.15);
			(*theTexCoords)[1] = 1.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theWhiteColor[0];
			(*theColor)[1] = theWhiteColor[1];
			(*theColor)[2] = theWhiteColor[2];
			(*theColor)[3] = theWhiteColor[3];
			theColor++;
			
			//		light far outer vertex

			(*thePosition)[0] = theNextHalfEdge->itsOuterPoint.v[0];
			(*thePosition)[1] = theNextHalfEdge->itsOuterPoint.v[1];
			(*thePosition)[2] = theNextHalfEdge->itsOuterPoint.v[2];
			(*thePosition)[3] = theNextHalfEdge->itsOuterPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 1.00 : 0.00);
			(*theTexCoords)[1] = 0.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theWhiteColor[0];
			(*theColor)[1] = theWhiteColor[1];
			(*theColor)[2] = theWhiteColor[2];
			(*theColor)[3] = theWhiteColor[3];
			theColor++;
			
			//		dark near inner vertex

			(*thePosition)[0] = theHalfEdge->itsInnerPoint.v[0];
			(*thePosition)[1] = theHalfEdge->itsInnerPoint.v[1];
			(*thePosition)[2] = theHalfEdge->itsInnerPoint.v[2];
			(*thePosition)[3] = theHalfEdge->itsInnerPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 0.15 : 0.85);
			(*theTexCoords)[1] = 1.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theGreyColor[0];
			(*theColor)[1] = theGreyColor[1];
			(*theColor)[2] = theGreyColor[2];
			(*theColor)[3] = theGreyColor[3];
			theColor++;
			
			//		dark near outer vertex

			(*thePosition)[0] = theHalfEdge->itsOuterPoint.v[0];
			(*thePosition)[1] = theHalfEdge->itsOuterPoint.v[1];
			(*thePosition)[2] = theHalfEdge->itsOuterPoint.v[2];
			(*thePosition)[3] = theHalfEdge->itsOuterPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 0.00 : 1.00);
			(*theTexCoords)[1] = 0.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theGreyColor[0];
			(*theColor)[1] = theGreyColor[1];
			(*theColor)[2] = theGreyColor[2];
			(*theColor)[3] = theGreyColor[3];
			theColor++;
			
			//		dark far inner vertex

			(*thePosition)[0] = theNextHalfEdge->itsInnerPoint.v[0];
			(*thePosition)[1] = theNextHalfEdge->itsInnerPoint.v[1];
			(*thePosition)[2] = theNextHalfEdge->itsInnerPoint.v[2];
			(*thePosition)[3] = theNextHalfEdge->itsInnerPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 0.85 : 0.15);
			(*theTexCoords)[1] = 1.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theGreyColor[0];
			(*theColor)[1] = theGreyColor[1];
			(*theColor)[2] = theGreyColor[2];
			(*theColor)[3] = theGreyColor[3];
			theColor++;
			
			//		dark far outer vertex

			(*thePosition)[0] = theNextHalfEdge->itsOuterPoint.v[0];
			(*thePosition)[1] = theNextHalfEdge->itsOuterPoint.v[1];
			(*thePosition)[2] = theNextHalfEdge->itsOuterPoint.v[2];
			(*thePosition)[3] = theNextHalfEdge->itsOuterPoint.v[3];
			thePosition++;
			
			(*theTexCoords)[0] = (theParity ? 1.00 : 0.00);
			(*theTexCoords)[1] = 0.0;
			(*theTexCoords)[2] = 0.0;	//	unused for non-cubemap texture
			theTexCoords++;
			
			(*theColor)[0] = theGreyColor[0];
			(*theColor)[1] = theGreyColor[1];
			(*theColor)[2] = theGreyColor[2];
			(*theColor)[3] = theGreyColor[3];
			theColor++;


			//	Facets
			
			//		light facets
			
			(*theMeshFace)[0] = theMeshVertexIndex + 0;
			(*theMeshFace)[1] = theMeshVertexIndex + 1;
			(*theMeshFace)[2] = theMeshVertexIndex + 2;
			theMeshFace++;
			
			(*theMeshFace)[0] = theMeshVertexIndex + 2;
			(*theMeshFace)[1] = theMeshVertexIndex + 1;
			(*theMeshFace)[2] = theMeshVertexIndex + 3;
			theMeshFace++;
			
			//		dark facets
			
			(*theMeshFace)[0] = theMeshVertexIndex + 4;
			(*theMeshFace)[1] = theMeshVertexIndex + 6;
			(*theMeshFace)[2] = theMeshVertexIndex + 5;
			theMeshFace++;
			
			(*theMeshFace)[0] = theMeshVertexIndex + 5;
			(*theMeshFace)[1] = theMeshVertexIndex + 6;
			(*theMeshFace)[2] = theMeshVertexIndex + 7;
			theMeshFace++;
		
			//		advance theMeshVertexIndex
			theMeshVertexIndex += 8;


			//	Let the tangential texture coordinate
			//	run the other way next time.
			theParity = ! theParity;
		
			//	Move on to the next HalfEdge.
			theHalfEdge	= theNextHalfEdge;

		} while (theHalfEdge != theVertex->itsOutboundHalfEdge);
	}
	
	//	Did we write the correct number of entries into each of the arrays?
	GEOMETRY_GAMES_ASSERT(
		thePosition  - (*someMeshVertexPositions) == *aNumMeshVertices,
		"Wrong number of elements written into someMeshVertexPositions in MakeVertexFigureMesh()");
	GEOMETRY_GAMES_ASSERT(
		theTexCoords - (*someMeshVertexTexCoords) == *aNumMeshVertices,
		"Wrong number of elements written into someMeshVertexTexCoords in MakeVertexFigureMesh()");
	GEOMETRY_GAMES_ASSERT(
		theColor     - (*someMeshVertexColors   ) == *aNumMeshVertices,
		"Wrong number of elements written into someMeshVertexColors in MakeVertexFigureMesh()");
	GEOMETRY_GAMES_ASSERT(
		theMeshFace - (*someMeshFacets) == *aNumMeshFacets,
		"Wrong number of elements written into someMeshFacets in MakeVertexFigureMesh()");
}

void FreeVertexFigureMesh(
	unsigned int	*aNumMeshVertices,				//	output
	double			(**someMeshVertexPositions)[4],	//	output
	double			(**someMeshVertexTexCoords)[3],	//	output
	double			(**someMeshVertexColors)[4],	//	output
	unsigned int	*aNumMeshFacets,				//	output
	unsigned int	(**someMeshFacets)[3])			//	output
{
	GEOMETRY_GAMES_ASSERT(
			aNumMeshVertices		!= NULL
		 && aNumMeshFacets			!= NULL
		 && someMeshVertexPositions	!= NULL
		 && someMeshVertexTexCoords	!= NULL
		 && someMeshVertexColors	!= NULL
		 && someMeshFacets			!= NULL,
		"Output pointers must not be NULL");
	
	*aNumMeshVertices	= 0;
	FREE_MEMORY_SAFELY(*someMeshVertexPositions);
	FREE_MEMORY_SAFELY(*someMeshVertexTexCoords);
	FREE_MEMORY_SAFELY(*someMeshVertexColors);

	*aNumMeshFacets		= 0;
	FREE_MEMORY_SAFELY(*someMeshFacets);
}


void CullAndSortVisibleCells(
	Honeycomb	*aHoneycomb,
	Matrix		*aViewMatrix,
	double		anImageWidth,
	double		anImageHeight,
	double		aHorizonRadius,
	double		aDirichletDomainRadius,
	SpaceType	aSpaceType)
{
	double			theCullingHyperplanes[4][4];
	double			theTilingRadius,
					theAdjustedDirichletDomainRadius;
	Honeycell		*theHoneycell;
	unsigned int	i;
	Vector			theCellCenterInCameraSpace;

	if (aHoneycomb != NULL)
	{
		//	Count the number of visible cells.
		aHoneycomb->itsNumVisibleCells			= 0;
		aHoneycomb->itsNumVisiblePlainCells		= 0;
		aHoneycomb->itsNumVisibleReflectedCells	= 0;
		
		//	A translated copy of the Dirichlet domain
		//	may intersect the observer's horizon sphere whenever
		//	its center lies within
		//
		//		aHorizonRadius + aDirichletDomainRadius
		//
		//	units of the observer.
		theTilingRadius = aHorizonRadius + aDirichletDomainRadius;
		
		//	For best efficiency in BoundingSphereIntersectsViewFrustum(),
		//	pre-compute sine/ø/sinh of aDirichletDomainRadius.
		switch (aSpaceType)
		{
			case SpaceNone:
				GEOMETRY_GAMES_ABORT("unexpected SpaceNone");
				break;
				
			case SpaceSpherical:
				//	This approach works poorly in the spherical case,
				//	because sin(θ) starts decreasing as θ goes past π/2.
				//	To avoid complicating an algorithm that works correctly
				//	and is wonderfully efficient in the more demanding
				//	Euclidean and hyperbolic cases, we'll ignore
				//	theAdjustedDirichletDomainRadius in the spherical case
				//	and simply accept all cells, knowing that performance
				//	isn't an issue in the spherical case anyhow.
				theAdjustedDirichletDomainRadius = sin(aDirichletDomainRadius);
				break;
				
			case SpaceFlat:
				theAdjustedDirichletDomainRadius = aDirichletDomainRadius;
				break;
				
			case SpaceHyperbolic:
				theAdjustedDirichletDomainRadius = sinh(aDirichletDomainRadius);
				break;
		}
		
		//	We'll want to cull to the view frustum's side faces.
		//	Extend each side to a hyperplane through the origin
		//	in the 4-dimensional embedding space,
		//	and prepare the inward-pointing unit normal vector
		//	to each such hyperplane.
		//
		//		Note:  The frustum's side faces all pass through
		//		the origin (0,0,0,1), so these normal vectors
		//		will all lie in the "horizontal" hyperplane w = 0.
		//
		MakeCullingHyperplanes(anImageWidth, anImageHeight, theCullingHyperplanes);

		//	Examine each cell, and put those that are visible
		//	onto itsVisibleCells list.
		theHoneycell = aHoneycomb->itsCells;
		for (i = 0; i < aHoneycomb->itsNumCells; i++)
		{
			VectorTimesMatrix(	&theHoneycell->itsCellCenterInWorldSpace,
								aViewMatrix,
								&theCellCenterInCameraSpace);

			theHoneycell->itsDistanceCameraToCellCenter = VectorGeometricDistance(&theCellCenterInCameraSpace);

			//	Technical note:
			//
			//		On the one hand, culling against z > 0 is redundant,
			//		given that we'll be culling against the view frustum anyhow.
			//		On the other hand, it's a computationally inexpensive test
			//		to do first, to eliminate almost half the cells
			//		without calling BoundingSphereIntersectsViewFrustum()
			//		on them at all.
			//
			//		(As an added bonus, the few cells that do get culled by z > 0
			//		and wouldn't get culled by the view frustum are cells
			//		sitting close to -- but behind -- the origin.  Such cells
			//		would "steal" one of the maximum level-of-detail slots.
			//		By culling such cells, the level-of-detail code will
			//		work a little better.)
			if
			(
				//	Accept all cells if the space is spherical,
				//	for the reason explained in the comment accompanying
				//	"sin(aDirichletDomainRadius)" above.
				aSpaceType == SpaceSpherical
			 ||
				(
					theCellCenterInCameraSpace.v[2] > - theAdjustedDirichletDomainRadius
				 &&
					theHoneycell->itsDistanceCameraToCellCenter < theTilingRadius
				 &&
					BoundingSphereIntersectsViewFrustum(theCellCenterInCameraSpace.v,
														theAdjustedDirichletDomainRadius,
														theCullingHyperplanes)
				)
			)
			{
				aHoneycomb->itsVisibleCells[aHoneycomb->itsNumVisibleCells] = theHoneycell;
				aHoneycomb->itsNumVisibleCells++;
				
				if (theHoneycell->itsMatrix.itsParity == aViewMatrix->itsParity)
					aHoneycomb->itsNumVisiblePlainCells++;
				else
					aHoneycomb->itsNumVisibleReflectedCells++;
			}
			
			theHoneycell++;
		}

		//	Sort the visible cells in increasing distance
		//	from the observer.  The cells should be roughly sorted
		//	to begin with (because they are sorted in order of
		//	increasing distance from the basepoint (0,0,0,1))
		//	so it makes little difference whether we use qsort()
		//	or a bubble sort.  The important thing is that we're sorting
		//	only the visible cells, not the whole honeycomb.
		//
		//		Note:  Profiling confirms that in practice
		//		the time spent in qsort() is small compared
		//		to the time spent in culling (above).
		//
		qsort(	aHoneycomb->itsVisibleCells,
				aHoneycomb->itsNumVisibleCells,
				sizeof(Honeycell *),
				CompareCellCenterDistances);
	}
}

static void MakeCullingHyperplanes(
	double	anImageWidth,				//	input
	double	anImageHeight,				//	input
	double	someCullingPlanes[4][4])	//	output;  inward-pointing unit normal vectors to the hyperplanes
										//		that pass through the side faces of view frustum
										//		and through the origin (0,0,0,0)
{
	double	w,
			h,
			c,
			theInverseLengthWC,
			theInverseLengthHC;

	//	Note #1:  The frustum's side faces all pass through
	//	the origin (0,0,0,1), so these normal vectors
	//	will all lie in the "horizontal" hyperplane w = 0.
	//
	//	Note #2:  In principle the normal vectors have
	//	unit length relative to the metric on the 4-dimensional
	//	embedding space.  But because the normal vectors
	//	all lie in the "horizontal" hyperplane w = 0,
	//	their length gets computed relative to the part
	//	of the metric that's always positive definite.

	//	MakeProjectionMatrix() uses a viewing frustum with corners at
	//
	//			(±n*(w/c), ±n*(h/c), n)
	//		and
	//			(±f*(w/c), ±f*(h/c), f)
	//
	//	where
	//		w = view width
	//		h = view height
	//		c = characteristic view size (explained immediately below)
	//		n = near clipping distance (ignored here)
	//		f = far  clipping distance (ignored here)
	//
	//	The characteristic view size is the distance in the view,
	//	measured from the center of the view outwards,
	//	that subtends a 45° angle in the observer's field-of-view.
	//
	w = 0.5 * anImageWidth;
	h = 0.5 * anImageHeight;
	c = CharacteristicViewSize(anImageWidth, anImageHeight);
	GEOMETRY_GAMES_ASSERT(w > 0.0 && h > 0.0, "received image of nonpositive size");
	GEOMETRY_GAMES_ASSERT(c > 0.0, "nonpositive characteristic size");

	//	The caller will cull to the frustum's side faces
	//	(as well as to the horizon radius).  Here we prepare
	//	inward-pointing unit normal vectors to the hyperplanes
	//	that pass through those side faces and also through
	//	the origin (0,0,0,0).

	theInverseLengthWC = 1.0 / sqrt(w*w + c*c);
	theInverseLengthHC = 1.0 / sqrt(h*h + c*c);

	someCullingPlanes[0][0] = -c * theInverseLengthWC;
	someCullingPlanes[0][1] = 0.0;
	someCullingPlanes[0][2] =  w * theInverseLengthWC;
	someCullingPlanes[0][3] = 0.0;

	someCullingPlanes[1][0] = +c * theInverseLengthWC;
	someCullingPlanes[1][1] = 0.0;
	someCullingPlanes[1][2] =  w * theInverseLengthWC;
	someCullingPlanes[1][3] = 0.0;

	someCullingPlanes[2][0] = 0.0;
	someCullingPlanes[2][1] = -c * theInverseLengthHC;
	someCullingPlanes[2][2] =  h * theInverseLengthHC;
	someCullingPlanes[2][3] = 0.0;

	someCullingPlanes[3][0] = 0.0;
	someCullingPlanes[3][1] = +c * theInverseLengthHC;
	someCullingPlanes[3][2] =  h * theInverseLengthHC;
	someCullingPlanes[3][3] = 0.0;
}

static bool BoundingSphereIntersectsViewFrustum(
	double	aCellCenterInCameraSpace[4],
	double	anAdjustedDirichletDomainRadius,	//	= sin(r), r, or sinh(r), as appropriate for the geometry
	double	someCullingPlanes[4][4])			//	inward-pointing unit normals to sides of view frustum
{
	unsigned int	i;
	double			theAdjustedDistance;	//	distance from center of bounding sphere to culling plane
	
	for (i = 0; i < 4; i++)
	{
		//	Taking the dot product of aCellCenterInCameraSpace
		//	with a unit normal vector to a culling plane
		//	gives sin(d), d, or sinh(d), according to the geometry.
		//
		//		Note:  someCullingPlanes[i][3] is always zero,
		//		so don't need to know the sign of geometry-dependent component
		//		of the inner product.  Very convenient!
		//
		theAdjustedDistance	= aCellCenterInCameraSpace[0] * someCullingPlanes[i][0]
							+ aCellCenterInCameraSpace[1] * someCullingPlanes[i][1]
							+ aCellCenterInCameraSpace[2] * someCullingPlanes[i][2]
						//  ± aCellCenterInCameraSpace[3] * 0.0
							;
						
		
		if (theAdjustedDistance < - anAdjustedDirichletDomainRadius)
			return false;
	}
	
	return true;
}


static __cdecl signed int CompareCellCenterDistances(
	const void	*p1,
	const void	*p2)
{
	double	theDifference;

	theDifference = (*((Honeycell **) p1))->itsDistanceCameraToCellCenter
				  - (*((Honeycell **) p2))->itsDistanceCameraToCellCenter;

	if (theDifference < 0.0)
		return -1;

	if (theDifference > 0.0)
		return +1;

	return 0;
}


#ifdef START_OUTSIDE

void SelectFirstCellOnly(
	Honeycomb	*aHoneycomb)
{
	GEOMETRY_GAMES_ASSERT(aHoneycomb != NULL,			"missing aHoneycomb"			);
	GEOMETRY_GAMES_ASSERT(aHoneycomb->itsNumCells > 0,	"aHoneycomb has no Honeycells"	);
	
	aHoneycomb->itsNumVisibleCells			= 1;
	aHoneycomb->itsNumVisiblePlainCells		= 1;
	aHoneycomb->itsNumVisibleReflectedCells	= 0;
	
	aHoneycomb->itsVisibleCells[0] = &aHoneycomb->itsCells[0];
}

#endif	//	START_OUTSIDE

