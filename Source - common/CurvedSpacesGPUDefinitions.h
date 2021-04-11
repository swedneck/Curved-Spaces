//	CurvedSpacesGPUDefinitions.h
//
//	© 2021 by Jeff Weeks
//	See TermsOfUse.txt

#ifndef CurvedSpacesGPUDefinitions_h
#define CurvedSpacesGPUDefinitions_h

#include <simd/simd.h>


enum
{
	VertexAttributePosition		= 0,
	VertexAttributeTexCoords	= 1,
	VertexAttributeColor		= 2
};

enum
{
	BufferIndexVertexAttributes	= 0,
	BufferIndexTilingGroup		= 1,
	BufferIndexUniforms			= 2
};

enum
{
	TextureIndexPrimary	= 0
};
enum
{
	SamplerIndexPrimary	= 0
};


typedef struct
{
	simd_float4x4	itsProjectionMatrixForBoxFull,
					itsProjectionMatrixForBoxFront,
					itsProjectionMatrixForBoxBack;

	__fp16	itsSphFogSaturationNear,					//	fog saturation at distance  0  (the observer)
			itsSphFogSaturationMid,						//	fog saturation at distance  π  (the antipode)
			itsSphFogSaturationFar,						//	fog saturation at distance 2π  (back at the observer again)
			itsEucFogInverseSquareSaturationDistance,	//	1 / (max_r)²
			itsHypFogInverseLogCoshSaturationDistance;	//	1 / log(cosh(max_r))

} CurvedSpacesUniformData;

//	For the most part the same GPU vertex function works for all three geometries
//	(spherical, Euclidean and hyperbolic).  The exceptions are
//
//		- a different fog formula in each geometry
//
//		- special projection matrices for use
//			in spherical spaces that lack antipodal symmetry
//			(namely odd-order lens spaces and the 3-sphere itself), to map
//			the  back hemisphere to the back  half of the clipping box, and
//			the front hemisphere to the front half of the clipping box.
//
typedef enum
{
	ShaderSphericalFogBoxFull,
	ShaderSphericalFogBoxFront,
	ShaderSphericalFogBoxBack,
	ShaderEuclideanFogBoxFull,
	ShaderHyperbolicFogBoxFull,
	ShaderNoFogBoxFull,
	ShaderNoFogBoxFront,
	ShaderNoFogBoxBack
} ShaderFogAndClipBoxType;

#endif	//	CurvedSpacesGPUDefinitions_h
