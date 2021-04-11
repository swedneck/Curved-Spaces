//	CurvedSpacesGyroscope.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include "GeometryGamesUtilities-Common.h"


//	How big should the ring of arrows be?
#define OUTER_RADIUS	0.050
#define OUTER_HEIGHT	0.025

//	How large should the central axle be?
#define INNER_RADIUS	0.017
#define INNER_HEIGHT	0.100

//	What texture coordinates will roughly respect the triangles' proportions?

#define TEX_ARROW_HALF_WIDTH	(OUTER_HEIGHT / OUTER_RADIUS)		//	assuming arrow length is roughly
																	//		(outer circumference)/6 ≈ OUTER_RADIUS
#define TEX_ARROW_SIDE_B		(0.5 - TEX_ARROW_HALF_WIDTH)
#define TEX_ARROW_SIDE_A		(0.5 + TEX_ARROW_HALF_WIDTH)

#define TEX_AXLE_HALF_WIDTH		(0.5 * INNER_RADIUS / INNER_HEIGHT)	//	assuming triangle width is roughly
																	//		(inner circumference)/6 ≈ INNER_RADIUS
#define TEX_AXLE_SIDE_A			(0.5 - TEX_AXLE_HALF_WIDTH)
#define TEX_AXLE_SIDE_B			(0.5 + TEX_AXLE_HALF_WIDTH)

//	What colors should the gyroscope be?
#define COLOR_ARROW_OUTER	PREMULTIPLY_RGBA(0.1656,  0.6408,  1.0442, 1.0000)	//	= P3(1/4, 5/8,   1 )
#define COLOR_ARROW_INNER	PREMULTIPLY_RGBA(0.1109,  0.1901,  0.2574, 1.0000)	//	= P3(1/8, 3/16, 1/4)
#define COLOR_AXLE_BOTTOM	PREMULTIPLY_RGBA(1.2249, -0.0421, -0.0196, 1.0000)	//	= P3( 1,   0,    0 )
#define COLOR_AXLE_TOP		PREMULTIPLY_RGBA(1.0000,  1.0000,  1.0000, 1.0000)	//	= P3( 1,   1,    1 )

//	For convenience, predefine cos(2πk/n) and sin(2πk/n).

#define ROOT_3_OVER_2	 0.86602540378443864676

#define COS0	 1.0
#define SIN0	 0.0

#define COS1	 0.5
#define SIN1	 ROOT_3_OVER_2

#define COS2	-0.5
#define SIN2	 ROOT_3_OVER_2

#define COS3	-1.0
#define SIN3	 0.0

#define COS4	-0.5
#define SIN4	-ROOT_3_OVER_2

#define COS5	 0.5
#define SIN5	-ROOT_3_OVER_2


void MakeGyroscopeMesh(
	unsigned int	*aNumMeshVertices,				//	output
	double			(**someMeshVertexPositions)[4],	//	output
	double			(**someMeshVertexTexCoords)[3],	//	output
	double			(**someMeshVertexColors)[4],	//	output;	pre-multiplied (αR, αG, αB, α)
	unsigned int	*aNumMeshFacets,				//	output
	unsigned int	(**someMeshFacets)[3])			//	output
{
	unsigned int	i,
					j;

	static const struct
	{
		double	pos[4],
				tex[3],	//	(u,v,0) -- last component is unused for non-cubemap texture
				col[4];	//	pre-multiplied (αR, αG, αB, α)
	}
	v[2*6*3 + 2*(6+1)] =
	{
		//	arrows, outer surface

		{{OUTER_RADIUS*COS1, OUTER_RADIUS*SIN1, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS1, OUTER_RADIUS*SIN1, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS0, OUTER_RADIUS*SIN0,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_OUTER},

		{{OUTER_RADIUS*COS2, OUTER_RADIUS*SIN2, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS2, OUTER_RADIUS*SIN2, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS1, OUTER_RADIUS*SIN1,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_OUTER},

		{{OUTER_RADIUS*COS3, OUTER_RADIUS*SIN3, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS3, OUTER_RADIUS*SIN3, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS2, OUTER_RADIUS*SIN2,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_OUTER},

		{{OUTER_RADIUS*COS4, OUTER_RADIUS*SIN4, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS4, OUTER_RADIUS*SIN4, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS3, OUTER_RADIUS*SIN3,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_OUTER},

		{{OUTER_RADIUS*COS5, OUTER_RADIUS*SIN5, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS5, OUTER_RADIUS*SIN5, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS4, OUTER_RADIUS*SIN4,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_OUTER},

		{{OUTER_RADIUS*COS0, OUTER_RADIUS*SIN0, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS0, OUTER_RADIUS*SIN0, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_OUTER},
		{{OUTER_RADIUS*COS5, OUTER_RADIUS*SIN5,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_OUTER},

		//	arrows, inner surface

		{{OUTER_RADIUS*COS1, OUTER_RADIUS*SIN1, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS1, OUTER_RADIUS*SIN1, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS0, OUTER_RADIUS*SIN0,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_INNER},

		{{OUTER_RADIUS*COS2, OUTER_RADIUS*SIN2, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS2, OUTER_RADIUS*SIN2, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS1, OUTER_RADIUS*SIN1,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_INNER},

		{{OUTER_RADIUS*COS3, OUTER_RADIUS*SIN3, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS3, OUTER_RADIUS*SIN3, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS2, OUTER_RADIUS*SIN2,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_INNER},

		{{OUTER_RADIUS*COS4, OUTER_RADIUS*SIN4, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS4, OUTER_RADIUS*SIN4, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS3, OUTER_RADIUS*SIN3,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_INNER},

		{{OUTER_RADIUS*COS5, OUTER_RADIUS*SIN5, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS5, OUTER_RADIUS*SIN5, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS4, OUTER_RADIUS*SIN4,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_INNER},

		{{OUTER_RADIUS*COS0, OUTER_RADIUS*SIN0, +OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_B, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS0, OUTER_RADIUS*SIN0, -OUTER_HEIGHT, 1.0}, {TEX_ARROW_SIDE_A, 0.0, 0.0}, COLOR_ARROW_INNER},
		{{OUTER_RADIUS*COS5, OUTER_RADIUS*SIN5,  0.0,          1.0}, {      0.5,        1.0, 0.0}, COLOR_ARROW_INNER},

		//	top half of axle (red)
		{{INNER_RADIUS*COS0, INNER_RADIUS*SIN0, 0.0,          1.0}, {TEX_AXLE_SIDE_A, 0.0, 0.0}, COLOR_AXLE_BOTTOM},
		{{INNER_RADIUS*COS1, INNER_RADIUS*SIN1, 0.0,          1.0}, {TEX_AXLE_SIDE_B, 0.0, 0.0}, COLOR_AXLE_BOTTOM},
		{{INNER_RADIUS*COS2, INNER_RADIUS*SIN2, 0.0,          1.0}, {TEX_AXLE_SIDE_A, 0.0, 0.0}, COLOR_AXLE_BOTTOM},
		{{INNER_RADIUS*COS3, INNER_RADIUS*SIN3, 0.0,          1.0}, {TEX_AXLE_SIDE_B, 0.0, 0.0}, COLOR_AXLE_BOTTOM},
		{{INNER_RADIUS*COS4, INNER_RADIUS*SIN4, 0.0,          1.0}, {TEX_AXLE_SIDE_A, 0.0, 0.0}, COLOR_AXLE_BOTTOM},
		{{INNER_RADIUS*COS5, INNER_RADIUS*SIN5, 0.0,          1.0}, {TEX_AXLE_SIDE_B, 0.0, 0.0}, COLOR_AXLE_BOTTOM},
		{{0.0,               0.0,              +INNER_HEIGHT, 1.0}, {      0.5,       1.0, 0.0}, COLOR_AXLE_BOTTOM},

		//	bottom half of axle (white)
		{{INNER_RADIUS*COS0, INNER_RADIUS*SIN0, 0.0,          1.0}, {TEX_AXLE_SIDE_A, 0.0, 0.0}, COLOR_AXLE_TOP},
		{{INNER_RADIUS*COS1, INNER_RADIUS*SIN1, 0.0,          1.0}, {TEX_AXLE_SIDE_B, 0.0, 0.0}, COLOR_AXLE_TOP},
		{{INNER_RADIUS*COS2, INNER_RADIUS*SIN2, 0.0,          1.0}, {TEX_AXLE_SIDE_A, 0.0, 0.0}, COLOR_AXLE_TOP},
		{{INNER_RADIUS*COS3, INNER_RADIUS*SIN3, 0.0,          1.0}, {TEX_AXLE_SIDE_B, 0.0, 0.0}, COLOR_AXLE_TOP},
		{{INNER_RADIUS*COS4, INNER_RADIUS*SIN4, 0.0,          1.0}, {TEX_AXLE_SIDE_A, 0.0, 0.0}, COLOR_AXLE_TOP},
		{{INNER_RADIUS*COS5, INNER_RADIUS*SIN5, 0.0,          1.0}, {TEX_AXLE_SIDE_B, 0.0, 0.0}, COLOR_AXLE_TOP},
		{{0.0,               0.0,              -INNER_HEIGHT, 1.0}, {      0.5,       1.0, 0.0}, COLOR_AXLE_TOP}
	};

	static const unsigned int	f[2*6 + 2*6][3] =
	{
		//	arrows, outer surface
		{ 0,  1,  2},
		{ 3,  4,  5},
		{ 6,  7,  8},
		{ 9, 10, 11},
		{12, 13, 14},
		{15, 16, 17},

		//	arrows, inner surface
		{18, 19, 20},
		{21, 22, 23},
		{24, 25, 26},
		{27, 28, 29},
		{30, 31, 32},
		{33, 34, 35},

		//	bottom half of axle (red)
		{36, 37, 42},
		{37, 38, 42},
		{38, 39, 42},
		{39, 40, 42},
		{40, 41, 42},
		{41, 36, 42},

		//	top half of axle (white)
		{44, 43, 49},
		{45, 44, 49},
		{46, 45, 49},
		{47, 46, 49},
		{48, 47, 49},
		{43, 48, 49}
	};


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

	//	Set the number of vertices and facets.
	*aNumMeshVertices	= BUFFER_LENGTH(v);
	*aNumMeshFacets		= BUFFER_LENGTH(f);

	//	Allocate the output arrays.
	//	The caller will take responsibility for calling FreeGyroscopeMesh()
	//	to free these arrays when they're no longer needed.
	*someMeshVertexPositions	= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshVertexTexCoords	= (double (*)[3]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [3]) );
	*someMeshVertexColors		= (double (*)[4]) GET_MEMORY( (*aNumMeshVertices) * sizeof(double [4]) );
	*someMeshFacets				= (unsigned int (*)[3]) GET_MEMORY( (*aNumMeshFacets) * sizeof(unsigned int [3]) );

	//	Copy the requested mesh data to the output arrays.
	
	for (i = 0; i < *aNumMeshVertices; i++)
	{
		for (j = 0; j < 4; j++)
			(*someMeshVertexPositions)[i][j]	= v[i].pos[j];
		
		for (j = 0; j < 3; j++)
			(*someMeshVertexTexCoords)[i][j]	= v[i].tex[j];

		for (j = 0; j < 4; j++)
			(*someMeshVertexColors)[i][j]		= v[i].col[j];
	}

	for (i = 0; i < *aNumMeshFacets; i++)
	{
		for (j = 0; j < 3; j++)
			(*someMeshFacets)[i][j]	= f[i][j];
	}
}

void FreeGyroscopeMesh(
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
