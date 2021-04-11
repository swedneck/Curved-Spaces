//	CurvedSpacesCube.c
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#include "CurvedSpaces-Common.h"
#include "GeometryGamesUtilities-Common.h"

#ifdef SHAPE_OF_SPACE_CH_7

#define HW	0.1	//	cube's half width

//	What colors should the Cube be?
//
//		Note #1:  These colors are chosen so that
//		of the four cubes nearest to the obsever in Fig 7.5 (and others),
//		the cube at the lower-left has the same face colors
//		as the single cube in Fig 7.6.
//
//		Note #2:  These are the colors used to create the figures
//		for the third edition of The Shape of Space.
//		I'm pretty sure (but not 100% sure) I rendered those images
//		on my MacBook using my display's color space, which is
//		fairly close to Display P3.
//		If I ever need to recreate those figures in the future,
//		I should use my own "Color Calculator.xcodeproj" to convert
//		the following linear presumably-DisplayP3 color values
//		to linear extended-range sRGB, and then render the figures
//		on an iPhone or an Apple Silicon Mac.  For example,
//		the shade of red given below would map from
//
//				P3(1.0000, 0.0000, 0.2500)
//			to
//				XRsRGB(1.2249, -0.0421,  0.2549)
//
#define COLOR_X_MINUS	PREMULTIPLY_RGBA(0.2500, 1.0000, 0.5000, 1.0000)	//	green
#define COLOR_X_PLUS	PREMULTIPLY_RGBA(1.0000, 0.0000, 0.2500, 1.0000)	//	red
#define COLOR_Y_MINUS	PREMULTIPLY_RGBA(0.7500, 0.5000, 1.0000, 1.0000)	//	violet
#define COLOR_Y_PLUS	PREMULTIPLY_RGBA(1.0000, 1.0000, 0.2500, 1.0000)	//	yellow
#define COLOR_Z_MINUS	PREMULTIPLY_RGBA(0.5000, 0.7500, 1.0000, 1.0000)	//	blue
#define COLOR_Z_PLUS	PREMULTIPLY_RGBA(1.0000, 0.5000, 0.0000, 1.0000)	//	orange


void MakeCubeMesh(
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
				col[4];	//	pre-multiplied (αR, αG, αB, α)
	}
	v[6*4] =
	{
		//	x = -HW
		
		{{-HW, -HW, -HW, 1.0}, COLOR_X_MINUS},
		{{-HW, -HW, +HW, 1.0}, COLOR_X_MINUS},
		{{-HW, +HW, -HW, 1.0}, COLOR_X_MINUS},
		{{-HW, +HW, +HW, 1.0}, COLOR_X_MINUS},

		//	x = +HW
		
		{{+HW, -HW, -HW, 1.0}, COLOR_X_PLUS},
		{{+HW, +HW, -HW, 1.0}, COLOR_X_PLUS},
		{{+HW, -HW, +HW, 1.0}, COLOR_X_PLUS},
		{{+HW, +HW, +HW, 1.0}, COLOR_X_PLUS},

		//	y = -HW
		
		{{-HW, -HW, -HW, 1.0}, COLOR_Y_MINUS},
		{{+HW, -HW, -HW, 1.0}, COLOR_Y_MINUS},
		{{-HW, -HW, +HW, 1.0}, COLOR_Y_MINUS},
		{{+HW, -HW, +HW, 1.0}, COLOR_Y_MINUS},

		//	y = +HW
		
		{{-HW, +HW, -HW, 1.0}, COLOR_Y_PLUS},
		{{-HW, +HW, +HW, 1.0}, COLOR_Y_PLUS},
		{{+HW, +HW, -HW, 1.0}, COLOR_Y_PLUS},
		{{+HW, +HW, +HW, 1.0}, COLOR_Y_PLUS},

		//	z = -HW
		
		{{-HW, -HW, -HW, 1.0}, COLOR_Z_MINUS},
		{{-HW, +HW, -HW, 1.0}, COLOR_Z_MINUS},
		{{+HW, -HW, -HW, 1.0}, COLOR_Z_MINUS},
		{{+HW, +HW, -HW, 1.0}, COLOR_Z_MINUS},

		//	z = +HW
		
		{{-HW, -HW, +HW, 1.0}, COLOR_Z_PLUS},
		{{+HW, -HW, +HW, 1.0}, COLOR_Z_PLUS},
		{{-HW, +HW, +HW, 1.0}, COLOR_Z_PLUS},
		{{+HW, +HW, +HW, 1.0}, COLOR_Z_PLUS},

	};

	static const unsigned int	f[6*2][3] =
	{
		//	x = -HW
		{  0,  1,  2 },
		{  2,  1,  3 },
		
		//	x = +HW
		{  4,  5,  6 },
		{  6,  5,  7 },

		//	y = -HW
		{  8,  9, 10 },
		{ 10,  9, 11 },
		
		//	y = +HW
		{ 12, 13, 14 },
		{ 14, 13, 15 },

		//	z = -HW
		{ 16, 17, 18 },
		{ 18, 17, 19 },
		
		//	z = +HW
		{ 20, 21, 22 },
		{ 22, 21, 23 }
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
	//	The caller will take responsibility for calling FreeCubeMesh()
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
		
		//	Set (u,v,w) cubemap texture coordinates, even though
		//	the Cube centerpiece doesn't use any texture at all.
		for (j = 0; j < 3; j++)
			(*someMeshVertexTexCoords)[i][j]	= v[i].pos[j];	//	Use position as texture coordinates.

		for (j = 0; j < 4; j++)
			(*someMeshVertexColors)[i][j]		= v[i].col[j];
	}

	for (i = 0; i < *aNumMeshFacets; i++)
	{
		for (j = 0; j < 3; j++)
			(*someMeshFacets)[i][j]	= f[i][j];
	}
}

void FreeCubeMesh(
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

#endif	//	SHAPE_OF_SPACE_CH_7

