//	CurvedSpacesSphere.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include "GeometryGamesUtilities-Common.h"
#include <math.h>


//	An icosahedron is most efficient starting point
//	for a triangulation of a sphere, so let's use that.
//	(By contrast, some of the Geometry Games start
//	with an octahedron instead, so that the resulting
//	sphere mesh will align properly with axis-aligned tube meshes.
//	But in Curved Spaces the spheres don't need to align with tubes.)

//	Starting with the icosahedron, each successive subdivision
//	quadruples the number of facets:
//
//		level 0		   20 facets (icosahedron)
//		level 1		   80 facets
//		level 2		  320 facets
//		level 3		 1280 facets
//		level 4		 5120 facets
//		level 5		20480 facets
//
//	Allow for up to three subdivisions of the icosahedron,
//	which at the full level-of-detail realizes the sphere
//	as a polyhedron with 1280 facets.
//
//	Don't ever push the refinement level too ridiculously high, or
//	the vertex indices will overflow the 16-bit ints used to store them.
//	Large numbers of vertices would also require an unreasonable amount
//	of memory for theNewVertexTable[][] in SubdivideMesh().
//
#define MAX_SPHERE_REFINEMENT_LEVEL		3

//	The number of facets quadruples from one refinement level to the next.
#define MAX_NUM_REFINED_SPHERE_FACETS	(20 << (2 * MAX_SPHERE_REFINEMENT_LEVEL))

//	Each facet sees three edges, but each edge gets seen twice.
#define MAX_NUM_REFINED_SPHERE_EDGES	(3 * MAX_NUM_REFINED_SPHERE_FACETS / 2)

//	Each facet sees three vertices, and -- except for the twelve original vertices --
//	each vertex gets seen six times.  To compensate for the twelve original vertices,
//	each of which gets seen only five times, but must add in a correction factor
//	of 12*(6 - 5) = 12 "missing" vertex sightings.
#define MAX_NUM_REFINED_SPHERE_VERTICES	((3 * MAX_NUM_REFINED_SPHERE_FACETS + 12*(6 - 5)) / 6)


static void InitIcosahedron(
	double aRadius, const double aColor[4],
	unsigned int *anIcosahedronNumVertices, double anIcosahedronVertexPositions[MAX_NUM_REFINED_SPHERE_VERTICES][4], double anIcosahedronVertexTexCoords[MAX_NUM_REFINED_SPHERE_VERTICES][3], double anIcosahedronVertexColors[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	unsigned int *anIcosahedronNumFacets, unsigned int anIcosahedronFacets[MAX_NUM_REFINED_SPHERE_FACETS][3]);
static void SubdivideMesh(
	double aRadius, const double aColor[4],
	unsigned int aSrcNumVertices, double aSrcVertexPositions[MAX_NUM_REFINED_SPHERE_VERTICES][4], double aSrcVertexTexCoords[MAX_NUM_REFINED_SPHERE_VERTICES][3], double aSrcVertexColors[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	unsigned int aSrcNumFacets, unsigned int aSrcFacets[MAX_NUM_REFINED_SPHERE_FACETS][3],
	unsigned int *aSubdivisionNumVertices, double aSubdivisionVertexPositions[MAX_NUM_REFINED_SPHERE_VERTICES][4], double aSubdivisionVertexTexCoords[MAX_NUM_REFINED_SPHERE_VERTICES][3], double aSubdivisionVertexColors[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	unsigned int *aSubdivisionNumFacets, unsigned int aSubdivisionFacets[MAX_NUM_REFINED_SPHERE_FACETS][3]);


void MakeSphereMesh(
	double			aRadius,						//	input
	unsigned int	aNumSubdivisions,				//	input
	const double	aColor[4],						//	input;	pre-multiplied (αR, αG, αB, α)
	unsigned int	*aNumMeshVertices,				//	output
	double			(**someMeshVertexPositions)[4],	//	output
	double			(**someMeshVertexTexCoords)[3],	//	output
	double			(**someMeshVertexColors)[4],	//	output;	pre-multiplied (αR, αG, αB, α)
	unsigned int	*aNumMeshFacets,				//	output
	unsigned int	(**someMeshFacets)[3])			//	output
{
	unsigned int	theSphereNumVertices[MAX_SPHERE_REFINEMENT_LEVEL + 1];
	double			theSphereVertexPositions[MAX_SPHERE_REFINEMENT_LEVEL + 1][MAX_NUM_REFINED_SPHERE_VERTICES][4],
					theSphereVertexTexCoords[MAX_SPHERE_REFINEMENT_LEVEL + 1][MAX_NUM_REFINED_SPHERE_VERTICES][3],
					theSphereVertexColors[MAX_SPHERE_REFINEMENT_LEVEL + 1][MAX_NUM_REFINED_SPHERE_VERTICES][4];
	unsigned int	theSphereNumFacets[MAX_SPHERE_REFINEMENT_LEVEL + 1],
					theSphereFacets[MAX_SPHERE_REFINEMENT_LEVEL + 1][MAX_NUM_REFINED_SPHERE_FACETS][3],
					i,
					j;

	GEOMETRY_GAMES_ASSERT(
			aNumMeshVertices		!= NULL
		 && someMeshVertexPositions	!= NULL
		 && someMeshVertexTexCoords	!= NULL
		 && someMeshVertexColors	!= NULL
		 && aNumMeshFacets			!= NULL
		 && someMeshFacets			!= NULL,
		"Output pointers must not be NULL");
	GEOMETRY_GAMES_ASSERT(
			*aNumMeshVertices			== 0
		 && *someMeshVertexPositions	== NULL
		 && *someMeshVertexTexCoords	== NULL
		 && *someMeshVertexColors		== NULL
		 && *aNumMeshFacets				== 0
		 && *someMeshFacets				== NULL,
		"Output pointers must point to NULL arrays");


	//	Construct an icosahedron for the base level.
	InitIcosahedron(
		aRadius,
		aColor,
		&theSphereNumVertices[0],
		theSphereVertexPositions[0],
		theSphereVertexTexCoords[0],
		theSphereVertexColors[0],
		&theSphereNumFacets[0],
		theSphereFacets[0]);

	//	Subdivide each mesh to get the next one in the series.
	for (i = 0; i < MAX_SPHERE_REFINEMENT_LEVEL; i++)
	{
		SubdivideMesh
		(
			aRadius,	//	useful for normalization
			aColor,
		 
			//	input mesh
			theSphereNumVertices[i],
			theSphereVertexPositions[i],
			theSphereVertexTexCoords[i],
			theSphereVertexColors[i],
			theSphereNumFacets[i],
			theSphereFacets[i],
			
			//	output mesh
			&theSphereNumVertices[i+1],
			theSphereVertexPositions[i+1],
			theSphereVertexTexCoords[i+1],
			theSphereVertexColors[i+1],
			&theSphereNumFacets[i+1],
			theSphereFacets[i+1]
		);
	}
	
	//	Make sure that the requested number of subdivisions isn't too large.
	GEOMETRY_GAMES_ASSERT(
		aNumSubdivisions <= MAX_SPHERE_REFINEMENT_LEVEL,
		"aNumSubdivisions exceeds the maximum supported level.  You may increase MAX_SPHERE_REFINEMENT_LEVEL if desired.");

	//	Set the number of vertices and facets.
	*aNumMeshVertices	= theSphereNumVertices[aNumSubdivisions];
	*aNumMeshFacets		= theSphereNumFacets[aNumSubdivisions];

	//	Allocate the output arrays.
	//	The caller will take responsibility for calling FreeSphereMesh()
	//	to free these arrays when they're no longer needed.
	*someMeshVertexPositions	= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshVertexTexCoords	= (double (*)[3]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [3]) );
	*someMeshVertexColors		= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshFacets				= (unsigned int (*)[3]) GET_MEMORY( (*aNumMeshFacets) * sizeof(unsigned int [3]) );

	//	Copy the requested subdivision to the output arrays.
	//	Factor aRadius into the vertex positions.
	
	for (i = 0; i < *aNumMeshVertices; i++)
	{
		for (j = 0; j < 4; j++)
			(*someMeshVertexPositions)[i][j]	= theSphereVertexPositions[aNumSubdivisions][i][j];
		
		for (j = 0; j < 3; j++)
			(*someMeshVertexTexCoords)[i][j]	= theSphereVertexTexCoords[aNumSubdivisions][i][j];

		for (j = 0; j < 4; j++)
			(*someMeshVertexColors)[i][j]		= theSphereVertexColors[aNumSubdivisions][i][j];
	}

	for (i = 0; i < *aNumMeshFacets; i++)
	{
		for (j = 0; j < 3; j++)
			(*someMeshFacets)[i][j]	= theSphereFacets[aNumSubdivisions][i][j];
	}
}

void FreeSphereMesh(
	unsigned int	*aNumMeshVertices,
	double			(**someMeshVertexPositions)[4],
	double			(**someMeshVertexTexCoords)[3],
	double			(**someMeshVertexColors)[4],
	unsigned int	*aNumMeshFacets,
	unsigned int	(**someMeshFacets)[3])
{
	GEOMETRY_GAMES_ASSERT(
			aNumMeshVertices		!= NULL
		 && someMeshVertexPositions	!= NULL
		 && someMeshVertexTexCoords	!= NULL
		 && someMeshVertexColors	!= NULL
		 && aNumMeshFacets			!= NULL
		 && someMeshFacets			!= NULL,
		"Output pointers must not be NULL");
	
	*aNumMeshVertices = 0;
	FREE_MEMORY_SAFELY(*someMeshVertexPositions);
	FREE_MEMORY_SAFELY(*someMeshVertexTexCoords);
	FREE_MEMORY_SAFELY(*someMeshVertexColors);

	*aNumMeshFacets = 0;
	FREE_MEMORY_SAFELY(*someMeshFacets);
}

static void InitIcosahedron(
	double			aRadius,
	const double	aColor[4],	//	pre-multiplied (αR, αG, αB, α)
	unsigned int	*anIcosahedronNumVertices,
	double			anIcosahedronVertexPositions[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	double			anIcosahedronVertexTexCoords[MAX_NUM_REFINED_SPHERE_VERTICES][3],
	double			anIcosahedronVertexColors[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	unsigned int	*anIcosahedronNumFacets,
	unsigned int	anIcosahedronFacets[MAX_NUM_REFINED_SPHERE_FACETS][3])
{
	unsigned int	i,
					j;
	
	//	The icosahedron's (unnormalized) vertices sit at
	//
	//			( 0, ±1, ±φ)
	//			(±1, ±φ,  0)
	//			(±φ,  0, ±1)
	//
	//	where φ is the golden ratio.  The golden ratio is a root
	//	of the irreducible polynomial φ² - φ - 1,
	//	with numerical value φ = (1 + √5)/2 ≈ 1.6180339887…

#define GR	1.61803398874989484820
#define NF	0.52573111211913360603	//	inverse length of unnormalized vertex = 1/√(φ² + 1)
#define A	( 1.0 * NF )
#define B	( GR  * NF )

	//	The icosahedron's 12 vertices, normalized to unit length
	static const double			v[12][3] =
								{
									{0.0,  -A,  -B},
									{0.0,  +A,  -B},
									{0.0,  -A,  +B},
									{0.0,  +A,  +B},

									{ -A,  -B, 0.0},
									{ +A,  -B, 0.0},
									{ -A,  +B, 0.0},
									{ +A,  +B, 0.0},

									{ -B, 0.0,  -A},
									{ -B, 0.0,  +A},
									{ +B, 0.0,  -A},
									{ +B, 0.0,  +A}
								};

	//	The icosahedron's 20 faces
	static const unsigned int	f[20][3] =
								{
									//	Winding order is clockwise when viewed
									//	from outside the icosahedron in a left-handed coordinate system.

									//	side-based faces

									{ 0,  8,  1},
									{ 1, 10,  0},
									{ 2, 11,  3},
									{ 3,  9,  2},

									{ 4,  0,  5},
									{ 5,  2,  4},
									{ 6,  3,  7},
									{ 7,  1,  6},

									{ 8,  4,  9},
									{ 9,  6,  8},
									{10,  7, 11},
									{11,  5, 10},

									//	corner-based faces
									{ 0,  4,  8},
									{ 2,  9,  4},
									{ 1,  8,  6},
									{ 3,  6,  9},
									{ 0, 10,  5},
									{ 2,  5, 11},
									{ 1,  7, 10},
									{ 3, 11,  7}
								};

	//	vertices

	*anIcosahedronNumVertices = 12;

	for (i = 0; i < 12; i++)
	{
		for (j = 0; j < 3; j++)
			anIcosahedronVertexPositions[i][j] = aRadius * v[i][j];
		anIcosahedronVertexPositions[i][3] = 1.0;

		for (j = 0; j < 3; j++)
			anIcosahedronVertexTexCoords[i][j] = v[i][j];

		for (j = 0; j < 4; j++)
			anIcosahedronVertexColors[i][j] = aColor[j];
	}

	//	facets
	
	*anIcosahedronNumFacets = 20;

	for (i = 0; i < 20; i++)
	{
		for (j = 0; j < 3; j++)
			anIcosahedronFacets[i][j] = f[i][j];
	}
}

static void SubdivideMesh(

	double			aRadius,	//	useful for normalization
	const double	aColor[4],	//	pre-multiplied (αR, αG, αB, α)
	
	//	the source mesh (input)
	unsigned int	aSrcNumVertices,
	double			aSrcVertexPositions[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	double			aSrcVertexTexCoords[MAX_NUM_REFINED_SPHERE_VERTICES][3],
	double			aSrcVertexColors[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	unsigned int	aSrcNumFacets,
	unsigned int	aSrcFacets[MAX_NUM_REFINED_SPHERE_FACETS][3],
	
	//	the subdivided mesh (output)
	unsigned int	*aSubdivisionNumVertices,
	double			aSubdivisionVertexPositions[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	double			aSubdivisionVertexTexCoords[MAX_NUM_REFINED_SPHERE_VERTICES][3],
	double			aSubdivisionVertexColors[MAX_NUM_REFINED_SPHERE_VERTICES][4],
	unsigned int	*aSubdivisionNumFacets,
	unsigned int	aSubdivisionFacets[MAX_NUM_REFINED_SPHERE_FACETS][3])
{
	unsigned int	theVertexCount,
					i,
					j,
					k,
					(*theNewVertexTable)[MAX_NUM_REFINED_SPHERE_VERTICES],
					v0,
					v1;
	double			theLengthSquared,
					theFactor;
	unsigned int	*v,
					vv[3];

	GEOMETRY_GAMES_ASSERT(
		aSrcNumVertices <= MAX_NUM_REFINED_SPHERE_VERTICES,
		"too many source vertices");
	GEOMETRY_GAMES_ASSERT(
		aSrcNumFacets   <= MAX_NUM_REFINED_SPHERE_FACETS,
		"too many source facets"  );

	//	We'll subdivide the mesh, replacing each old facet with four new ones.
	//
	//		  /\		//
	//		 /__\		//
	//		/_\/_\		//
	//
	*aSubdivisionNumFacets = 4 * aSrcNumFacets;

	//	Each facet sees three vertices, and -- except for the twelve original vertices --
	//	each vertex gets seen six times.  To compensate for the twelve original vertices,
	//	each of which gets seen only five times, but must add in a correction factor
	//	of 12*(6 - 5) = 12 "missing" vertex sightings.
	*aSubdivisionNumVertices = (3*(*aSubdivisionNumFacets) + 12*(6 - 5)) / 6;

	GEOMETRY_GAMES_ASSERT(
		*aSubdivisionNumVertices <= MAX_NUM_REFINED_SPHERE_VERTICES,
		"too many subdivision vertices");
	GEOMETRY_GAMES_ASSERT(
		*aSubdivisionNumFacets   <= MAX_NUM_REFINED_SPHERE_FACETS,
		"too many subdivision facets"  );

	
	//	First copy the source mesh vertices' positions and texture coordinates
	//	to the destination mesh...
	for (i = 0; i < aSrcNumVertices; i++)
	{
		for (j = 0; j < 4; j++)
			aSubdivisionVertexPositions[i][j]	= aSrcVertexPositions[i][j];

		for (j = 0; j < 3; j++)
			aSubdivisionVertexTexCoords[i][j]	= aSrcVertexTexCoords[i][j];

		for (j = 0; j < 4; j++)
			aSubdivisionVertexColors[i][j]		= aColor[j];
	}
	theVertexCount = aSrcNumVertices;

	//	...and then create one new vertex on each edge.
	//
	//	Use theNewVertexTable[][] to index them,
	//	so two facets sharing an edge can share the same vertex.
	//
	//	theNewVertexTable[v0][v1] takes the indices v0 and v1 of two old vertices,
	//	and gives the index of the new vertex that sits at the midpoint of the edge
	//	that connects v0 and v1.
	//
	//	The size of theNewVertexTable grows as the square of the number of source vertices.
	//	For modest meshes this won't be a problem.  For larger meshes a fancier algorithm,
	//	with linear rather than quadratic memory demands, could be used.
	//
	theNewVertexTable = (unsigned int (*)[MAX_NUM_REFINED_SPHERE_VERTICES])
			malloc( aSrcNumVertices * sizeof(unsigned int [MAX_NUM_REFINED_SPHERE_VERTICES]) );
	GEOMETRY_GAMES_ASSERT(
		theNewVertexTable != NULL,
		"failed to allocate memory for theNewVertexTable");

	//	Initialize theNewVertexTable[][] to all 0xFFFF,
	//	to indicate that no new vertices have yet been created.
	for (i = 0; i < aSrcNumVertices; i++)
		for (j = 0; j < aSrcNumVertices; j++)
			theNewVertexTable[i][j] = 0xFFFF;

	//	For each edge in the source mesh, create a new vertex at its midpoint.
	for (i = 0; i < aSrcNumFacets; i++)
	{
		for (j = 0; j < 3; j++)
		{
			v0 = aSrcFacets[i][   j   ];
			v1 = aSrcFacets[i][(j+1)%3];

			if (theNewVertexTable[v0][v1] == 0xFFFF)
			{
				GEOMETRY_GAMES_ASSERT(
					theVertexCount < *aSubdivisionNumVertices,
					"Internal error #1 in SubdivideMesh()");
				
				//	The new vertex will be midway between vertices v0 and v1.
				
				//	First compute the new vertex's cube map texture coordinates,
				//	which sit on the unit sphere.
				for (k = 0; k < 3; k++)
				{
					aSubdivisionVertexTexCoords[theVertexCount][k]
						= 0.5 * (aSubdivisionVertexTexCoords[v0][k] + aSubdivisionVertexTexCoords[v1][k]);
				}
				theLengthSquared = 0.0;
				for (k = 0; k < 3; k++)
				{
					theLengthSquared += aSubdivisionVertexTexCoords[theVertexCount][k]
									  * aSubdivisionVertexTexCoords[theVertexCount][k];
				}
				GEOMETRY_GAMES_ASSERT(
					theLengthSquared >= 0.5,
					"Impossibly short interpolated cube map texture coordinates");
				theFactor = 1.0 / sqrt(theLengthSquared);
				for (k = 0; k < 3; k++)
				{
					aSubdivisionVertexTexCoords[theVertexCount][k] *= theFactor;
				}

				//	The new vertex's position is simply
				//	aRadius times its cube map texture coordinates,
				//	with a fourth coordinate of 1.0 appended.
				for (k = 0; k < 3; k++)
				{
					aSubdivisionVertexPositions[theVertexCount][k]
						= aRadius
						* aSubdivisionVertexTexCoords[theVertexCount][k];
				}
				aSubdivisionVertexPositions[theVertexCount][3] = 1.0;

				//	Assign the constant color.
				for (k = 0; k < 4; k++)
				{
					aSubdivisionVertexColors[theVertexCount][k] = aColor[k];
				}

				//	Record the new vertex at [v1][v0] as well as [v0][v1],
				//	so only one new vertex will get created for each edge.
				theNewVertexTable[v0][v1] = theVertexCount;
				theNewVertexTable[v1][v0] = theVertexCount;

				theVertexCount++;
			}
		}
	}
	GEOMETRY_GAMES_ASSERT(
		theVertexCount == *aSubdivisionNumVertices,
		"Internal error #2 in SubdivideMesh()");

	//	Four each facet in the source mesh,
	//	create four smaller facets in the subdivision.
	for (i = 0; i < aSrcNumFacets; i++)
	{
		//	The old vertices incident to this facet will be v[0], v[1] and v[2].
		v = aSrcFacets[i];

		//	The new vertices -- which sit at the midpoints of the old edges --
		//	will be vv[0], vv[1], and vv[2].
		//	Each vv[j] sits opposite the corresponding v[j].
		for (j = 0; j < 3; j++)
			vv[j] = theNewVertexTable[ v[(j+1)%3] ][ v[(j+2)%3] ];

		//	Create the new facets.

		aSubdivisionFacets[4*i + 0][0] = vv[0];
		aSubdivisionFacets[4*i + 0][1] = vv[1];
		aSubdivisionFacets[4*i + 0][2] = vv[2];

		aSubdivisionFacets[4*i + 1][0] = v[0];
		aSubdivisionFacets[4*i + 1][1] = vv[2];
		aSubdivisionFacets[4*i + 1][2] = vv[1];

		aSubdivisionFacets[4*i + 2][0] = v[1];
		aSubdivisionFacets[4*i + 2][1] = vv[0];
		aSubdivisionFacets[4*i + 2][2] = vv[2];

		aSubdivisionFacets[4*i + 3][0] = v[2];
		aSubdivisionFacets[4*i + 3][1] = vv[1];
		aSubdivisionFacets[4*i + 3][2] = vv[0];
	}
	
	free(theNewVertexTable);
}

